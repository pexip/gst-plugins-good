/* GStreamer
 *
 * Copyright (C) 2016 Pexip AS
 *   @author Stian Selnes <stian@pexip.com>
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

#include <gst/check/check.h>
#include <gst/check/gstharness.h>
#include <gst/video/gstvideovp8meta.h>

#define RTP_VP8_CAPS_STR \
  "application/x-rtp,media=video,encoding-name=VP8,clock-rate=90000,payload=96"

enum {
    BT_PLAIN_PICID_NONE,
    BT_PLAIN_PICID_7,
    BT_PLAIN_PICID_15,
    BT_TS_PICID_NONE,
    BT_TS_PICID_7,
    BT_TS_PICID_15,
    BT_TS_PICID_7_NO_TLOPICIDX,
    BT_TS_PICID_7_NO_TID_Y_KEYIDX
};

static guint8 intra_picid6336_seqnum0[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x80, 0x98, 0xc0, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_picid24_seqnum0[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x80, 0x18, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_nopicid_seqnum0[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x00, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_picid24_seqnum0_tl1_sync_tl0picidx12[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0xe0, 0x18, 0x0c, 0x60, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_picid6336_seqnum0_tl1_sync_tl0picidx12[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0xe0, 0x98, 0xc0, 0x0c, 0x60, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_nopicid_seqnum0_tl1_sync_tl0picidx12[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x60, 0x0c, 0x60, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_picid24_seqnum0_tl1_sync_no_tl0picidx[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0xa0, 0x18, 0x60, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};
static guint8 intra_picid24_seqnum0_notyk_tl0picidx12[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0xc0, 0x18, 0x0c, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};


static GstBuffer *
create_rtp_vp8_buffer (guint seqnum, guint picid, guint buffer_type,
    GstClockTime buf_pts)
{
  static struct BufferTemplate {
    guint8 * template;
    gsize size;
    gint picid_bits;
  } templates[] = {
    { intra_nopicid_seqnum0, sizeof (intra_nopicid_seqnum0), 0 },
    { intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0), 7 },
    { intra_picid6336_seqnum0, sizeof (intra_picid6336_seqnum0), 15 },

    { intra_nopicid_seqnum0_tl1_sync_tl0picidx12,
      sizeof (intra_nopicid_seqnum0_tl1_sync_tl0picidx12),
      0
    },
    { intra_picid24_seqnum0_tl1_sync_tl0picidx12,
      sizeof (intra_picid24_seqnum0_tl1_sync_tl0picidx12),
      7
    },
    { intra_picid6336_seqnum0_tl1_sync_tl0picidx12,
      sizeof (intra_picid6336_seqnum0_tl1_sync_tl0picidx12),
      15
    },

    { intra_picid24_seqnum0_tl1_sync_no_tl0picidx,
      sizeof (intra_picid24_seqnum0_tl1_sync_no_tl0picidx),
      7
    },
    { intra_picid24_seqnum0_notyk_tl0picidx12,
      sizeof (intra_picid24_seqnum0_notyk_tl0picidx12),
      7
    }
  };
  struct BufferTemplate *template = &templates[buffer_type];
  guint8 *packet = g_memdup (template->template, template->size);
  GstBuffer *ret;

  if (template->picid_bits == 7) {
    packet[14] = picid & 0x7f;
  } else if (template->picid_bits == 15) {
    packet[14] = ((picid >> 8) & 0xff) | 0x80;
    packet[15] = (picid >> 0) & 0xff;
  }

  packet[2] = (seqnum >> 8) & 0xff;
  packet[3] = (seqnum >> 0) & 0xff;

  ret = gst_buffer_new_wrapped (packet, template->size);
  GST_BUFFER_PTS (ret) = buf_pts;
  return ret;
}

typedef struct _DepayGapEventTestData
{
  gint seq_num;
  gint picid;
  guint buffer_type;
} DepayGapEventTestData;

static void
test_depay_gap_event_base (const DepayGapEventTestData *data,
    gboolean send_lost_event, gboolean expect_gap_event)
{
  GstEvent *event;
  GstClockTime pts = 0;
  GstHarness *h = gst_harness_new ("rtpvp8depay");
  if (send_lost_event == FALSE && expect_gap_event) {
    /* Expect picture ID gaps to be concealed, so tell the element to do so. */
    g_object_set (h->element, "hide-picture-id-gap", TRUE, NULL);
  }
  gst_harness_set_src_caps_str (h, RTP_VP8_CAPS_STR);

  gst_harness_push (h, create_rtp_vp8_buffer (data[0].seq_num, data[0].picid, data[0].buffer_type, pts));
  pts += 33 * GST_MSECOND;

  // Preparation before pushing gap event. Getting rid of all events which
  // came by this point - segment, caps, etc
  for (gint i = 0; i < 3; i++)
    gst_event_unref (gst_harness_pull_event (h));
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  if (send_lost_event) {
    gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
        gst_structure_new ("GstRTPPacketLost",
            "timestamp", G_TYPE_UINT64, pts,
            "duration", G_TYPE_UINT64, 33*GST_MSECOND, NULL)));
    pts += 33 * GST_MSECOND;
  }

  gst_harness_push (h, create_rtp_vp8_buffer (data[1].seq_num, data[1].picid, data[1].buffer_type, pts));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  if (expect_gap_event) {
    gboolean noloss = FALSE;

    // Making shure the GAP event was pushed downstream
    event = gst_harness_pull_event (h);
    fail_unless_equals_string ("gap", gst_event_type_get_name (GST_EVENT_TYPE (event)));
    gst_structure_get_boolean (gst_event_get_structure (event), "no-packet-loss", &noloss);

    // If we didn't send GstRTPPacketLost event, the gap
    // event should indicate that with 'no-packet-loss' parameter
    fail_unless_equals_int (noloss, !send_lost_event);
    gst_event_unref (event);
  }
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  gst_harness_teardown (h);
}

