/* gstrtpvp8depay.c - Source for GstRtpVP8Depay
 * Copyright (C) 2011 Sjoerd Simons <sjoerd@luon.net>
 * Copyright (C) 2011 Collabora Ltd.
 *   Contact: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gstrtpvp8depay.h"
#include "gstrtputils.h"

#include <gst/video/video.h>
#include <gst/video/gstvideovp8meta.h>

#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC (gst_rtp_vp8_depay_debug);
#define GST_CAT_DEFAULT gst_rtp_vp8_depay_debug

static void gst_rtp_vp8_depay_dispose (GObject * object);
static GstBuffer *gst_rtp_vp8_depay_process (GstRTPBaseDepayload * depayload,
    GstRTPBuffer * rtp);
static GstStateChangeReturn gst_rtp_vp8_depay_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_rtp_vp8_depay_handle_event (GstRTPBaseDepayload * depay,
    GstEvent * event);
static gboolean gst_rtp_vp8_depay_packet_lost (GstRTPBaseDepayload * depay,
    GstEvent * event);

G_DEFINE_TYPE (GstRtpVP8Depay, gst_rtp_vp8_depay, GST_TYPE_RTP_BASE_DEPAYLOAD);

static GstStaticPadTemplate gst_rtp_vp8_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-vp8"));

static GstStaticPadTemplate gst_rtp_vp8_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "clock-rate = (int) 90000,"
        "media = (string) \"video\","
        "encoding-name = (string) { \"VP8\", \"VP8-DRAFT-IETF-01\" }"));

enum
{
  PROP_0,
  PROP_WAIT_FOR_KEYFRAME,
  PROP_HIDE_PICTURE_ID_GAP,
};

#define PICTURE_ID_NONE (UINT_MAX)
#define IS_PICTURE_ID_15BITS(pid) (((guint)(pid) & 0x8000) != 0)

#define DEFAULT_WAIT_FOR_KEYFRAME FALSE
#define DEFAULT_HIDE_PICTURE_ID_GAP FALSE

static void
gst_rtp_vp8_depay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (object);
  switch (prop_id) {
    case PROP_WAIT_FOR_KEYFRAME:
      self->wait_for_keyframe = g_value_get_boolean (value);
      break;
    case PROP_HIDE_PICTURE_ID_GAP:
      self->hide_picture_id_gap = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_vp8_depay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (object);
  switch (prop_id) {
    case PROP_WAIT_FOR_KEYFRAME:
      g_value_set_boolean (value, self->wait_for_keyframe);
      break;
    case PROP_HIDE_PICTURE_ID_GAP:
      g_value_set_boolean (value, self->hide_picture_id_gap);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_vp8_depay_init (GstRtpVP8Depay * self)
{
  self->adapter = gst_adapter_new ();
  self->started = FALSE;
  self->wait_for_keyframe = DEFAULT_WAIT_FOR_KEYFRAME;
  self->last_pushed_was_lost_event = FALSE;
}

static void
gst_rtp_vp8_depay_class_init (GstRtpVP8DepayClass * gst_rtp_vp8_depay_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (gst_rtp_vp8_depay_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (gst_rtp_vp8_depay_class);
  GstRTPBaseDepayloadClass *depay_class =
      (GstRTPBaseDepayloadClass *) (gst_rtp_vp8_depay_class);

  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_vp8_depay_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_vp8_depay_src_template);

  gst_element_class_set_static_metadata (element_class, "RTP VP8 depayloader",
      "Codec/Depayloader/Network/RTP",
      "Extracts VP8 video from RTP packets)",
      "Sjoerd Simons <sjoerd@luon.net>");

  object_class->dispose = gst_rtp_vp8_depay_dispose;
  object_class->set_property = gst_rtp_vp8_depay_set_property;
  object_class->get_property = gst_rtp_vp8_depay_get_property;

  element_class->change_state = gst_rtp_vp8_depay_change_state;

  depay_class->process_rtp_packet = gst_rtp_vp8_depay_process;
  depay_class->handle_event = gst_rtp_vp8_depay_handle_event;
  depay_class->packet_lost = gst_rtp_vp8_depay_packet_lost;

  g_object_class_install_property (object_class, PROP_WAIT_FOR_KEYFRAME,
      g_param_spec_boolean ("wait-for-keyframe", "Wait for Keyframe",
          "Wait for the next keyframe after packet loss",
          DEFAULT_WAIT_FOR_KEYFRAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_HIDE_PICTURE_ID_GAP,
      g_param_spec_boolean ("hide-picture-id-gap", "Hide Picture ID Gap",
          "Wether to trigger a key-unit request when there is a gap in "
          "the picture ID", DEFAULT_HIDE_PICTURE_ID_GAP,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (gst_rtp_vp8_depay_debug, "rtpvp8depay", 0,
      "VP8 Video RTP Depayloader");
}

static void
gst_rtp_vp8_depay_dispose (GObject * object)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY (object);

  if (self->adapter != NULL)
    g_object_unref (self->adapter);
  self->adapter = NULL;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (gst_rtp_vp8_depay_parent_class)->dispose)
    G_OBJECT_CLASS (gst_rtp_vp8_depay_parent_class)->dispose (object);
}

static gint
picture_id_compare (guint16 id0, guint16 id1)
{
  guint shift = 16 - (IS_PICTURE_ID_15BITS (id1) ? 15 : 7);
  id0 = id0 << shift;
  id1 = id1 << shift;
  return ((gint16) (id1 - id0)) >> shift;
}

static void
send_last_lost_event (GstRtpVP8Depay * self)
{
  GST_DEBUG_OBJECT (self,
      "Sending the last stopped lost event: %" GST_PTR_FORMAT,
      self->last_lost_event);
  GST_RTP_BASE_DEPAYLOAD_CLASS (gst_rtp_vp8_depay_parent_class)
      ->packet_lost (GST_RTP_BASE_DEPAYLOAD_CAST (self),
      self->last_lost_event);
  gst_event_unref (self->last_lost_event);
  self->last_lost_event = NULL;
  self->last_pushed_was_lost_event = TRUE;
}

static void
send_new_lost_event (GstRtpVP8Depay * self, GstClockTime timestamp,
    guint new_picture_id, const gchar * reason)
{
  GstEvent *event;

  if (!GST_CLOCK_TIME_IS_VALID (timestamp)) {
    GST_WARNING_OBJECT (self,
        "Can't create lost event with invalid timestmap");
    return;
  }

  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("GstRTPPacketLost",
          "timestamp", G_TYPE_UINT64, timestamp,
          "duration", G_TYPE_UINT64, 0,
          "no-packet-loss", G_TYPE_BOOLEAN, self->hide_picture_id_gap,
          NULL));

  GST_DEBUG_OBJECT (self, "Pushing lost event "
      "(picids 0x%x 0x%x, reason \"%s\"): %" GST_PTR_FORMAT,
      self->last_picture_id, new_picture_id, reason, event);

  GST_RTP_BASE_DEPAYLOAD_CLASS (gst_rtp_vp8_depay_parent_class)
    ->packet_lost (GST_RTP_BASE_DEPAYLOAD_CAST (self), event);

  gst_event_unref (event);
  self->last_pushed_was_lost_event = TRUE;
}

static void
send_lost_event_if_needed (GstRtpVP8Depay * self, guint new_picture_id,
    GstClockTime lost_event_timestamp)
{
  const gchar *reason = NULL;
  gboolean fwd_last_lost_event = FALSE;
  gboolean create_lost_event = FALSE;

  if (self->last_picture_id == PICTURE_ID_NONE ||
      self->last_picture_id == new_picture_id)
    return;

  if (new_picture_id == PICTURE_ID_NONE) {
    reason = "picture id does not exist";
    fwd_last_lost_event = TRUE;
  } else if (IS_PICTURE_ID_15BITS (self->last_picture_id) &&
      !IS_PICTURE_ID_15BITS (new_picture_id)) {
    reason = "picture id has less bits than before";
    fwd_last_lost_event = TRUE;
  } else if (picture_id_compare (self->last_picture_id, new_picture_id) != 1) {
    reason = "picture id gap";
    fwd_last_lost_event = TRUE;
    /* Only create a new one if we just didn't push a lost event */
    create_lost_event = self->last_pushed_was_lost_event == FALSE;
  }

  if (self->last_lost_event) {
    if (fwd_last_lost_event) {
      GST_DEBUG_OBJECT (self, "Forwarding lost event "
          "(picids 0x%x 0x%x, reason \"%s\"): %" GST_PTR_FORMAT,
          self->last_picture_id, new_picture_id, reason, self->last_lost_event);
      GST_RTP_BASE_DEPAYLOAD_CLASS (gst_rtp_vp8_depay_parent_class)
          ->packet_lost (GST_RTP_BASE_DEPAYLOAD_CAST (self),
          self->last_lost_event);

      // If we forward last received lost event, there is no need
      // to create another one
      create_lost_event = FALSE;
      self->last_pushed_was_lost_event = TRUE;
    }
    gst_event_unref (self->last_lost_event);
    self->last_lost_event = NULL;
  }

  if (create_lost_event)
    send_new_lost_event (self, lost_event_timestamp, new_picture_id, reason);
}

