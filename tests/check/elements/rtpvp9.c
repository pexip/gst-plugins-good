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

#define RTP_VP9_CAPS_STR \
  "application/x-rtp,media=video,encoding-name=VP9,clock-rate=90000,payload=96"

GST_START_TEST (test_depay_flexible_mode)
{
  /* b-bit, e-bit, f-bit and marker bit set */
  /* First packet of first frame, handcrafted to also set the e-bit and marker
   * bit in addition to changing the seqnum */
  guint8 intra[] = {
    0x80, 0xf4, 0x00, 0x00, 0x49, 0xb5, 0xbe, 0x32, 0xb1, 0x01, 0x64, 0xd1,
    0xbc, 0x98, 0xbf, 0x00, 0x83, 0x49, 0x83, 0x42, 0x00, 0x77, 0xf0, 0x43,
    0x71, 0xd8, 0xe0, 0x90, 0x70, 0x66, 0x80, 0x60, 0x0e, 0xf0, 0x5f, 0xfd,
  };
  /* b-bit, e-bit, p-bit, f-bit and marker bit set */
  /* First packet of second frame, handcrafted to also set the e-bit and
   * marker bit in addition to changing the seqnum */
  guint8 inter[] = {
    0x80, 0xf4, 0x00, 0x01, 0x49, 0xb6, 0x02, 0xc0, 0xb1, 0x01, 0x64, 0xd1,
    0xfc, 0x98, 0xc0, 0x00, 0x02, 0x87, 0x01, 0x00, 0x09, 0x3f, 0x1c, 0x12,
    0x0e, 0x0c, 0xd0, 0x1b, 0xa7, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xda, 0x11,
  };

  GstHarness *h = gst_harness_new ("rtpvp9depay");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          intra, sizeof (intra), 0, sizeof (intra), NULL, NULL));
  fail_unless_equals_int (1, gst_harness_buffers_received (h));

  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          inter, sizeof (inter), 0, sizeof (inter), NULL, NULL));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_depay_non_flexible_mode)
{
  /* b-bit, e-bit and  marker bit set. f-bit NOT set */
  /* First packet of first frame, handcrafted to also set the e-bit and marker
   * bit in addition to changing the seqnum */
  guint8 intra[] = {
    0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
    0x8c, 0x98, 0xc0, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
    0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
  };
  /* b-bit, e-bit, p-bit  and marker bit set. f-bit NOT set */
  /* First packet of second frame, handcrafted to also set the e-bit and
   * marker bit in addition to changing the seqnum */
  guint8 inter[] = {
    0x80, 0xf4, 0x00, 0x01, 0x49, 0x88, 0xe5, 0x38, 0xa0, 0x6c, 0x65, 0x6c,
    0xcc, 0x98, 0xc1, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
    0xd0, 0x1b, 0x97, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0x8a, 0x9f, 0x01, 0xbc
  };

  GstHarness *h = gst_harness_new ("rtpvp9depay");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          intra, sizeof (intra), 0, sizeof (intra), NULL, NULL));
  fail_unless_equals_int (1, gst_harness_buffers_received (h));

  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          inter, sizeof (inter), 0, sizeof (inter), NULL, NULL));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  gst_harness_teardown (h);
}

GST_END_TEST;

static guint8 intra_picid6336_seqnum0[] = {
  0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
  0x8c, 0x98, 0xc0, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
};
static guint8 intra_picid24_seqnum0[] = {
  0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
  0x8c, 0x18, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
};
static guint8 intra_nopicid_seqnum0[] = {
  0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
  0x0c, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
};

enum {
    BT_PLAIN_PICID_NONE,
    BT_PLAIN_PICID_7,
    BT_PLAIN_PICID_15,
  /* Commented out for now, until added VP9 equvivalents.
    BT_TS_PICID_NONE,
    BT_TS_PICID_7,
    BT_TS_PICID_15,
    BT_TS_PICID_7_NO_TLOPICIDX,
    BT_TS_PICID_7_NO_TID_Y_KEYIDX
    */
};

