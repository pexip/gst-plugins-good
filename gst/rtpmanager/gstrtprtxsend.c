/* RTP Retransmission sender element for GStreamer
 *
 * gstrtprtxsend.c:
 *
 * Copyright (C) 2013 Collabora Ltd.
 *   @author Julien Isorce <julien.isorce@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-rtprtxsend
 *
 * See #GstRtpRtxReceive for examples
 *
 * The purpose of the sender RTX object is to keep a history of RTP packets up
 * to a configurable limit (max-size-time or max-size-packets). It will listen
 * for upstream custom retransmission events (GstRTPRetransmissionRequest) that
 * comes from downstream (#GstRtpSession). When receiving a request it will
 * look up the requested seqnum in its list of stored packets. If the packet
 * is available, it will create a RTX packet according to RFC 4588 and send
 * this as an auxiliary stream. RTX is SSRC-multiplexed
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <string.h>
#include <stdlib.h>

#include "gstrtprtxsend.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtp_rtx_send_debug);
#define GST_CAT_DEFAULT gst_rtp_rtx_send_debug

#define UNLIMITED_KBPS (-1)

#define DEFAULT_RTX_PAYLOAD_TYPE 0
#define DEFAULT_MAX_SIZE_TIME    0
#define DEFAULT_MAX_SIZE_PACKETS 100
#define DEFAULT_MAX_KBPS         UNLIMITED_KBPS
#define DEFAULT_MAX_BUCKET_SIZE  UNLIMITED_KBPS

enum
{
  PROP_0,
  PROP_SSRC_MAP,
  PROP_PAYLOAD_TYPE_MAP,
  PROP_MAX_SIZE_TIME,
  PROP_MAX_SIZE_PACKETS,
  PROP_NUM_RTX_REQUESTS,
  PROP_NUM_RTX_PACKETS,
  PROP_MAX_KBPS,
  PROP_MAX_BUCKET_SIZE
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static gboolean gst_rtp_rtx_send_queue_check_full (GstDataQueue * queue,
    guint visible, guint bytes, guint64 time, gpointer checkdata);

static gboolean gst_rtp_rtx_send_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_rtp_rtx_send_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_rtp_rtx_send_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstFlowReturn gst_rtp_rtx_send_chain_list (GstPad * pad,
    GstObject * parent, GstBufferList * list);

static void gst_rtp_rtx_send_src_loop (GstRtpRtxSend * rtx);
static gboolean gst_rtp_rtx_send_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);

static GstStateChangeReturn gst_rtp_rtx_send_change_state (GstElement *
    element, GstStateChange transition);

static void gst_rtp_rtx_send_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_rtx_send_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rtp_rtx_send_finalize (GObject * object);

G_DEFINE_TYPE (GstRtpRtxSend, gst_rtp_rtx_send, GST_TYPE_ELEMENT);

typedef struct
{
  guint16 seqnum;
  guint32 timestamp;
  GstBuffer *buffer;
} BufferQueueItem;

typedef struct
{
  GstDataQueueItem item;
  GstCaps *rtx_caps;
} RtxDataQueueItem;

#define IS_RTX_ENABLED(rtx) (g_hash_table_size ((rtx)->rtx_pt_map) > 0)

static void
buffer_queue_item_free (BufferQueueItem * item)
{
  gst_buffer_unref (item->buffer);
  g_slice_free (BufferQueueItem, item);
}

typedef struct
{
  guint32 rtx_ssrc;
  GstCaps *rtx_caps;
  guint16 seqnum_base, next_seqnum;
  gint clock_rate;

  /* history of rtp packets */
  GSequence *queue;
} SSRCRtxData;

static SSRCRtxData *
ssrc_rtx_data_new (guint32 rtx_ssrc, guint rtx_payload_type,
    const GstCaps * master_stream_caps)
{
  GstStructure *s = gst_caps_get_structure (master_stream_caps, 0);
  SSRCRtxData *data = g_slice_new0 (SSRCRtxData);
  data->rtx_ssrc = rtx_ssrc;
  data->next_seqnum = data->seqnum_base = g_random_int_range (0, G_MAXUINT16);
  data->queue = g_sequence_new ((GDestroyNotify) buffer_queue_item_free);

  gst_structure_get_int (s, "clock-rate", &data->clock_rate);

  data->rtx_caps = gst_caps_copy (master_stream_caps);
  gst_caps_set_simple (data->rtx_caps,
      "ssrc", G_TYPE_UINT, data->rtx_ssrc,
      "seqnum-offset", G_TYPE_UINT, data->seqnum_base,
      "payload", G_TYPE_INT, rtx_payload_type, NULL);
  return data;
}

static void
ssrc_rtx_data_free (SSRCRtxData * data)
{
  gst_caps_unref (data->rtx_caps);
  g_sequence_free (data->queue);
  g_slice_free (SSRCRtxData, data);
}

typedef enum
{
  RTX_TASK_START,
  RTX_TASK_PAUSE,
  RTX_TASK_STOP,
} RtxTaskState;

