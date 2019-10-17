#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstmultiudpsink.h"
#include "gstmultiudpsinktimestamping.h"

static gpointer gst_multiudpsink_timestamping_v4_receiver_thread(gpointer data);
static gpointer gst_multiudpsink_timestamping_v6_receiver_thread(gpointer data);
static void gst_multiudpsink_timestamping_reader(GstMultiUDPSink * sink, GSocket * socket);
static void gst_multiudpsink_timestamping_parser(GstMultiUDPSink * sink, GSocket * socket, GInputMessage * message);


static void 
gst_multiudpsink_timestamping_receiver_start(GstMultiUDPSink * sink){
  return;
}


static gpointer
gst_multiudpsink_timestamping_v4_receiver_thread(gpointer data){
  GError * error = NULL;
  GstMultiUDPSink * sink = GST_MULTIUDPSINK(data);

  while (sink->control_msg_receiver_stop == FALSE){
    if (g_socket_condition_timed_wait (sink->used_socket, G_IO_IN, 100000, NULL, &error)){
      if (g_socket_condition_check(sink->used_socket, G_IO_IN) & G_IO_ERR){
          gst_multiudpsink_timestamping_reader(sink, sink->used_socket);
      }
    }    
  }
  return NULL;
}

static gpointer
gst_multiudpsink_timestamping_v6_receiver_thread(gpointer data){
  GError * error = NULL;
  GstMultiUDPSink * sink = GST_MULTIUDPSINK(data);

  while (sink->control_msg_receiver_stop == FALSE){
    if (g_socket_condition_timed_wait (sink->used_socket_v6, G_IO_IN, 100000, NULL, &error)){
      if (g_socket_condition_check(sink->used_socket_v6, G_IO_IN) & G_IO_ERR){
          gst_multiudpsink_timestamping_reader(sink, sink->used_socket_v6);
      }
    }
  }
  return NULL;
}

//Max number of control messages we will read at a time.
#define CONTROL_MSG_MAX_MESSAGES 16

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
  if (*message->num_control_messages != 2) {
    g_error("Expected number of control messages returned to be 2, but got %d!\n", *message->num_control_messages);
    return;
  }

  GUnixTimestampingMessageUnified unified = {0};
  if ((g_unix_timestamping_unify_control_message_set(*message->control_messages, &unified)) == -1){
    g_error("Failed to unify control message set!\n");
    return;
  }

  printf("Type:%u TypeName:%s Source:%u SourceName:%s PacketID: %u TS: %ld.%09ld\n", unified.timestamping_type, g_unix_timestamping_get_timestamping_type_name(unified.timestamping_type),
    unified.timestamping_source, g_unix_timestamping_get_timestamping_source_name(unified.timestamping_source), unified.packet_id, unified.timestamping_sec, unified.timestamping_nsec);  
}