// Packet loss + no loss in picture ids
static const DepayGapEventTestData stop_gap_events_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, BT_PLAIN_PICID_7}, {102, 25, BT_PLAIN_PICID_7}},

  // 15bit picture ids
  {{100, 250, BT_PLAIN_PICID_15}, {102, 251, BT_PLAIN_PICID_15}},

  // 7bit picture ids wrap
  {{100, 127, BT_PLAIN_PICID_7}, {102, 0, BT_PLAIN_PICID_7}},

  // 15bit picture ids wrap
  {{100, 32767, BT_PLAIN_PICID_15}, {102, 0, BT_PLAIN_PICID_15}},

  // 7bit to 15bit picture id
  {{100, 127, BT_PLAIN_PICID_7}, {102, 128, BT_PLAIN_PICID_15}},
};

GST_START_TEST (test_depay_stop_gap_events)
{
  test_depay_gap_event_base (&stop_gap_events_test_data[__i__][0], TRUE, FALSE);
}
GST_END_TEST;

// Packet loss + lost picture ids
static const DepayGapEventTestData resend_gap_event_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, BT_PLAIN_PICID_7}, {102, 26, BT_PLAIN_PICID_7}},

  // 15bit picture ids
  {{100, 250, BT_PLAIN_PICID_15}, {102, 252, BT_PLAIN_PICID_15}},

  // 7bit picture ids wrap
  {{100, 127, BT_PLAIN_PICID_7}, {102, 1, BT_PLAIN_PICID_7}},

  // 15bit picture ids wrap
  {{100, 32767, BT_PLAIN_PICID_15}, {102, 1, BT_PLAIN_PICID_15}},

  // 7bit to 15bit picture id
  {{100, 126, BT_PLAIN_PICID_7}, {102, 129, BT_PLAIN_PICID_15}},
};

GST_START_TEST (test_depay_resend_gap_event)
{
  test_depay_gap_event_base (&resend_gap_event_test_data[__i__][0], TRUE, TRUE);
}
GST_END_TEST;

// Packet loss + one of picture ids does not exist
static const DepayGapEventTestData resend_gap_event_nopicid_test_data[][2] = {
  {{100, 24, BT_PLAIN_PICID_7}, {102, 0, BT_PLAIN_PICID_NONE}},
  {{100, 0, BT_PLAIN_PICID_NONE}, {102, 26, BT_PLAIN_PICID_7}},
  {{100, 0, BT_PLAIN_PICID_NONE}, {102, 0, BT_PLAIN_PICID_NONE}},
};

GST_START_TEST (test_depay_resend_gap_event_nopicid)
{
  test_depay_gap_event_base (&resend_gap_event_nopicid_test_data[__i__][0], TRUE, TRUE);
}
GST_END_TEST;