static void
gst_rtp_rtx_send_set_flushing (GstRtpRtxSend * rtx, gboolean flush)
{
  GST_OBJECT_LOCK (rtx);
  gst_data_queue_set_flushing (rtx->queue, flush);
  gst_data_queue_flush (rtx->queue);
  GST_OBJECT_UNLOCK (rtx);
}

static gboolean
gst_rtp_rtx_send_set_task_state (GstRtpRtxSend * rtx, RtxTaskState task_state)
{
  GstTask *task = GST_PAD_TASK (rtx->srcpad);
  gboolean ret = TRUE;

  switch (task_state) {
    case RTX_TASK_START:
    {
      gboolean active = task && GST_TASK_STATE (task) == GST_TASK_STARTED;
      if (IS_RTX_ENABLED (rtx) && !active) {
        GST_DEBUG_OBJECT (rtx, "Starting RTX task");
        gst_rtp_rtx_send_set_flushing (rtx, FALSE);
        ret = gst_pad_start_task (rtx->srcpad,
            (GstTaskFunction) gst_rtp_rtx_send_src_loop, rtx, NULL);
      }
      break;
    }
    case RTX_TASK_PAUSE:
      if (task) {
        GST_DEBUG_OBJECT (rtx, "Pausing RTX task");
        gst_rtp_rtx_send_set_flushing (rtx, TRUE);
        ret = gst_pad_pause_task (rtx->srcpad);
      }
      break;
    case RTX_TASK_STOP:
      if (task) {
        GST_DEBUG_OBJECT (rtx, "Stopping RTX task");
        gst_rtp_rtx_send_set_flushing (rtx, TRUE);
        ret = gst_pad_stop_task (rtx->srcpad);
      }
      break;
  }

  return ret;
}

static void
gst_rtp_rtx_send_class_init (GstRtpRtxSendClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->get_property = gst_rtp_rtx_send_get_property;
  gobject_class->set_property = gst_rtp_rtx_send_set_property;
  gobject_class->finalize = gst_rtp_rtx_send_finalize;

  g_object_class_install_property (gobject_class, PROP_SSRC_MAP,
      g_param_spec_boxed ("ssrc-map", "SSRC Map",
          "Map of SSRCs to their retransmission SSRCs for SSRC-multiplexed mode"
          " (default = random)", GST_TYPE_STRUCTURE,
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PAYLOAD_TYPE_MAP,
      g_param_spec_boxed ("payload-type-map", "Payload Type Map",
          "Map of original payload types to their retransmission payload types",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_SIZE_TIME,
      g_param_spec_uint ("max-size-time", "Max Size Time",
          "Amount of ms to queue (0 = unlimited)", 0, G_MAXUINT,
          DEFAULT_MAX_SIZE_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_SIZE_PACKETS,
      g_param_spec_uint ("max-size-packets", "Max Size Packets",
          "Amount of packets to queue (0 = unlimited)", 0, G_MAXINT16,
          DEFAULT_MAX_SIZE_PACKETS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_REQUESTS,
      g_param_spec_uint ("num-rtx-requests", "Num RTX Requests",
          "Number of retransmission events received", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_PACKETS,
      g_param_spec_uint ("num-rtx-packets", "Num RTX Packets",
          " Number of retransmission packets sent", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_KBPS,
      g_param_spec_int ("max-kbps", "Maximum Kbps",
          "The maximum number of kilobits of RTX packets to allow "
          "(-1 = unlimited)", -1, G_MAXINT, DEFAULT_MAX_KBPS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_BUCKET_SIZE,
      g_param_spec_int ("max-bucket-size", "Maximum Bucket Size (Kb)",
          "The size of the token bucket, related to burstiness resilience "
          "(-1 = unlimited)", -1, G_MAXINT, DEFAULT_MAX_BUCKET_SIZE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "RTP Retransmission Sender", "Codec",
      "Retransmit RTP packets when needed, according to RFC4588",
      "Julien Isorce <julien.isorce@collabora.co.uk>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_change_state);
}

static void
gst_rtp_rtx_send_reset (GstRtpRtxSend * rtx)
{
  GST_OBJECT_LOCK (rtx);
  gst_data_queue_flush (rtx->queue);
  g_hash_table_remove_all (rtx->ssrc_data);
  g_hash_table_remove_all (rtx->rtx_ssrcs);
  rtx->num_rtx_requests = 0;
  rtx->num_rtx_packets = 0;
  GST_OBJECT_UNLOCK (rtx);
}

static void
gst_rtp_rtx_send_finalize (GObject * object)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (object);

  g_hash_table_unref (rtx->ssrc_data);
  g_hash_table_unref (rtx->rtx_ssrcs);
  if (rtx->external_ssrc_map)
    gst_structure_free (rtx->external_ssrc_map);
  g_hash_table_unref (rtx->rtx_pt_map);
  if (rtx->rtx_pt_map_structure)
    gst_structure_free (rtx->rtx_pt_map_structure);
  if (rtx->master_stream_caps)
    gst_caps_unref (rtx->master_stream_caps);
  if (rtx->rtx_stream_caps)
    gst_caps_unref (rtx->rtx_stream_caps);
  g_object_unref (rtx->queue);

  G_OBJECT_CLASS (gst_rtp_rtx_send_parent_class)->finalize (object);
}

static void
gst_rtp_rtx_send_init (GstRtpRtxSend * rtx)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (rtx);

  rtx->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  GST_PAD_SET_PROXY_CAPS (rtx->srcpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->srcpad);
  gst_pad_set_event_function (rtx->srcpad,
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_src_event));
  gst_pad_set_activatemode_function (rtx->srcpad,
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_activate_mode));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->srcpad);

  rtx->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");
  GST_PAD_SET_PROXY_CAPS (rtx->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->sinkpad);
  gst_pad_set_event_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_sink_event));
  gst_pad_set_chain_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_chain));
  gst_pad_set_chain_list_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rtp_rtx_send_chain_list));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->sinkpad);

  rtx->queue = gst_data_queue_new (gst_rtp_rtx_send_queue_check_full, NULL,
      NULL, rtx);
  rtx->ssrc_data = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) ssrc_rtx_data_free);
  rtx->rtx_ssrcs = g_hash_table_new (g_direct_hash, g_direct_equal);
  rtx->rtx_pt_map = g_hash_table_new (g_direct_hash, g_direct_equal);

  rtx->max_size_time = DEFAULT_MAX_SIZE_TIME;
  rtx->max_size_packets = DEFAULT_MAX_SIZE_PACKETS;
  rtx->prev_time = GST_CLOCK_TIME_NONE;
}

