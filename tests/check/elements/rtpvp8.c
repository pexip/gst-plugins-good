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

#define RTP_VP8_CAPS_STR \
  "application/x-rtp,media=video,encoding-name=VP8,clock-rate=90000,payload=96"

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

static GstBuffer *
create_rtp_vp8_buffer (guint seqnum, guint picid, gint picid_bits,
    GstClockTime buf_pts)
{
  GstBuffer *ret;
  guint8 *packet;
  gsize size;

  g_assert (picid_bits == 0 || picid_bits == 7 || picid_bits == 15);

  if (picid_bits == 0) {
    size = sizeof (intra_nopicid_seqnum0);
    packet = g_memdup (intra_nopicid_seqnum0, size);
  } else if (picid_bits == 7) {
    size = sizeof (intra_picid24_seqnum0);
    packet = g_memdup (intra_picid24_seqnum0, size);
    packet[14] = picid & 0x7f;
  } else {
    size = sizeof (intra_picid6336_seqnum0);
    packet = g_memdup (intra_picid6336_seqnum0, size);
    packet[14] = ((picid >> 8) & 0xff) | 0x80;
    packet[15] = (picid >> 0) & 0xff;
  }

  packet[2] = (seqnum >> 8) & 0xff;
  packet[3] = (seqnum >> 0) & 0xff;

  ret = gst_buffer_new_wrapped (packet, size);
  GST_BUFFER_PTS (ret) = buf_pts;
  return ret;
}

typedef struct _DepayGapEventTestData
{
  gint seq_num;
  gint picid;
  gint picid_bits;
} DepayGapEventTestData;

static void
test_depay_gap_event_base (const DepayGapEventTestData *data,
    gboolean send_lost_event, gboolean expect_gap_event)
{
  GstEvent *event;
  GstClockTime pts = 0;
  GstHarness *h = gst_harness_new ("rtpvp8depay");
  gst_harness_set_src_caps_str (h, RTP_VP8_CAPS_STR);

  gst_harness_push (h, create_rtp_vp8_buffer (data[0].seq_num, data[0].picid, data[0].picid_bits, pts));
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

  gst_harness_push (h, create_rtp_vp8_buffer (data[1].seq_num, data[1].picid, data[1].picid_bits, pts));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  if (expect_gap_event) {
    // Making shure the GAP event was pushed downstream
    event = gst_harness_pull_event (h);
    fail_unless_equals_string ("gap", gst_event_type_get_name (GST_EVENT_TYPE (event)));
    gst_event_unref (event);
  }
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  gst_harness_teardown (h);
}

// Packet loss + no loss in picture ids
static const DepayGapEventTestData stop_gap_events_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, 7}, {102, 25, 7}},

  // 15bit picture ids
  {{100, 250, 15}, {102, 251, 15}},

  // 7bit picture ids wrap
  {{100, 127, 7}, {102, 0, 7}},

  // 15bit picture ids wrap
  {{100, 32767, 15}, {102, 0, 15}},

  // 7bit to 15bit picture id
  {{100, 127, 7}, {102, 128, 15}},
};

GST_START_TEST (test_depay_stop_gap_events)
{
  test_depay_gap_event_base (&stop_gap_events_test_data[__i__][0], TRUE, FALSE);
}
GST_END_TEST;

// Packet loss + lost picture ids
static const DepayGapEventTestData resend_gap_event_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, 7}, {102, 26, 7}},

  // 15bit picture ids
  {{100, 250, 15}, {102, 252, 15}},

  // 7bit picture ids wrap
  {{100, 127, 7}, {102, 1, 7}},

  // 15bit picture ids wrap
  {{100, 32767, 15}, {102, 1, 15}},

  // 7bit to 15bit picture id
  {{100, 126, 7}, {102, 129, 15}},
};

GST_START_TEST (test_depay_resend_gap_event)
{
  test_depay_gap_event_base (&resend_gap_event_test_data[__i__][0], TRUE, TRUE);
}
GST_END_TEST;

// Packet loss + one of picture ids does not exist
static const DepayGapEventTestData resend_gap_event_nopicid_test_data[][2] = {
  {{100, 24, 7}, {102, 0, 0}},
  {{100, 0, 0}, {102, 26, 7}},
  {{100, 0, 0}, {102, 0, 0}},
};

GST_START_TEST (test_depay_resend_gap_event_nopicid)
{
  test_depay_gap_event_base (&resend_gap_event_nopicid_test_data[__i__][0], TRUE, TRUE);
}
GST_END_TEST;

// No packet loss + lost picture ids
static const DepayGapEventTestData create_gap_event_on_picid_gaps_test_data[][2] = {
  // 7bit picture ids
  {{100, 24, 7}, {101, 26, 7}},

  // 15bit picture ids
  {{100, 250, 15}, {101, 252, 15}},

  // 7bit picture ids wrap
  {{100, 127, 7}, {101, 1, 7}},

  // 15bit picture ids wrap
  {{100, 32767, 15}, {101, 1, 15}},

  // 7bit to 15bit picture id
  {{100, 126, 7}, {101, 129, 15}},
};

GST_START_TEST (test_depay_create_gap_event_on_picid_gaps)
{
  test_depay_gap_event_base (&create_gap_event_on_picid_gaps_test_data[__i__][0], FALSE, TRUE);
}
GST_END_TEST;

// No packet loss + one of picture ids does not exist
static const DepayGapEventTestData nopicid_test_data[][2] = {
  {{100, 24, 7}, {101, 0, 0}},
  {{100, 0, 0}, {101, 26, 7}},
  {{100, 0, 0}, {101, 0, 0}},
};

GST_START_TEST (test_depay_nopicid)
{
  test_depay_gap_event_base (&nopicid_test_data[__i__][0], FALSE, FALSE);
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

  return s;
}

GST_CHECK_MAIN (rtpvp8);