// No packet loss + lost picture ids
static const DepayGapEventTestData create_gap_event_on_picid_gaps_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, BT_PLAIN_PICID_7}, {101, 26, BT_PLAIN_PICID_7}},

  // 15bit picture ids
  {{100, 250, BT_PLAIN_PICID_15}, {101, 252, BT_PLAIN_PICID_15}},

  // 7bit picture ids wrap
  {{100, 127, BT_PLAIN_PICID_7}, {101, 1, BT_PLAIN_PICID_7}},

  // 15bit picture ids wrap
  {{100, 32767, BT_PLAIN_PICID_15}, {101, 1, BT_PLAIN_PICID_15}},

  // 7bit to 15bit picture id
  {{100, 126, BT_PLAIN_PICID_15}, {101, 129, BT_PLAIN_PICID_15}},
};

GST_START_TEST (test_depay_create_gap_event_on_picid_gaps)
{
  test_depay_gap_event_base (&create_gap_event_on_picid_gaps_test_data[__i__][0], FALSE, TRUE);
}
GST_END_TEST;

// No packet loss + one of picture ids does not exist
static const DepayGapEventTestData nopicid_test_data[][2] = {
  {{100, 24, BT_PLAIN_PICID_7}, {101, 0, BT_PLAIN_PICID_NONE}},
  {{100, 0, BT_PLAIN_PICID_NONE}, {101, 26, BT_PLAIN_PICID_7}},
  {{100, 0, BT_PLAIN_PICID_NONE}, {101, 0, BT_PLAIN_PICID_NONE}},
};

GST_START_TEST (test_depay_nopicid)
{
  test_depay_gap_event_base (&nopicid_test_data[__i__][0], FALSE, FALSE);
}
GST_END_TEST;

#define verify_meta(buffer, picid, use_ts, ybit, tid, tl0picindex)      \
  G_STMT_START {                                                        \
    GstVideoVP8Meta *meta = gst_buffer_get_video_vp8_meta (buffer);     \
    fail_unless (meta != NULL);                                         \
                                                                        \
    fail_unless_equals_int (picid, meta->picture_id);                   \
    fail_unless_equals_int (use_ts, meta->use_temporal_scaling);        \
    fail_unless_equals_int (ybit, meta->layer_sync);                    \
    fail_unless_equals_int (tid, meta->temporal_layer_id);              \
    fail_unless_equals_int (tl0picindex, meta->tl0picidx);              \
  } G_STMT_END