static gboolean
gst_rtp_rtx_send_queue_check_full (GstDataQueue * queue,
    guint visible, guint bytes, guint64 time, gpointer checkdata)
{
  return FALSE;
}

static void
gst_rtp_rtx_data_queue_item_free (gpointer item)
{
  RtxDataQueueItem *data = item;
  if (data->item.object)
    gst_mini_object_unref (data->item.object);
  if (data->rtx_caps)
    gst_caps_unref (data->rtx_caps);
  g_slice_free (RtxDataQueueItem, data);
}

static gboolean
gst_rtp_rtx_send_push_out (GstRtpRtxSend * rtx, gpointer object,
    GstCaps * rtx_caps)
{
  RtxDataQueueItem *data;
  GstDataQueueItem *gstdata;
  gboolean success;

  data = g_slice_new0 (RtxDataQueueItem);
  gstdata = (GstDataQueueItem *) data;
  gstdata->object = GST_MINI_OBJECT (object);
  gstdata->size = 1;
  gstdata->duration = 1;
  gstdata->visible = TRUE;
  gstdata->destroy = gst_rtp_rtx_data_queue_item_free;
  data->rtx_caps = rtx_caps;

  success = gst_data_queue_push (rtx->queue, gstdata);
  if (!success)
    gstdata->destroy (gstdata);

  return success;
}

static guint32
gst_rtp_rtx_send_choose_ssrc (GstRtpRtxSend * rtx, guint32 choice,
    gboolean consider_choice)
{
  guint32 ssrc = consider_choice ? choice : g_random_int ();

  /* make sure to be different than any other */
  while (g_hash_table_contains (rtx->ssrc_data, GUINT_TO_POINTER (ssrc)) ||
      g_hash_table_contains (rtx->rtx_ssrcs, GUINT_TO_POINTER (ssrc))) {
    ssrc = g_random_int ();
  }

  return ssrc;
}

static SSRCRtxData *
gst_rtp_rtx_send_new_ssrc_data (GstRtpRtxSend * rtx, guint32 ssrc,
    guint payload_type)
{
  gboolean consider = FALSE;
  guint32 rtx_ssrc = 0;
  guint32 consider_ssrc = 0;
  SSRCRtxData *data = NULL;
  GstCaps *current_caps = gst_pad_get_current_caps (rtx->sinkpad);
  guint rtx_payload_type =
      GPOINTER_TO_UINT (g_hash_table_lookup (rtx->rtx_pt_map,
          GUINT_TO_POINTER (payload_type)));

  if (rtx->external_ssrc_map) {
    gchar *ssrc_str;
    ssrc_str = g_strdup_printf ("%" G_GUINT32_FORMAT, ssrc);
    consider = gst_structure_get_uint (rtx->external_ssrc_map, ssrc_str,
        &consider_ssrc);
    g_free (ssrc_str);
  }
  rtx_ssrc = gst_rtp_rtx_send_choose_ssrc (rtx, consider_ssrc, consider);
  data = ssrc_rtx_data_new (rtx_ssrc, rtx_payload_type, current_caps);

  if (G_UNLIKELY (consider && rtx_ssrc != consider_ssrc))
    GST_WARNING_OBJECT (rtx,
        "ssrc from RTX SSRC map collided with existing ssrc, "
        "using %u instead of %u for RTX ssrc", rtx_ssrc, consider_ssrc);

  GST_DEBUG_OBJECT (rtx,
      "New master stream (payload: %d->%d, ssrc: %u->%u, "
      "caps: %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT ")",
      payload_type, rtx_payload_type, ssrc, rtx_ssrc,
      current_caps, data->rtx_caps);
  gst_caps_unref (current_caps);

  g_hash_table_insert (rtx->ssrc_data, GUINT_TO_POINTER (ssrc), data);
  g_hash_table_insert (rtx->rtx_ssrcs, GUINT_TO_POINTER (rtx_ssrc),
      GUINT_TO_POINTER (ssrc));

  return data;
}

