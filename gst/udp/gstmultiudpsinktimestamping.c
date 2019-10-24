#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmultiudpsinktimestamping.h"
#include "gstmultiudpsink.h"
#include <stdio.h>
#include <sys/socket.h>

//Max number of control messages we will read at a time.
#define CONTROL_MSG_MAX_MESSAGES 16
#define GST_MULTIUDPSINK_TIMESTAMPING_NAME_MAXLEN 16
#define GST_MULTIUDPSINK_TIMESTAMPING_NAME_PREFIX "ts_recv"

static void gst_multiudpsink_timestamping_finalize (GObject * object);
static gpointer gst_multiudpsink_timestamping_receiver_thread(gpointer data);
static void gst_multiudpsink_timestamping_reader(GstMultiUDPSink * sink, GSocket * socket);
static void gst_multiudpsink_timestamping_parser(GstMultiUDPSink * sink, GSocket * socket, GInputMessage * message);

#define gst_multiudpsink_timestamping_parent_class parent_class
G_DEFINE_TYPE (GstMultiUDPSinkTimestamping, gst_multiudpsink_timestamping, G_TYPE_OBJECT);

struct _gst_multiudpsink_timestamping_priv_data {
  GstMultiUDPSink * sink;
  GSocket * socket;
};

static void
gst_multiudpsink_timestamping_class_init (GstMultiUDPSinkTimestampingClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gst_multiudpsink_timestamping_finalize;
  return;
}

static void
gst_multiudpsink_timestamping_init (GstMultiUDPSinkTimestamping * self)
{
  self->thread = NULL;
  self->shutdown = g_cancellable_new();
}