static GstBuffer *
gst_rtp_vp8_depay_process (GstRTPBaseDepayload * depay, GstRTPBuffer * rtp)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (depay);
  GstBuffer *payload;
  guint8 *data;
  guint hdridx = 0;
  guint picture_id = PICTURE_ID_NONE;
  gboolean temporally_scaled = FALSE;
  guint tl0picidx = 0;
  guint tid_y_keyidx = 0;
  guint size = gst_rtp_buffer_get_payload_len (rtp);
  guint s_bit;
  guint part_id;
  gboolean frame_start;
  gboolean sent_lost_event = FALSE;
  gboolean prev_frame_is_dropped = FALSE;

  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (rtp->buffer))) {
    GST_DEBUG_OBJECT (self, "Discontinuity, flushing adapter");
    gst_adapter_clear (self->adapter);

    // self->started is true means
    // we have a frame data in adapter.
    // Cleanning this data means we
    // introduce the gap in frame flow.
    prev_frame_is_dropped = self->started;
    self->started = FALSE;

    if (self->wait_for_keyframe)
      self->waiting_for_keyframe = TRUE;
  }

  /* At least one header and one vp8 byte */
  if (G_UNLIKELY (size < 2))
    goto too_small;

  data = gst_rtp_buffer_get_payload (rtp);

  s_bit = (data[0] >> 4) & 0x1;
  part_id = (data[0] >> 0) & 0x7;

  /* Check X optional header */
  if ((data[0] & 0x80) != 0) {
    hdridx++;
    /* Check I optional header */
    if ((data[1] & 0x80) != 0) {
      hdridx++;
      if (G_UNLIKELY (size <= hdridx))
        goto too_small;
      /* Check for 16 bits PictureID */
      picture_id = data[hdridx];
      if ((picture_id & 0x80) != 0) {
        hdridx++;
        if (G_UNLIKELY (size <= hdridx))
          goto too_small;
        /* Retain marker bit as IS_PICTURE_ID_15BITS uses it */
        picture_id = (picture_id << 8) | data[hdridx];
      }
    }
    /* Stream is temporally scaled if L or T bits are set */
    temporally_scaled = ((data[1] & 0x60) != 0);
    /* Check L optional header */
    if ((data[1] & 0x40) != 0) {
      hdridx++;
      if (G_UNLIKELY (size <= hdridx))
         goto too_small;
      /* TL0PICIDX must be ignored unless T is set */
      if ((data[1] & 0x20) != 0)
        tl0picidx = data[hdridx];
    }
    /* Check T or K optional headers */
    if ((data[1] & 0x20) != 0 || (data[1] & 0x10) != 0) {
      hdridx++;
      if (G_UNLIKELY (size <= hdridx))
        goto too_small;
      tid_y_keyidx = data[hdridx];
    }
  }

  GST_LOG_OBJECT (depay,
      "hdrsize %u, size %u, picture id 0x%x, s %u, part_id %u",
      hdridx + 1, size, picture_id, s_bit, part_id);
  if (G_UNLIKELY (size <= hdridx))
    goto too_small;

  frame_start = (s_bit == 1) && (part_id == 0);

  // Detected the start of a new frame
  if (frame_start) {
    if (G_UNLIKELY (self->started)) {
      // If we still in a state of self->started == true
      // that means we never saw the end of the frame.
      // So this case is considered to be a GAP (incomlete frame)
      GST_DEBUG_OBJECT (depay, "Incomplete frame, flushing adapter");
      gst_adapter_clear (self->adapter);
      self->started = FALSE;

      /* FIXME: Add property to control whether to send GAP events */
      send_new_lost_event (self, GST_BUFFER_PTS (rtp->buffer), picture_id,
          "Incomplete frame detected");
      sent_lost_event = TRUE;
    }
    else if (G_UNLIKELY (prev_frame_is_dropped && !self->last_pushed_was_lost_event)) {
      // We has droped the previous frmae due to DISCONT flag in
      // the current packet. We need to send the lost event if it hasn't
      // been send already (by jitterbuffer)
      /* FIXME: Add property to control whether to send GAP events */
      send_new_lost_event (self, GST_BUFFER_PTS (rtp->buffer), picture_id,
          "Incomplete frame detected");
      sent_lost_event = TRUE;
    }
  }

  // Wating for start of the new frame
  if (!self->started) {
    if (G_UNLIKELY (!frame_start)) {
      // If the current packet is not the start of the
      // frame ignoring this packets.
      if (prev_frame_is_dropped
          && !self->last_pushed_was_lost_event && !sent_lost_event) {
        // But we need to send GAP event if the previous
        // frame was actually dropped and the event
        // has never been sent for some reasons.
        if (self->last_lost_event) {
          send_last_lost_event (self);
        } else {
          /* FIXME: Add property to control whether to send GAP events */
          send_new_lost_event (self, GST_BUFFER_PTS (rtp->buffer), picture_id,
              "Incomplete frame detected");
        }
        self->stop_lost_events = FALSE;
      }
      GST_DEBUG_OBJECT (depay,
          "The frame is missing the first packet, ignoring the packet");
      goto done;
    }

    GST_LOG_OBJECT (depay, "Found the start of the frame");

    /* Check gap in picture IDs */
    if (!sent_lost_event)
      send_lost_event_if_needed (self, picture_id, GST_BUFFER_PTS (rtp->buffer));
    self->started = TRUE;
    self->stop_lost_events = FALSE;
  }

  payload = gst_rtp_buffer_get_payload_subbuffer (rtp, hdridx + 1, -1);
  gst_adapter_push (self->adapter, payload);
  self->last_picture_id = picture_id;

  /* Marker indicates that it was the last rtp packet for this frame */
  if (gst_rtp_buffer_get_marker (rtp)) {
    GstBuffer *out;
    guint8 header[10];

    GST_LOG_OBJECT (depay,
        "Found the end of the frame (%" G_GSIZE_FORMAT " bytes)",
        gst_adapter_available (self->adapter));
    if (gst_adapter_available (self->adapter) < 10)
      goto too_small;
    gst_adapter_copy (self->adapter, &header, 0, 10);

    out = gst_adapter_take_buffer (self->adapter,
        gst_adapter_available (self->adapter));

    self->started = FALSE;

    /* mark keyframes */
    out = gst_buffer_make_writable (out);
    /* Filter away all metas that are not sensible to copy */
    gst_rtp_drop_non_video_meta (self, out);
    gst_buffer_add_video_vp8_meta_full (out,
        temporally_scaled,
        ((tid_y_keyidx & 0x20) == 0x20), /* Unpack Y bit */
        ((tid_y_keyidx & 0xc0) >> 6), /* Unpack TID */
        tl0picidx);
    if ((header[0] & 0x01)) {
      GST_BUFFER_FLAG_SET (out, GST_BUFFER_FLAG_DELTA_UNIT);

      if (self->waiting_for_keyframe) {
        gst_buffer_unref (out);
        out = NULL;
        GST_INFO_OBJECT (self, "Dropping inter-frame before intra-frame");
        gst_pad_push_event (GST_RTP_BASE_DEPAYLOAD_SINKPAD (depay),
            gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
                TRUE, 0));
      }
    } else {
      guint profile, width, height;

      GST_BUFFER_FLAG_UNSET (out, GST_BUFFER_FLAG_DELTA_UNIT);
      GST_DEBUG_OBJECT (self, "Processed keyframe");

      profile = (header[0] & 0x0e) >> 1;
      width = GST_READ_UINT16_LE (header + 6) & 0x3fff;
      height = GST_READ_UINT16_LE (header + 8) & 0x3fff;

      if (G_UNLIKELY (self->last_width != width ||
              self->last_height != height || self->last_profile != profile)) {
        gchar profile_str[3];
        GstCaps *srccaps;

        snprintf (profile_str, 3, "%u", profile);
        srccaps = gst_caps_new_simple ("video/x-vp8",
            "framerate", GST_TYPE_FRACTION, 0, 1,
            "height", G_TYPE_INT, height,
            "width", G_TYPE_INT, width,
            "profile", G_TYPE_STRING, profile_str, NULL);

        gst_pad_set_caps (GST_RTP_BASE_DEPAYLOAD_SRCPAD (depay), srccaps);
        gst_caps_unref (srccaps);

        self->last_width = width;
        self->last_height = height;
        self->last_profile = profile;
      }
      self->waiting_for_keyframe = FALSE;
    }

    if (picture_id != PICTURE_ID_NONE)
      self->stop_lost_events = TRUE;

    self->last_pushed_was_lost_event = FALSE;

    return out;
  }