/* Copy fixed header and extension. Add OSN before to copy payload
 * Copy memory to avoid to manually copy each rtp buffer field.
 */
static GstBuffer *
gst_rtp_rtx_buffer_new (GstRtpRtxSend * rtx, GstBuffer * buffer)
{
  GstMemory *mem = NULL;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstRTPBuffer new_rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *new_buffer = gst_buffer_new ();
  GstMapInfo map;
  guint payload_len = 0;
  SSRCRtxData *data;
  guint32 ssrc;
  guint16 seqnum;
  guint8 fmtp;

  gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp);

  /* get needed data from GstRtpRtxSend */
  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));
  ssrc = data->rtx_ssrc;
  seqnum = data->next_seqnum++;
  fmtp = GPOINTER_TO_UINT (g_hash_table_lookup (rtx->rtx_pt_map,
          GUINT_TO_POINTER (gst_rtp_buffer_get_payload_type (&rtp))));

  GST_DEBUG_OBJECT (rtx, "creating rtx buffer, orig seqnum: %u, "
      "rtx seqnum: %u, rtx ssrc: %X", gst_rtp_buffer_get_seq (&rtp),
      seqnum, ssrc);

  /* gst_rtp_buffer_map does not map the payload so do it now */
  gst_rtp_buffer_get_payload (&rtp);

  /* copy fixed header */
  mem = gst_memory_copy (rtp.map[0].memory, 0, rtp.size[0]);
  gst_buffer_append_memory (new_buffer, mem);

  /* copy extension if any */
  if (rtp.size[1]) {
    mem = gst_allocator_alloc (NULL, rtp.size[1], NULL);
    gst_memory_map (mem, &map, GST_MAP_WRITE);
    memcpy (map.data, rtp.data[1], rtp.size[1]);
    gst_memory_unmap (mem, &map);
    gst_buffer_append_memory (new_buffer, mem);
  }

  /* copy payload and add OSN just before */
  payload_len = 2 + rtp.size[2];
  mem = gst_allocator_alloc (NULL, payload_len, NULL);

  gst_memory_map (mem, &map, GST_MAP_WRITE);
  GST_WRITE_UINT16_BE (map.data, gst_rtp_buffer_get_seq (&rtp));
  if (rtp.size[2])
    memcpy (map.data + 2, rtp.data[2], rtp.size[2]);
  gst_memory_unmap (mem, &map);
  gst_buffer_append_memory (new_buffer, mem);

  /* everything needed is copied */
  gst_rtp_buffer_unmap (&rtp);

  /* set ssrc, seqnum and fmtp */
  gst_rtp_buffer_map (new_buffer, GST_MAP_WRITE, &new_rtp);
  gst_rtp_buffer_set_ssrc (&new_rtp, ssrc);
  gst_rtp_buffer_set_seq (&new_rtp, seqnum);
  gst_rtp_buffer_set_payload_type (&new_rtp, fmtp);
  /* RFC 4588: let other elements do the padding, as normal */
  gst_rtp_buffer_set_padding (&new_rtp, FALSE);
  gst_rtp_buffer_unmap (&new_rtp);

  /* Copy over timestamps */
  gst_buffer_copy_into (new_buffer, buffer, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);

  return new_buffer;
}

static gint
buffer_queue_items_cmp (BufferQueueItem * a, BufferQueueItem * b,
    gpointer user_data)
{
  /* gst_rtp_buffer_compare_seqnum returns the opposite of what we want,
   * it returns negative when seqnum1 > seqnum2 and we want negative
   * when b > a, i.e. a is smaller, so it comes first in the sequence */
  return gst_rtp_buffer_compare_seqnum (b->seqnum, a->seqnum);
}

static guint64
gst_rtp_rtx_send_get_tokens (GstRtpRtxSend * rtx, GstClock * clock)
{
  guint64 tokens = 0;
  GstClockTimeDiff elapsed_time = 0;
  GstClockTime current_time = 0;
  GstClockTimeDiff token_time;

  current_time = gst_clock_get_time (clock);

  if (!GST_CLOCK_TIME_IS_VALID (rtx->prev_time)) {
    rtx->prev_time = current_time;
    return 0;
  }

  if (current_time < rtx->prev_time) {
    GST_WARNING_OBJECT (rtx, "Clock is going backwards!!");
    return 0;
  }

  elapsed_time = GST_CLOCK_DIFF (rtx->prev_time, current_time);

  /* calculate number of tokens and how much time is "spent" by these tokens */
  tokens =
      gst_util_uint64_scale (elapsed_time, rtx->max_kbps * 1000, GST_SECOND);
  token_time = gst_util_uint64_scale (GST_SECOND, tokens, rtx->max_kbps * 1000);

  /* increment the time with how much we spent in terms of whole tokens */
  rtx->prev_time += token_time;
  return tokens;
}