static GstBuffer *
create_rtp_vp9_buffer_full (guint seqnum, guint picid, guint buffer_type,
    GstClockTime buf_pts, gboolean B_bit_start_of_frame, gboolean marker_bit)
{
  static struct BufferTemplate {
    guint8 * template;
    gsize size;
    gint picid_bits;
  } templates[] = {
    { intra_nopicid_seqnum0, sizeof (intra_nopicid_seqnum0), 0 },
    { intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0), 7 },
    { intra_picid6336_seqnum0, sizeof (intra_picid6336_seqnum0), 15 },
    /*
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
    */
  };
  struct BufferTemplate *template = &templates[buffer_type];
  guint8 *packet = g_memdup (template->template, template->size);
  GstBuffer *ret;

  packet[2] = (seqnum >> 8) & 0xff;
  packet[3] = (seqnum >> 0) & 0xff;

  /* We're forcing the E-bit (EndOfFrame) together with the RTP marker bit here, which is a bit of a hack.
   * If we're to enable spatial scalability tests, we need to take that into account when setting the E bit.
   */
  if (marker_bit){
    packet[1] |= 0x80;
    packet[12] |= 0x4;
  } else {
    packet[1] &= ~0x80;
    packet[12] &= ~0x4;
  }

  if (B_bit_start_of_frame)
    packet[12] |= 0x8;
  else
    packet[12] &= ~0x8;

  if (template->picid_bits == 7) {
    /* Prerequisites for this to be correct:
    ((packet[12] & 0x80) == 0x80); //I bit set
    */
    g_assert((packet[12] & 0x80) == 0x80);
    packet[13] = picid & 0x7f;

  } else if (template->picid_bits == 15) {
    /* Prerequisites for this to be correct:
    ((packet[12] & 0x80) == 0x80); //I bit set
    */
    g_assert((packet[12] & 0x80) == 0x80);
    packet[13] = ((picid >> 8) & 0xff) | 0x80;
    packet[14] = (picid >> 0) & 0xff;
  }

  ret = gst_buffer_new_wrapped (packet, template->size);
  GST_BUFFER_PTS (ret) = buf_pts;
  return ret;
}


static GstBuffer *
create_rtp_vp9_buffer (guint seqnum, guint picid, guint buffer_type,
    GstClockTime buf_pts)
{
  return create_rtp_vp9_buffer_full (seqnum, picid, buffer_type, buf_pts, TRUE,
      TRUE);
}

typedef struct _DepayGapEventTestData
{
  gint seq_num;
  gint picid;
  guint buffer_type;
} DepayGapEventTestData;

typedef struct
{
  gint seq_num;
  gint picid;
  guint buffer_type;
  gboolean s_bit;
  gboolean marker_bit;
} DepayGapEventTestDataFull;

static void
test_depay_gap_event_base (const DepayGapEventTestData *data,
    gboolean send_lost_event, gboolean expect_gap_event, int iter)
{
  GstEvent *event;
  GstClockTime pts = 0;
  GstHarness *h = gst_harness_new ("rtpvp9depay");
  if (send_lost_event == FALSE && expect_gap_event) {
    /* Expect picture ID gaps to be concealed, so tell the element to do so. */
    g_object_set (h->element, "hide-picture-id-gap", TRUE, NULL);
  }
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  gst_harness_push (h, create_rtp_vp9_buffer (data[0].seq_num, data[0].picid, data[0].buffer_type, pts));
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

  gst_harness_push (h, create_rtp_vp9_buffer (data[1].seq_num, data[1].picid, data[1].buffer_type, pts));
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
  test_depay_gap_event_base (&stop_gap_events_test_data[__i__][0], TRUE, FALSE, __i__);
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
  test_depay_gap_event_base (&resend_gap_event_test_data[__i__][0], TRUE, TRUE, __i__);
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
  test_depay_gap_event_base (&resend_gap_event_nopicid_test_data[__i__][0], TRUE, TRUE, __i__);
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
  test_depay_gap_event_base (&create_gap_event_on_picid_gaps_test_data[__i__][0], FALSE, TRUE, __i__);
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
  test_depay_gap_event_base (&nopicid_test_data[__i__][0], FALSE, FALSE, __i__);
}
GST_END_TEST;