static void
gst_multiudpsink_timestamping_finalize (GObject * object)
{
  GstMultiUDPSinkTimestamping * handle = GST_MULTIUDPSINK_TIMESTAMPING(object);
  g_assert(handle->thread);
  g_assert(handle->shutdown);

  //Signal thread stop
  g_cancellable_cancel(handle->shutdown);

  //Wait for thread to exit
  g_thread_join(handle->thread);
  g_thread_unref(handle->thread);
  g_object_unref(handle->shutdown);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gpointer
gst_multiudpsink_timestamping_receiver_thread(gpointer data){
  GError * error = NULL;
  struct _gst_multiudpsink_timestamping_priv_data * priv_data = (struct _gst_multiudpsink_timestamping_priv_data *)data;
  g_assert(priv_data);

  while (g_cancellable_is_cancelled(priv_data->sink->timestamping->shutdown) == FALSE){
    if (g_socket_condition_wait (priv_data->socket, G_IO_ERR, priv_data->sink->timestamping->shutdown, &error)){
      if (g_socket_condition_check(priv_data->socket, G_IO_ERR) & G_IO_ERR){
          gst_multiudpsink_timestamping_reader(priv_data->sink, priv_data->socket);
      }
    }
  }
  g_free(data);
  return NULL;
}

static void
gst_multiudpsink_timestamping_reader(GstMultiUDPSink * sink, GSocket * socket){
    gint ret;
    GError * error = NULL;
    GInputMessage message[CONTROL_MSG_MAX_MESSAGES];
    GSocketControlMessage ** control_messages[CONTROL_MSG_MAX_MESSAGES];
    guint num_control_messages[CONTROL_MSG_MAX_MESSAGES];

    memset(control_messages, 0, sizeof(control_messages));
    memset(num_control_messages, 0, sizeof(num_control_messages));

    for (int i=0; i < CONTROL_MSG_MAX_MESSAGES; i++){
        message[i].address = NULL;
        message[i].vectors = NULL;
        message[i].num_vectors = 0;
        message[i].bytes_received = 0;
        message[i].flags = 0;
        message[i].control_messages = &control_messages[i];
        message[i].num_control_messages = &num_control_messages[i];
    }

    ret = g_socket_receive_messages(socket, message, CONTROL_MSG_MAX_MESSAGES, MSG_ERRQUEUE, NULL, &error);
    if (ret == -1){
        if (error->code == G_IO_ERROR_WOULD_BLOCK) {
          g_warning("receiver: g_socket_receive_messages (IO_ERR) unexpectently returned G_IO_ERROR_WOULD_BLOCK: %s\n", error->message);
        } else {
          g_error("receiver: g_socket_receive_messages (IO_ERR): %s\n", error->message);
        }
        return;
    }

    for (int i=0; i < ret; i++){
      gst_multiudpsink_timestamping_parser(sink, socket, &message[i]);
      for (int j=0; j < num_control_messages[i]; j++){
          g_object_unref(control_messages[i][j]);
      }
      g_free(control_messages[i]);
    }
}

static void
gst_multiudpsink_timestamping_parser(GstMultiUDPSink * sink, GSocket * socket, GInputMessage * message){
  GUnixTimestampingMessageUnified unified = {0};
  if (*message->num_control_messages != 2) {
    g_error("Expected number of control messages returned to be 2, but got %d!\n", *message->num_control_messages);
    return;
  }

  if ((g_unix_timestamping_unify_control_message_set(*message->control_messages, &unified)) == -1){
    g_error("Failed to unify control message set!\n");
    return;
  }

  g_debug("Type:%u TypeName:%s Source:%u SourceName:%s PacketID: %u TS: %ld.%09ld\n", unified.timestamping_type, g_unix_timestamping_get_timestamping_type_name(unified.timestamping_type),
    unified.timestamping_source, g_unix_timestamping_get_timestamping_source_name(unified.timestamping_source), unified.packet_id, unified.timestamping_sec, unified.timestamping_nsec); 

  GstClockTime ts = (((GstClockTime)unified.timestamping_sec * GST_SECOND) + unified.timestamping_nsec);
  GstStructure * st = gst_structure_new("GstMultiUDPTimestamping",
    "type", G_TYPE_UINT, unified.timestamping_type,
    "source", G_TYPE_UINT, unified.timestamping_source,
    "packetid", G_TYPE_UINT, unified.packet_id,
    "timestamp", GST_TYPE_CLOCK_TIME, ts,
    NULL );
  GstEvent * ev = gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, st);
  g_assert(gst_pad_send_event(sink, ev) == TRUE);
}

GstMultiUDPSinkTimestamping *
gst_multiudpsink_timestamping_new(GstMultiUDPSink * sink){
  gchar thread_name[GST_MULTIUDPSINK_TIMESTAMPING_NAME_MAXLEN+1] = {0};
  struct _gst_multiudpsink_timestamping_priv_data * priv_data = NULL;
  GstMultiUDPSinkTimestamping * handle = NULL; 

  g_assert(sink);
  g_assert(sink->control_msg_receiver == NULL);
  g_assert(sink->used_socket || sink->used_socket_v6);

  priv_data = g_malloc0(sizeof(struct _gst_multiudpsink_timestamping_priv_data));
  g_assert(priv_data);
  priv_data->sink = sink;
  priv_data->socket = (sink->used_socket) ? sink->used_socket : sink->used_socket_v6;  
  snprintf(thread_name, GST_MULTIUDPSINK_TIMESTAMPING_NAME_MAXLEN, "%s_sd_%d", GST_MULTIUDPSINK_TIMESTAMPING_NAME_PREFIX, g_socket_get_fd(priv_data->socket));

  handle = g_object_new (GST_TYPE_MULTIUDPSINK_TIMESTAMPING, NULL);
  g_assert(handle);
  handle->thread = g_thread_new(thread_name, gst_multiudpsink_timestamping_receiver_thread, priv_data);

  return handle;
}

gboolean
gst_multiudpsink_timestamping_get_socket_enabled(GSocket * gsocket)
{
  g_assert(gsocket);
  return g_unix_timestamping_get_socket_enabled(gsocket);
}