static gboolean
gst_rtp_rtx_send_token_bucket (GstRtpRtxSend * rtx, GstBuffer * buf)
{
  GstClock *clock;
  gsize buffer_size;
  guint64 tokens;

  /* with an unlimited bucket-size, we have nothing to do */
  if (rtx->max_bucket_size == UNLIMITED_KBPS ||
    rtx->max_kbps == UNLIMITED_KBPS)
    return TRUE;

  /* without a clock, nothing to do */
  clock = GST_ELEMENT_CAST (rtx)->clock;
  if (clock == NULL) {
    GST_WARNING_OBJECT (rtx, "No clock, can't get the time");
    return TRUE;
  }

  buffer_size = gst_buffer_get_size (buf) * 8;
  tokens = gst_rtp_rtx_send_get_tokens (rtx, clock);

  rtx->bucket_size = MIN (rtx->max_bucket_size * 1000, rtx->bucket_size + tokens);
  GST_LOG_OBJECT (rtx, "Adding %lu tokens to bucket (contains %lu tokens)",
      tokens, rtx->bucket_size);

  if (buffer_size > rtx->bucket_size) {
    GST_DEBUG_OBJECT (rtx, "Buffer size (%lu) exeedes bucket size (%lu)",
        buffer_size, rtx->bucket_size);
    return FALSE;
  }

  rtx->bucket_size -= buffer_size;
  GST_LOG_OBJECT (rtx, "Buffer taking %lu tokens (%lu left)",
      buffer_size, rtx->bucket_size);
  return TRUE;
}

static gboolean
gst_rtp_rtx_send_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (parent);
  gboolean res;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *s = gst_event_get_structure (event);

      /* This event usually comes from the downstream gstrtpsession */
      if (gst_structure_has_name (s, "GstRTPRetransmissionRequest")) {
        guint seqnum = 0;
        guint ssrc = 0;
        GstBuffer *rtx_buf = NULL;
        GstCaps *rtx_buf_caps = NULL;

        /* retrieve seqnum of the packet that need to be retransmitted */
        if (!gst_structure_get_uint (s, "seqnum", &seqnum))
          seqnum = -1;

        /* retrieve ssrc of the packet that need to be retransmitted */
        if (!gst_structure_get_uint (s, "ssrc", &ssrc))
          ssrc = -1;

        GST_DEBUG_OBJECT (rtx, "got rtx request for seqnum: %u, ssrc: %X",
            seqnum, ssrc);

        GST_OBJECT_LOCK (rtx);
        /* check if request is for us */
        if (g_hash_table_contains (rtx->ssrc_data, GUINT_TO_POINTER (ssrc))) {
          SSRCRtxData *data;
          GSequenceIter *iter;
          BufferQueueItem search_item;

          /* update statistics */
          ++rtx->num_rtx_requests;

          data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));

          search_item.seqnum = seqnum;
          iter = g_sequence_lookup (data->queue, &search_item,
              (GCompareDataFunc) buffer_queue_items_cmp, NULL);
          if (iter) {
            BufferQueueItem *item = g_sequence_get (iter);
            GST_LOG_OBJECT (rtx, "found %" G_GUINT16_FORMAT, item->seqnum);
            if (gst_rtp_rtx_send_token_bucket (rtx, item->buffer)) {
              rtx_buf = gst_rtp_rtx_buffer_new (rtx, item->buffer);
              rtx_buf_caps = gst_caps_ref (data->rtx_caps);
            } else {
              GST_DEBUG_OBJECT (rtx, "Packet #%" G_GUINT16_FORMAT
                  " dropped due to full bucket", item->seqnum);
            }
          } else {
            BufferQueueItem *high_buf, *low_buf;
            high_buf = g_sequence_get (g_sequence_iter_prev (
                    g_sequence_get_end_iter (data->queue)));
            low_buf = g_sequence_get (g_sequence_get_begin_iter (data->queue));
            GST_INFO_OBJECT (rtx, "Packet #%" G_GUINT16_FORMAT
                " not found in the queue (contains: #%" G_GUINT16_FORMAT
                " -> #%" G_GUINT16_FORMAT ")",
                seqnum, low_buf->seqnum, high_buf->seqnum);
          }
        } else {
          GST_INFO_OBJECT (rtx, "ssrc: %X not found in map", ssrc);
        }
        GST_OBJECT_UNLOCK (rtx);

        if (rtx_buf)
          gst_rtp_rtx_send_push_out (rtx, rtx_buf, rtx_buf_caps);

        gst_event_unref (event);
        res = TRUE;

        /* This event usually comes from the downstream gstrtpsession */
      } else if (gst_structure_has_name (s, "GstRTPCollision")) {
        guint ssrc = 0;

        if (!gst_structure_get_uint (s, "ssrc", &ssrc))
          ssrc = -1;

        GST_DEBUG_OBJECT (rtx, "got ssrc collision, ssrc: %X", ssrc);

        GST_OBJECT_LOCK (rtx);

        /* choose another ssrc for our retransmited stream */
        if (g_hash_table_contains (rtx->rtx_ssrcs, GUINT_TO_POINTER (ssrc))) {
          guint master_ssrc;
          SSRCRtxData *data;

          master_ssrc = GPOINTER_TO_UINT (g_hash_table_lookup (rtx->rtx_ssrcs,
                  GUINT_TO_POINTER (ssrc)));
          data = g_hash_table_lookup (rtx->ssrc_data,
              GUINT_TO_POINTER (master_ssrc));

          /* change rtx_ssrc and update the reverse map */
          data->rtx_ssrc = gst_rtp_rtx_send_choose_ssrc (rtx, 0, FALSE);
          data->rtx_caps = gst_caps_make_writable (data->rtx_caps);
          gst_caps_set_simple (data->rtx_caps,
              "ssrc", G_TYPE_UINT, data->rtx_ssrc, NULL);
          g_hash_table_remove (rtx->rtx_ssrcs, GUINT_TO_POINTER (ssrc));
          g_hash_table_insert (rtx->rtx_ssrcs,
              GUINT_TO_POINTER (data->rtx_ssrc),
              GUINT_TO_POINTER (master_ssrc));

          GST_OBJECT_UNLOCK (rtx);

          /* no need to forward to payloader because we make sure to have
           * a different ssrc
           */
          gst_event_unref (event);
          res = TRUE;
        } else {
          /* if master ssrc has collided, remove it from our data, as it
           * is not going to be used any longer */
          if (g_hash_table_contains (rtx->ssrc_data, GUINT_TO_POINTER (ssrc))) {
            SSRCRtxData *data;
            data =
                g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));
            g_hash_table_remove (rtx->rtx_ssrcs,
                GUINT_TO_POINTER (data->rtx_ssrc));
            g_hash_table_remove (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));
          }

          GST_OBJECT_UNLOCK (rtx);

          /* forward event to payloader in case collided ssrc is
           * master stream */
          res = gst_pad_event_default (pad, parent, event);
        }
      } else {
        res = gst_pad_event_default (pad, parent, event);
      }
      break;
    }
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
  return res;
}