GST_START_TEST (test_depay_send_gap_event_when_marker_bit_missing_and_picid_gap)
{
  gboolean send_lost_event = __i__ == 0;
  GstClockTime pts = 0;
  gint seqnum = 100;
  gint i;
  gboolean noloss = FALSE;
  GstEvent *event;

  GstHarness *h = gst_harness_new_parse ("rtpvp9depay hide-picture-id-gap=TRUE");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  /* Push a complete frame to avoid depayloader to suppress gap events */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 23,
              BT_PLAIN_PICID_7, pts, TRUE, TRUE)));
  pts += 33 * GST_MSECOND;
  seqnum++;

  for (i = 0; i < 3; i++)
    gst_event_unref (gst_harness_pull_event (h));
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  /* Push packet with start bit set, but no marker bit */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 24,
              BT_PLAIN_PICID_7, pts, TRUE, FALSE)));
  pts += 33 * GST_MSECOND;
  seqnum++;

  if (send_lost_event) {
    gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
            gst_structure_new ("GstRTPPacketLost",
                "timestamp", G_TYPE_UINT64, pts,
                "duration", G_TYPE_UINT64, 33*GST_MSECOND, NULL)));
    pts += 33 * GST_MSECOND;
    seqnum++;
  }

  /* Push packet with gap in picid */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 26,
              BT_PLAIN_PICID_7, pts, TRUE, TRUE)));

  /* Expect only 2 output frames since the one frame was incomplete */
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  /* There should be a gap event, either triggered by the loss or the picid
   * gap */

  /* Making shure the GAP event was pushed downstream */
  event = gst_harness_pull_event (h);
  fail_unless_equals_string ("gap", gst_event_type_get_name (GST_EVENT_TYPE (event)));
  gst_structure_get_boolean (gst_event_get_structure (event), "no-packet-loss", &noloss);

  /* If we didn't send GstRTPPacketLost event, the gap event should indicate
   * that with 'no-packet-loss' parameter */
  fail_unless_equals_int (noloss, !send_lost_event);
  gst_event_unref (event);

  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  gst_harness_teardown (h);
}
GST_END_TEST;

GST_START_TEST (test_depay_send_gap_event_when_marker_bit_missing_and_no_picid_gap)
{
  GstClockTime pts = 0;
  gint seqnum = 100;
  gint i;
  gboolean noloss = FALSE;
  GstEvent *event;

  GstHarness *h = gst_harness_new_parse ("rtpvp9depay hide-picture-id-gap=TRUE");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  /* Push a complete frame to avoid depayloader to suppress gap events */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 23,
              BT_PLAIN_PICID_7, pts, TRUE, TRUE)));
  pts += 33 * GST_MSECOND;
  seqnum++;

  for (i = 0; i < 3; i++)
    gst_event_unref (gst_harness_pull_event (h));
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  /* Push packet with start bit set, but no marker bit */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 24,
              BT_PLAIN_PICID_7, pts, TRUE, FALSE)));
  pts += 33 * GST_MSECOND;
  seqnum++;

  /* Push packet for next picid, without having sent a packet with marker bit
   * foor the previous picid */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (seqnum, 25,
              BT_PLAIN_PICID_7, pts, TRUE, TRUE)));

  /* Expect only 2 output frames since one was incomplete */
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  /* Making sure the GAP event was pushed downstream */
  event = gst_harness_pull_event (h);
  fail_unless_equals_string ("gap", gst_event_type_get_name (GST_EVENT_TYPE (event)));
  gst_structure_get_boolean (gst_event_get_structure (event), "no-packet-loss", &noloss);

  /* The gap event should indiciate that there is information missing, but
   * no actual packet loss. */
  fail_unless_equals_int (noloss, TRUE);
  gst_event_unref (event);

  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  gst_harness_teardown (h);
}
GST_END_TEST;

GST_START_TEST (test_depay_no_gap_event_when_partial_frames_with_no_picid_gap)
{
  gint i;
  GstClockTime pts = 0;
  GstHarness *h = gst_harness_new ("rtpvp9depay");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  /* start with complete frame to make sure depayloader will not drop
   * potential gap events */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (100, 24,
              BT_PLAIN_PICID_7, pts, TRUE, TRUE)));
  fail_unless_equals_int (1, gst_harness_buffers_received (h));

   /* drop setup events to more easily check for gap events */
  for (i = 0; i < 3; i++)
    gst_event_unref (gst_harness_pull_event (h));
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  /* Next frame is split in two packets */
  pts += 33 * GST_MSECOND;
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (101, 25,
              BT_PLAIN_PICID_7, pts, TRUE, FALSE)));
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h, create_rtp_vp9_buffer_full (102, 25,
              BT_PLAIN_PICID_7, pts, FALSE, TRUE)));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  /* there must be no gap events */
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

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

  /* expected outputs */
  guint vp9_payload_header_size;
  guint vp9_payload_control_value;
} no_meta_test_data[] = {
  { VP8_PAY_NO_PICTURE_ID,     1, 0x0e}, /* no picture ID single byte header, B&E set. We expect SS (v) to be set */
  { VP8_PAY_PICTURE_ID_7BITS,  2, 0x8e }, /* I bit set for Pid_ID, B&E set. We expect SS (v) to be set, since framework sets it.  */
  { VP8_PAY_PICTURE_ID_15BITS, 3, 0x8e }, /* I bit set for Pid_ID, B&E set. We expect SS (v) to be set, since framework sets it. */

};