done:
  return NULL;

too_small:
  GST_DEBUG_OBJECT (self, "Invalid rtp packet (too small), ignoring");
  gst_adapter_clear (self->adapter);
  if (self->started) {
    /* FIXME: Add property to control whether to send GAP events */
    send_new_lost_event (self, GST_BUFFER_PTS (rtp->buffer), picture_id,
        "Invalid rtp packet detected");
  }
  self->started = FALSE;

  goto done;
}

static GstStateChangeReturn
gst_rtp_vp8_depay_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->last_profile = -1;
      self->last_height = -1;
      self->last_width = -1;
      self->waiting_for_keyframe = TRUE;
      self->caps_sent = FALSE;
      self->last_picture_id = PICTURE_ID_NONE;
      if (self->last_lost_event) {
        gst_event_unref (self->last_lost_event);
        self->last_lost_event = NULL;
      }
      self->stop_lost_events = FALSE;
      break;
    default:
      break;
  }

  return
      GST_ELEMENT_CLASS (gst_rtp_vp8_depay_parent_class)->change_state (element,
      transition);
}

static gboolean
gst_rtp_vp8_depay_handle_event (GstRTPBaseDepayload * depay, GstEvent * event)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (depay);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      self->last_profile = -1;
      self->last_height = -1;
      self->last_width = -1;
      self->last_picture_id = PICTURE_ID_NONE;
      if (self->last_lost_event) {
        gst_event_unref (self->last_lost_event);
        self->last_lost_event = NULL;
      }
      self->stop_lost_events = FALSE;
      break;
    default:
      break;
  }

  return
      GST_RTP_BASE_DEPAYLOAD_CLASS
      (gst_rtp_vp8_depay_parent_class)->handle_event (depay, event);
}

static gboolean
gst_rtp_vp8_depay_packet_lost (GstRTPBaseDepayload * depay, GstEvent * event)
{
  GstRtpVP8Depay *self = GST_RTP_VP8_DEPAY_CAST (depay);
  if (self->stop_lost_events) {
    GST_DEBUG_OBJECT (depay, "Stopping lost event %" GST_PTR_FORMAT, event);
    if (self->last_lost_event)
      gst_event_unref (self->last_lost_event);
    self->last_lost_event = gst_event_ref (event);
    return TRUE;
  }

  self->last_pushed_was_lost_event = TRUE;

  return
      GST_RTP_BASE_DEPAYLOAD_CLASS
      (gst_rtp_vp8_depay_parent_class)->packet_lost (depay, event);
}

gboolean
gst_rtp_vp8_depay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpvp8depay",
      GST_RANK_MARGINAL, GST_TYPE_RTP_VP8_DEPAY);
}