static gboolean
gst_rtp_rtx_send_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_pad_push_event (rtx->srcpad, event);
      gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_PAUSE);
      return TRUE;
    case GST_EVENT_FLUSH_STOP:
      gst_pad_push_event (rtx->srcpad, event);
      gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_START);
      return TRUE;
    default:
      break;
  }

  GST_OBJECT_LOCK (rtx);
  if (!IS_RTX_ENABLED (rtx)) {
    if (GST_EVENT_CAPS == GST_EVENT_TYPE (event)) {
      GstCaps *event_caps;
      gst_event_parse_caps (event, &event_caps);
      gst_caps_replace (&rtx->master_stream_caps, event_caps);
    }
  } else if (GST_EVENT_IS_SERIALIZED (event)) {
    GST_OBJECT_UNLOCK (rtx);

    GST_INFO_OBJECT (rtx, "Got %s event - enqueueing it",
        GST_EVENT_TYPE_NAME (event));
    gst_rtp_rtx_send_push_out (rtx, event, NULL);
    return TRUE;
  }

  GST_OBJECT_UNLOCK (rtx);
  return gst_pad_event_default (pad, parent, event);
}

/* like rtp_jitter_buffer_get_ts_diff() */
static guint32
gst_rtp_rtx_send_get_ts_diff (SSRCRtxData * data)
{
  guint64 high_ts, low_ts;
  BufferQueueItem *high_buf, *low_buf;
  guint32 result;

  high_buf =
      g_sequence_get (g_sequence_iter_prev (g_sequence_get_end_iter
          (data->queue)));
  low_buf = g_sequence_get (g_sequence_get_begin_iter (data->queue));

  if (!high_buf || !low_buf || high_buf == low_buf)
    return 0;

  if (data->clock_rate) {
    high_ts = high_buf->timestamp;
    low_ts = low_buf->timestamp;

    /* it needs to work if ts wraps */
    if (high_ts >= low_ts) {
      result = (guint32) (high_ts - low_ts);
    } else {
      result = (guint32) (high_ts + G_MAXUINT32 + 1 - low_ts);
    }
    result = gst_util_uint64_scale_int (result, 1000, data->clock_rate);
  } else {
    high_ts = GST_BUFFER_PTS (high_buf->buffer);
    low_ts = GST_BUFFER_PTS  (low_buf->buffer);
    result = gst_util_uint64_scale_int_round (high_ts - low_ts, 1, GST_MSECOND);
  }

  return result;
}