GST_START_TEST (test_pay_no_meta)
{

  guint8 vp9_bitstream_payload[] = {
    0x82, 0x49, 0x83, 0x42, 0x00, 0x03, 0xf0, 0x03, 0xf6, 0x08, 0x38, 0x24,
    0x1c, 0x18, 0x54, 0x00, 0x00, 0x20, 0x40, 0x00, 0x13, 0xbf, 0xff, 0xf8,
    0x65, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f,
    0xff, 0xff, 0xff, 0xf8, 0x65, 0xdc, 0x3a, 0xff, 0xff, 0xff, 0xf0, 0xcb,
    0xb8, 0x00, 0x00
  };
  const struct no_meta_test_data *test_data = &no_meta_test_data[__i__];
  GstBuffer *buffer;
  GstMapInfo map = GST_MAP_INFO_INIT;
  GstHarness *h = gst_harness_new ("rtpvp9pay");
  gst_harness_set_src_caps_str (h, "video/x-vp9");

  /* check unknown picture id enum value */
  fail_unless (test_data->pid <= VP8_PAY_PICTURE_ID_15BITS);

  g_object_set (h->element, "picture-id-mode", test_data->pid,
      "picture-id-offset", 0x5A5A, NULL);

  buffer = gst_buffer_new_wrapped (g_memdup (vp9_bitstream_payload,
          sizeof (vp9_bitstream_payload)), sizeof (vp9_bitstream_payload));

  buffer = gst_harness_push_and_pull (h, buffer);

  fail_unless (gst_buffer_map (buffer, &map, GST_MAP_READ));
  fail_unless (map.data != NULL);

  /* check buffer size and content */
  /* The VP9 rtp buffer generator autmatically adds Scalability structure information
   * in  gst_rtp_vp9_create_header_buffer, if frame is keyframe and start bit is set, so adjust for that (8 bytes).
   */
  fail_unless_equals_int (map.size,
      12 + test_data->vp9_payload_header_size + 8 + sizeof (vp9_bitstream_payload));

  fail_unless_equals_int (test_data->vp9_payload_control_value, map.data[12]);

  if (test_data->vp9_payload_header_size > 1) {
    /* vp9 header extension byte must have I set */
    fail_unless_equals_int (0x80, map.data[12] & 0x80);
    /* check picture id */
    if (test_data->pid == VP8_PAY_PICTURE_ID_7BITS)  {
      fail_unless_equals_int (0x5a, map.data[13]);
    } else if (test_data->pid == VP8_PAY_PICTURE_ID_15BITS) {
      fail_unless_equals_int (0xDA, map.data[13]);
      fail_unless_equals_int (0x5A, map.data[14]);
    }
  }

  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  gst_harness_teardown (h);
}
GST_END_TEST;


static Suite *
rtpvp9_suite (void)
{
  Suite *s = suite_create ("rtpvp9");
  TCase *tc_chain;
  suite_add_tcase (s, (tc_chain = tcase_create ("vp9depay")));
  tcase_add_test (tc_chain, test_depay_flexible_mode);
  tcase_add_test (tc_chain, test_depay_non_flexible_mode);
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events, 4, 5);
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events, 0, G_N_ELEMENTS (stop_gap_events_test_data));

  tcase_add_loop_test (tc_chain, test_depay_resend_gap_event, 0, G_N_ELEMENTS (resend_gap_event_test_data));
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_event_nopicid, 0, G_N_ELEMENTS (resend_gap_event_nopicid_test_data));
  tcase_add_loop_test (tc_chain, test_depay_create_gap_event_on_picid_gaps, 0, G_N_ELEMENTS (create_gap_event_on_picid_gaps_test_data));
  tcase_add_loop_test (tc_chain, test_depay_nopicid, 0, G_N_ELEMENTS (nopicid_test_data));
  tcase_add_loop_test (tc_chain, test_depay_send_gap_event_when_marker_bit_missing_and_picid_gap, 0, 2);
  tcase_add_test (tc_chain, test_depay_send_gap_event_when_marker_bit_missing_and_no_picid_gap);
  tcase_add_test (tc_chain, test_depay_no_gap_event_when_partial_frames_with_no_picid_gap);
  /*TODO: add tests for scalability options */

  suite_add_tcase (s, (tc_chain = tcase_create ("vp9pay")));
  tcase_add_loop_test (tc_chain, test_pay_no_meta, 0 , G_N_ELEMENTS(no_meta_test_data));
  /*TODO: add tests for scalability options */

  return s;
}

GST_CHECK_MAIN (rtpvp9);