GST_START_TEST (test_depay_temporally_scaled)
{
  GstBuffer *buffer;
  GstClockTime pts = 0;
  GstHarness *h = gst_harness_new ("rtpvp8depay");
  gst_harness_set_src_caps_str (h, RTP_VP8_CAPS_STR);

  /* Push non-temporally-scaled packet */
  gst_harness_push(h, create_rtp_vp8_buffer (1000, 9, BT_PLAIN_PICID_7, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 9, FALSE, FALSE, 0, 0);
  gst_buffer_unref (buffer);

  /* Push temporally scaled packet (no picid) */
  pts += 33 * GST_MSECOND;
  gst_harness_push (h, create_rtp_vp8_buffer (1001, 10, BT_TS_PICID_NONE, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 65535, TRUE, TRUE, 1, 12);
  gst_buffer_unref (buffer);

  /* Push temporally scaled packet (7bit picid) */
  pts += 33 * GST_MSECOND;
  gst_harness_push (h, create_rtp_vp8_buffer (1002, 11, BT_TS_PICID_7, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 11, TRUE, TRUE, 1, 12);
  gst_buffer_unref (buffer);

  /* Push temporally scaled packet (15bit picid) */
  pts += 33 * GST_MSECOND;
  gst_harness_push (h, create_rtp_vp8_buffer (1003, 12, BT_TS_PICID_15, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 12, TRUE, TRUE, 1, 12);
  gst_buffer_unref (buffer);

  /* Push temporally scaled packet (7bit picid, no tl0picidx) */
  pts += 33 * GST_MSECOND;
  gst_harness_push (h, create_rtp_vp8_buffer (1004, 13, BT_TS_PICID_7_NO_TLOPICIDX, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 13, TRUE, TRUE, 1, 0);
  gst_buffer_unref (buffer);

  /* Push temporally scaled packet (7bit picid, no tid/y/keyidx) */
  pts += 33 * GST_MSECOND;
  gst_harness_push (h, create_rtp_vp8_buffer (1005, 14, BT_TS_PICID_7_NO_TID_Y_KEYIDX, pts));
  buffer = gst_harness_pull (h);
  verify_meta (buffer, 14, TRUE, FALSE, 0, 0);
  gst_buffer_unref (buffer);

  gst_harness_teardown (h);
}
GST_END_TEST;

/* PictureID emum is not exported */
enum PictureID {
  VP8_PAY_NO_PICTURE_ID = 0,
  VP8_PAY_PICTURE_ID_7BITS = 1,
  VP8_PAY_PICTURE_ID_15BITS = 2,
};

static const struct no_meta_test_data {
  /* control inputs */
  enum PictureID pid; /* picture ID type of test */
  gboolean vp8_payload_header_m_flag;

  /* expected outputs */
  guint vp8_payload_header_size;
  guint vp8_payload_control_value;
} no_meta_test_data[] = {
  { VP8_PAY_NO_PICTURE_ID,     FALSE , 1, 0x10}, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE,  3, 0x90 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,   4, 0x90 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */

  /* repeated with non reference frame */
  { VP8_PAY_NO_PICTURE_ID,     FALSE , 1, 0x30}, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE,  3, 0xB0 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,   4, 0xB0 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */
};

GST_START_TEST (test_pay_no_meta)
{
  guint8 vp8_bitstream_payload[] = {
    0x30, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00, 0x90, 0x00, 0x06, 0x47,
    0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21, 0x00
  };
  const struct no_meta_test_data *test_data = &no_meta_test_data[__i__];
  GstBuffer *buffer, *outbuffer;
  GstMapInfo map = GST_MAP_INFO_INIT;
  GstHarness *h = gst_harness_new ("rtpvp8pay");
  gst_harness_set_src_caps_str (h, "video/x-vp8");

  /* unknown picture id enum value */
  fail_unless (test_data->pid <= VP8_PAY_PICTURE_ID_15BITS);

  g_object_set (h->element, "picture-id-mode", test_data->pid, NULL);

  /* Push a buffer in */
  buffer = gst_buffer_new_wrapped (g_memdup (vp8_bitstream_payload, sizeof (vp8_bitstream_payload)), sizeof (vp8_bitstream_payload));

  /* set droppable if N flag set */
  if ((test_data->vp8_payload_control_value & 0x20) != 0) {
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DROPPABLE);
  }

  gst_harness_push (h, buffer);

  /* Pull output buffer and verify the VP8 header is sane */
  outbuffer = gst_harness_pull (h);
  fail_unless (gst_buffer_map (outbuffer, &map, GST_MAP_READ));
  fail_unless (map.data != NULL);

  /* check buffer size and content */
  fail_unless_equals_int (12 + test_data->vp8_payload_header_size + sizeof (vp8_bitstream_payload), map.size); /* RTP + VP8 + VP8 bistream Payload */

  fail_unless_equals_int (test_data->vp8_payload_control_value, map.data[12]);

  if (test_data->vp8_payload_header_size > 2) {
    /* vp8 header extension byte must have I set */
    fail_unless_equals_int (0x80, map.data[13]);
    if (test_data->vp8_payload_header_m_flag) {
      fail_unless_equals_int (0x80, (map.data[14] & 0x80));
    } else {
      fail_unless_equals_int (0x00, (map.data[14] & 0x80));
    }
  }

  gst_buffer_unmap (outbuffer, &map);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}
GST_END_TEST;

static const struct with_meta_test_data {
  /* control inputs */
  enum PictureID pid; /* picture ID type of test */
  gboolean vp8_payload_header_m_flag;
  gboolean use_temporal_scaling;
  gboolean y_flag;

  /* expected outputs */
  guint vp8_payload_header_size;
  guint vp8_payload_control_value;
  guint vp8_payload_extended_value;
} with_meta_test_data[] = {
  { VP8_PAY_NO_PICTURE_ID,     FALSE, FALSE, FALSE, 1, 0x10, 0x80 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, FALSE, FALSE, 3, 0x90, 0x80 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  FALSE, FALSE, 4, 0x90, 0x80 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */
  { VP8_PAY_NO_PICTURE_ID,     FALSE, TRUE,  FALSE, 4, 0x90, 0x60 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, TRUE,  FALSE, 5, 0x90, 0xE0 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  TRUE,  FALSE, 6, 0x90, 0xE0 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */
  { VP8_PAY_NO_PICTURE_ID,     FALSE, TRUE,  TRUE,  4, 0x90, 0x60 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, TRUE,  TRUE,  5, 0x90, 0xE0 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  TRUE,  TRUE,  6, 0x90, 0xE0 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */

  /* repeated with non reference frame */
  { VP8_PAY_NO_PICTURE_ID,     FALSE, FALSE, FALSE, 1, 0x30, 0x80 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, FALSE, FALSE, 3, 0xB0, 0x80 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  FALSE, FALSE, 4, 0xB0, 0x80 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */
  { VP8_PAY_NO_PICTURE_ID,     FALSE, TRUE,  FALSE, 4, 0xB0, 0x60 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, TRUE,  FALSE, 5, 0xB0, 0xE0 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  TRUE,  FALSE, 6, 0xB0, 0xE0 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */
  { VP8_PAY_NO_PICTURE_ID,     FALSE, TRUE,  TRUE,  4, 0xB0, 0x60 }, /* no picture ID single byte header, S set */
  { VP8_PAY_PICTURE_ID_7BITS,  FALSE, TRUE,  TRUE,  5, 0xB0, 0xE0 }, /* X bit to allow for I bit means header is three bytes, S and X set */
  { VP8_PAY_PICTURE_ID_15BITS, TRUE,  TRUE,  TRUE,  6, 0xB0, 0xE0 }, /* X bit to allow for I bit with M bit means header is four bytes, S, X and M set */

};

GST_START_TEST (test_pay_with_meta)
{
  guint8 vp8_bitstream_payload[] = {
    0x30, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00, 0x90, 0x00, 0x06, 0x47,
    0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21, 0x00
  };
  const struct with_meta_test_data *test_data = &with_meta_test_data[__i__];
  GstBuffer *buffer, *outbuffer;
  GstVideoVP8Meta *meta;
  GstMapInfo map = GST_MAP_INFO_INIT;
  GstHarness *h = gst_harness_new ("rtpvp8pay");
  gst_harness_set_src_caps_str (h, "video/x-vp8");

  /* check for unknown picture id enum value */
  fail_unless (test_data->pid <= VP8_PAY_PICTURE_ID_15BITS);

  g_object_set (h->element, "picture-id-mode", test_data->pid, NULL);

  /* Push a buffer in */
  buffer = gst_buffer_new_wrapped (g_memdup (vp8_bitstream_payload, sizeof (vp8_bitstream_payload)), sizeof (vp8_bitstream_payload));
  gst_buffer_add_video_vp8_meta_full (buffer,
      0x5A5A, /* picture ID */
      test_data->use_temporal_scaling,
      test_data->y_flag,  /* Y bit */
      2,  /* set temporal layer ID */
      0xA5); /* tl0picidx */

  /* set droppable if N flag set */
  if ((test_data->vp8_payload_control_value & 0x20) != 0) {
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DROPPABLE);
  }

  gst_harness_push (h, buffer);

  /* Pull output buffer, and verify the VP8 header is sane */
  outbuffer = gst_harness_pull (h);
  fail_unless (gst_buffer_map (outbuffer, &map, GST_MAP_READ));
  fail_unless (map.data != NULL);

  meta = gst_buffer_get_video_vp8_meta (outbuffer);
  fail_unless (meta == NULL);

  /* check buffer size and content */
  fail_unless_equals_int (12 + test_data->vp8_payload_header_size + sizeof (vp8_bitstream_payload), map.size); /* RTP + VP8 + VP8 bistream Payload */

  fail_unless_equals_int (test_data->vp8_payload_control_value, map.data[12]);

  if (test_data->vp8_payload_header_size > 1) {
    int hdridx = 13;
    fail_unless_equals_int (test_data->vp8_payload_extended_value, map.data[hdridx++]);

    /* check picture ID */
    if (test_data->pid == VP8_PAY_PICTURE_ID_7BITS) {
      fail_unless_equals_int (0x5A, map.data[hdridx++]);
    } else if (test_data->pid == VP8_PAY_PICTURE_ID_15BITS) {
      fail_unless_equals_int (0xDA, map.data[hdridx++]);
      fail_unless_equals_int (0x5A, map.data[hdridx++]);
    }

    if (test_data->use_temporal_scaling) {
      /* check temporal layer 0 picture ID value */
      fail_unless_equals_int (0xA5, map.data[hdridx++]);
      /* check temporal layer ID value */
      fail_unless_equals_int (2, (map.data[hdridx] >> 6) & 0x3);

      if (test_data->y_flag) {
	fail_unless_equals_int (1, (map.data[hdridx] >> 5) & 1);
      } else {
	fail_unless_equals_int (0, (map.data[hdridx] >> 5) & 1);
      }
    }
  }

  gst_buffer_unmap (outbuffer, &map);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}
GST_END_TEST;

GST_START_TEST (test_pay_meta_no_picid)
{
  guint8 vp8_bitstream_payload[] = {
    0x30, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00, 0x90, 0x00, 0x06, 0x47,
    0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21, 0x00
  };
  gint i, last_picid = 0;
  GstBuffer *buffer, *outbuffer;
  GstHarness *h = gst_harness_new ("rtpvp8pay");
  g_object_set (h->element, "picture-id-mode", VP8_PAY_PICTURE_ID_15BITS, NULL);
  gst_harness_set_src_caps_str (h, "video/x-vp8");

  buffer = gst_buffer_new_wrapped (g_memdup (vp8_bitstream_payload,
      sizeof (vp8_bitstream_payload)),
      sizeof (vp8_bitstream_payload));

  /* Add meta indicating missing picture ID */
  gst_buffer_add_video_vp8_meta_full (buffer, 0xFFFF, FALSE, FALSE, 0, 0);

  for (i = 0; i < 2; i++) {
    GstMapInfo map = GST_MAP_INFO_INIT;

    gst_harness_push (h, gst_buffer_ref (buffer));

    outbuffer = gst_harness_pull (h);
    fail_unless (gst_buffer_map (outbuffer, &map, GST_MAP_READ));
    fail_unless (map.data != NULL);

    /* check buffer size and content */
    fail_unless_equals_int (16 + sizeof (vp8_bitstream_payload), map.size);

    fail_unless_equals_int (0x90, map.data[12]);
    fail_unless_equals_int (0x80, map.data[13]);

    /* Initial picture ID is random, so collect the first one
     * and assert that the second one is correct. */
    if (i == 0) {
      last_picid = ((map.data[14] << 8) | map.data[15]) & 0x7fff;
    } else {
      gint expected = last_picid + 1;
      if (last_picid == 0x7fff)
        expected = 0;

      fail_unless_equals_int (expected,
          ((map.data[14] << 8) | map.data[15]) & 0x7fff);
    }

    gst_buffer_unmap (outbuffer, &map);
    gst_buffer_unref (outbuffer);
  }

  gst_buffer_unref (buffer);

  gst_harness_teardown (h);
}
GST_END_TEST;

static Suite *
rtpvp8_suite (void)
{
  Suite *s = suite_create ("rtpvp8");
  TCase *tc_chain;

  suite_add_tcase (s, (tc_chain = tcase_create ("vp8depay")));
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events, 0, G_N_ELEMENTS (stop_gap_events_test_data));
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_event, 0, G_N_ELEMENTS (resend_gap_event_test_data));
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_event_nopicid, 0, G_N_ELEMENTS (resend_gap_event_nopicid_test_data));
  tcase_add_loop_test (tc_chain, test_depay_create_gap_event_on_picid_gaps, 0, G_N_ELEMENTS (create_gap_event_on_picid_gaps_test_data));
  tcase_add_loop_test (tc_chain, test_depay_nopicid, 0, G_N_ELEMENTS (nopicid_test_data));
  tcase_add_test (tc_chain, test_depay_temporally_scaled);

  suite_add_tcase (s, (tc_chain = tcase_create ("vp8pay")));
  tcase_add_loop_test (tc_chain, test_pay_no_meta, 0 , G_N_ELEMENTS(no_meta_test_data));
  tcase_add_loop_test (tc_chain, test_pay_with_meta, 0 , G_N_ELEMENTS(with_meta_test_data));
  tcase_add_test (tc_chain, test_pay_meta_no_picid);

  return s;
}

GST_CHECK_MAIN (rtpvp8);