/* Must be called with lock */
static void
process_buffer (GstRtpRtxSend * rtx, GstBuffer * buffer)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  BufferQueueItem *item;
  SSRCRtxData *data;
  guint16 seqnum;
  guint8 payload_type;
  guint32 ssrc, rtptime;

  /* read the information we want from the buffer */
  gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp);
  seqnum = gst_rtp_buffer_get_seq (&rtp);
  payload_type = gst_rtp_buffer_get_payload_type (&rtp);
  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  rtptime = gst_rtp_buffer_get_timestamp (&rtp);
  gst_rtp_buffer_unmap (&rtp);

  GST_TRACE_OBJECT (rtx, "Processing buffer seqnum: %u, ssrc: %X", seqnum,
      ssrc);

  /* do not store the buffer if it's payload type is unknown */
  if (g_hash_table_contains (rtx->rtx_pt_map, GUINT_TO_POINTER (payload_type))) {
    if (G_LIKELY (g_hash_table_contains (rtx->ssrc_data,
                GUINT_TO_POINTER (ssrc)))) {
      data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));
    } else {
      data = gst_rtp_rtx_send_new_ssrc_data (rtx, ssrc, payload_type);
    }

    /* add current rtp buffer to queue history */
    item = g_slice_new0 (BufferQueueItem);
    item->seqnum = seqnum;
    item->timestamp = rtptime;
    item->buffer = gst_buffer_ref (buffer);
    g_sequence_append (data->queue, item);

    /* remove oldest packets from history if they are too many */
    if (rtx->max_size_packets) {
      while (g_sequence_get_length (data->queue) > rtx->max_size_packets)
        g_sequence_remove (g_sequence_get_begin_iter (data->queue));
    }
    if (rtx->max_size_time) {
      while (gst_rtp_rtx_send_get_ts_diff (data) > rtx->max_size_time)
        g_sequence_remove (g_sequence_get_begin_iter (data->queue));
    }
  }
}

static GstFlowReturn
gst_rtp_rtx_send_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (parent);

  GST_OBJECT_LOCK (rtx);
  if (IS_RTX_ENABLED (rtx)) {
    process_buffer (rtx, buffer);
    GST_OBJECT_UNLOCK (rtx);

    gst_rtp_rtx_send_push_out (rtx, buffer, NULL);
    return GST_FLOW_OK;
  }

  GST_OBJECT_UNLOCK (rtx);
  return gst_pad_push (rtx->srcpad, buffer);
}

static gboolean
process_buffer_from_list (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  process_buffer (user_data, *buffer);
  return TRUE;
}

static GstFlowReturn
gst_rtp_rtx_send_chain_list (GstPad * pad, GstObject * parent,
    GstBufferList * list)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (parent);
  GstFlowReturn ret;

  GST_OBJECT_LOCK (rtx);
  if (IS_RTX_ENABLED (rtx)) {
    gst_buffer_list_foreach (list, process_buffer_from_list, rtx);

    GST_OBJECT_UNLOCK (rtx);
    gst_rtp_rtx_send_push_out (rtx, list, NULL);
    return GST_FLOW_OK;
  }
  GST_OBJECT_UNLOCK (rtx);

  ret = gst_pad_push_list (rtx->srcpad, list);

  return ret;
}

static void
gst_rtp_rtx_send_src_loop (GstRtpRtxSend * rtx)
{
  RtxDataQueueItem *data;
  GstDataQueueItem *gstdata;

  if (gst_data_queue_pop (rtx->queue, &gstdata)) {
    data = (RtxDataQueueItem *) gstdata;
    if (G_LIKELY (GST_IS_BUFFER (gstdata->object))) {
      GstEvent *caps_event = NULL;
      gboolean is_rtx_buf = data->rtx_caps != NULL;

      GST_OBJECT_LOCK (rtx);
      if (is_rtx_buf) {
        rtx->num_rtx_packets++;
        if (!rtx->is_rtx_stream ||
            !gst_caps_is_equal (rtx->rtx_stream_caps, data->rtx_caps))
          caps_event = gst_event_new_caps (data->rtx_caps);
      } else if (rtx->is_rtx_stream)
        caps_event = gst_event_new_caps (rtx->master_stream_caps);

      rtx->is_rtx_stream = is_rtx_buf;
      if (rtx->is_rtx_stream)
        gst_caps_replace (&rtx->rtx_stream_caps, data->rtx_caps);
      else
        gst_caps_replace (&rtx->rtx_stream_caps, NULL);
      GST_OBJECT_UNLOCK (rtx);

      if (caps_event)
        gst_pad_push_event (rtx->srcpad, caps_event);
      gst_pad_push (rtx->srcpad, GST_BUFFER (gstdata->object));
    } else if (GST_IS_BUFFER_LIST (gstdata->object)) {
      /* Buffer lists can only come from the chain function, so we know we are
         dealing with master stream */
      GstEvent *caps_event = NULL;

      GST_OBJECT_LOCK (rtx);
      if (rtx->is_rtx_stream)
        caps_event = gst_event_new_caps (rtx->master_stream_caps);
      rtx->is_rtx_stream = FALSE;
      gst_caps_replace (&rtx->rtx_stream_caps, NULL);
      GST_OBJECT_UNLOCK (rtx);

      if (caps_event)
        gst_pad_push_event (rtx->srcpad, caps_event);
      gst_pad_push_list (rtx->srcpad, GST_BUFFER_LIST (gstdata->object));
    } else if (GST_IS_EVENT (gstdata->object)) {
      /* after EOS, we should not send any more buffers,
       * even if there are more requests coming in */
      if (GST_EVENT_TYPE (gstdata->object) == GST_EVENT_EOS) {
        gst_rtp_rtx_send_set_flushing (rtx, TRUE);
      } else if (GST_EVENT_TYPE (gstdata->object) == GST_EVENT_CAPS) {
        GstCaps *new_master_caps;
        gst_event_parse_caps (GST_EVENT (gstdata->object), &new_master_caps);
        gst_caps_replace (&rtx->master_stream_caps, new_master_caps);
      }
      gst_pad_push_event (rtx->srcpad, GST_EVENT (gstdata->object));
    } else {
      g_assert_not_reached ();
    }

    gstdata->object = NULL;     /* we no longer own that object */
    gstdata->destroy (data);
  } else {
    GST_LOG_OBJECT (rtx, "flushing");
    gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_PAUSE);
  }
}

static gboolean
gst_rtp_rtx_send_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (parent);
  gboolean ret = FALSE;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      if (active) {
        ret = gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_START);
      } else {
        ret = gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_STOP);
      }
      GST_INFO_OBJECT (rtx, "activate_mode: active %d, ret %d", active, ret);
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_rtp_rtx_send_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (object);

  switch (prop_id) {
    case PROP_PAYLOAD_TYPE_MAP:
      GST_OBJECT_LOCK (rtx);
      g_value_set_boxed (value, rtx->rtx_pt_map_structure);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_SIZE_TIME:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->max_size_time);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_SIZE_PACKETS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->max_size_packets);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_NUM_RTX_REQUESTS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_requests);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_NUM_RTX_PACKETS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_packets);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_KBPS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_int (value, rtx->max_kbps);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_BUCKET_SIZE:
      GST_OBJECT_LOCK (rtx);
      g_value_set_int (value, rtx->max_bucket_size);
      GST_OBJECT_UNLOCK (rtx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
structure_to_hash_table (GQuark field_id, const GValue * value, gpointer hash)
{
  const gchar *field_str;
  guint field_uint;
  guint value_uint;

  field_str = g_quark_to_string (field_id);
  field_uint = atoi (field_str);
  value_uint = g_value_get_uint (value);
  g_hash_table_insert ((GHashTable *) hash, GUINT_TO_POINTER (field_uint),
      GUINT_TO_POINTER (value_uint));

  return TRUE;
}

static void
gst_rtp_rtx_reset_bucket_size (GstRtpRtxSend *rtx,
    gint max_kbps, gint max_bucket_size)
{
  gboolean prev_unlimited = rtx->max_kbps == UNLIMITED_KBPS ||
      rtx->max_bucket_size == UNLIMITED_KBPS;
  gboolean unlimited = max_kbps == UNLIMITED_KBPS ||
        max_bucket_size == UNLIMITED_KBPS;

  /* Fill the bucket to max if we switched from unlimited to limited
   * kbps mode */
  if (prev_unlimited && !unlimited) {
    rtx->bucket_size = max_bucket_size * 1000;
    rtx->prev_time = GST_CLOCK_TIME_NONE;
  }
}

static void
gst_rtp_rtx_send_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstRtpRtxSend *rtx = GST_RTP_RTX_SEND (object);

  switch (prop_id) {
    case PROP_SSRC_MAP:
      GST_OBJECT_LOCK (rtx);
      if (rtx->external_ssrc_map)
        gst_structure_free (rtx->external_ssrc_map);
      rtx->external_ssrc_map = g_value_dup_boxed (value);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_PAYLOAD_TYPE_MAP:
      GST_OBJECT_LOCK (rtx);
      if (rtx->rtx_pt_map_structure)
        gst_structure_free (rtx->rtx_pt_map_structure);
      rtx->rtx_pt_map_structure = g_value_dup_boxed (value);
      g_hash_table_remove_all (rtx->rtx_pt_map);
      gst_structure_foreach (rtx->rtx_pt_map_structure, structure_to_hash_table,
          rtx->rtx_pt_map);
      GST_OBJECT_UNLOCK (rtx);

      if (IS_RTX_ENABLED (rtx))
        gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_START);
      else
        gst_rtp_rtx_send_set_task_state (rtx, RTX_TASK_STOP);

      break;
    case PROP_MAX_SIZE_TIME:
      GST_OBJECT_LOCK (rtx);
      rtx->max_size_time = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_SIZE_PACKETS:
      GST_OBJECT_LOCK (rtx);
      rtx->max_size_packets = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_KBPS:
      GST_OBJECT_LOCK (rtx);
      {
        gint max_kbps = g_value_get_int (value);
        gst_rtp_rtx_reset_bucket_size (rtx, max_kbps, rtx->max_bucket_size);
        rtx->max_kbps = max_kbps;
      }
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_BUCKET_SIZE:
      GST_OBJECT_LOCK (rtx);
      {
        gint max_bucket_size = g_value_get_int (value);
        gst_rtp_rtx_reset_bucket_size (rtx, rtx->max_kbps, max_bucket_size);
        rtx->max_bucket_size = max_bucket_size;
      }
      GST_OBJECT_UNLOCK (rtx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_rtp_rtx_send_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstRtpRtxSend *rtx;

  rtx = GST_RTP_RTX_SEND (element);

  switch (transition) {
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_rtp_rtx_send_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtp_rtx_send_reset (rtx);
      break;
    default:
      break;
  }

  return ret;
}

gboolean
gst_rtp_rtx_send_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_rtp_rtx_send_debug, "rtprtxsend", 0,
      "rtp retransmission sender");

  return gst_element_register (plugin, "rtprtxsend", GST_RANK_NONE,
      GST_TYPE_RTP_RTX_SEND);
}
