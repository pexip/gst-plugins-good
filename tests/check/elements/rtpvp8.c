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

static guint8 inter_picid6337_seqnum2[] = {
  0x80, 0xe0, 0x00, 0x02, 0xe9, 0x30, 0xa3, 0x66, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x80, 0x98, 0xc1, 0x31, 0x02, 0x00, 0x19, 0x11, 0xbc, 0x00, 0x18,
  0x00, 0x18, 0x58, 0x2f, 0xf4, 0x00, 0x08, 0x80, 0x43, 0x98, 0x06, 0x00,
};

static guint8 intra_picid24_seqnum0[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x80, 0x18, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};

static guint8 inter_picid25_seqnum2[] = {
  0x80, 0xe0, 0x00, 0x02, 0xe9, 0x30, 0xa3, 0x66, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x80, 0x19, 0x31, 0x02, 0x00, 0x19, 0x11, 0xbc, 0x00, 0x18,
  0x00, 0x18, 0x58, 0x2f, 0xf4, 0x00, 0x08, 0x80, 0x43, 0x98, 0x06, 0x00,
};

static guint8 intra_nopicid_seqnum0[] = {
  0x80, 0xe0, 0x00, 0x00, 0x9a, 0xbb, 0xe3, 0xb3, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x00, 0xf0, 0x07, 0x00, 0x9d, 0x01, 0x2a, 0xb0, 0x00,
  0x90, 0x00, 0x06, 0x47, 0x08, 0x85, 0x85, 0x88, 0x99, 0x84, 0x88, 0x21,
};

static guint8 inter_nopicid_seqnum2[] = {
  0x80, 0xe0, 0x00, 0x02, 0xe9, 0x30, 0xa3, 0x66, 0x8b, 0xe9, 0x1d, 0x61,
  0x90, 0x00, 0x31, 0x02, 0x00, 0x19, 0x11, 0xbc, 0x00, 0x18,
  0x00, 0x18, 0x58, 0x2f, 0xf4, 0x00, 0x08, 0x80, 0x43, 0x98, 0x06, 0x00,
};

typedef struct _stop_gap_events_test_data_t
{
  guint8 *buf;
  gsize bufsize;
} stop_gap_events_test_data_t;

stop_gap_events_test_data_t test_data_no_gap_in_picid[2][2] = {
  // Pictureid without M bit
  {
        {intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0)}
        ,
        {inter_picid25_seqnum2, sizeof (inter_picid25_seqnum2)}
      }
  ,
  // Pictureid with M bit
  {
        {intra_picid6336_seqnum0, sizeof (intra_picid6336_seqnum0)}
        ,
        {inter_picid6337_seqnum2, sizeof (inter_picid6337_seqnum2)}
      }
  ,
};

stop_gap_events_test_data_t test_data_nopicid[3][2] = {
  {
        {intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0)}
        ,
        {inter_nopicid_seqnum2, sizeof (inter_nopicid_seqnum2)}
      }
  ,
  {
        {intra_nopicid_seqnum0, sizeof (intra_nopicid_seqnum0)}
        ,
        {inter_picid25_seqnum2, sizeof (inter_picid25_seqnum2)}
      }
  ,
  {
        {intra_nopicid_seqnum0, sizeof (intra_nopicid_seqnum0)}
        ,
        {inter_nopicid_seqnum2, sizeof (inter_nopicid_seqnum2)}
      }
  ,
};

static void
test_depay_stop_gap_events_base (stop_gap_events_test_data_t * test_data)
{
  GstEvent *event;
  GstHarness *h = gst_harness_new ("rtpvp8depay");
  gst_harness_set_src_caps_str (h, RTP_VP8_CAPS_STR);

  // The fist buffer has seq_num = 0
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[0].buf, test_data[0].bufsize, 0, test_data[0].bufsize, NULL,
          NULL));

  // Preparation before pushing gap event. Getting rid of all events which
  // came by this point - segment, caps, etc
  while ((event = gst_harness_try_pull_event (h)) != NULL)
    gst_event_unref (event);

  // The buffer with sequence number = 0x1 is lost, it does not introduce
  // gaps in pictureids like if it was a FEC packet
  gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new ("GstRTPPacketLost",
              "seqnum", G_TYPE_UINT, (guint) 1,
              "timestamp", G_TYPE_UINT64, 33 * GST_MSECOND,
              "duration", G_TYPE_UINT64, 33 * GST_MSECOND, NULL)));

  // Making shure the GAP event was not pushed downstream
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  // The next buffer seq_num = 2
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[1].buf, test_data[1].bufsize, 0, test_data[1].bufsize, NULL,
          NULL));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  // Making shure the GAP event was not pushed downstream
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);
  gst_harness_teardown (h);
}

GST_START_TEST (test_depay_stop_gap_events)
{
  test_depay_stop_gap_events_base (&test_data_no_gap_in_picid[__i__][0]);
}

GST_END_TEST;

GST_START_TEST (test_depay_stop_gap_events_pictureid_wraps)
{
  gboolean mbit = __i__ != 0;
  guint8 *intra =
      g_memdup (test_data_no_gap_in_picid[mbit][0].buf,
      test_data_no_gap_in_picid[mbit][0].bufsize);
  guint8 *inter =
      g_memdup (test_data_no_gap_in_picid[mbit][1].buf,
      test_data_no_gap_in_picid[mbit][1].bufsize);
  stop_gap_events_test_data_t test_data[2] = {
    {intra, test_data_no_gap_in_picid[mbit][0].bufsize}
    ,
    {inter, test_data_no_gap_in_picid[mbit][1].bufsize}
  };

  if (mbit) {
    intra[14] = 0xff;           // Mbit=1 + 0x7fff pictureid
    intra[15] = 0xff;
    inter[14] = 0x80;           // Mbit=1 + 0x000 pictureid
    inter[15] = 0x00;
  } else {
    intra[14] = 0x7f;           // Mbit=0 + 0x7f pictureid
    inter[14] = 0x00;           // Mbit=0 + 0x00 pictureid
  }
  test_depay_stop_gap_events_base (&test_data[0]);
  g_free (intra);
  g_free (inter);
}

GST_END_TEST;

GST_START_TEST (test_depay_stop_gap_events_extend_pictureid_bitwidth)
{
  guint8 *intra =
      g_memdup (intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0));
  guint8 *inter =
      g_memdup (inter_picid6337_seqnum2, sizeof (inter_picid6337_seqnum2));
  stop_gap_events_test_data_t test_data[2] = {
    {intra, sizeof (intra_picid24_seqnum0)}
    ,
    {inter, sizeof (inter_picid6337_seqnum2)}
  };

  // Overriding picture id, so that intra picutre id = 127 and inter = 128
  intra[14] = 0x7f;             // intra has Mbit = 0
  inter[14] = 0x80;             // inter has Mbit = 1
  inter[15] = 0x80;             // 0x7f + 1 = 0x80
  test_depay_stop_gap_events_base (&test_data[0]);
  g_free (intra);
  g_free (inter);
}

GST_END_TEST;

static void
test_depay_resend_gap_events_base (stop_gap_events_test_data_t * test_data)
{
  GstEvent *event;
  GstHarness *h = gst_harness_new ("rtpvp8depay");
  gst_harness_set_src_caps_str (h, RTP_VP8_CAPS_STR);

  // The fist buffer has seq_num = 0
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[0].buf, test_data[0].bufsize, 0, test_data[0].bufsize, NULL,
          NULL));

  // Preparation before pushing gap event. Getting rid of all events which
  // came by this point - segment, caps, etc
  while ((event = gst_harness_try_pull_event (h)) != NULL)
    gst_event_unref (event);

  // The buffer with sequence number = 0x1 is lost, but this time
  // the lost buffer is a frame, that's why there is a gap in pictureid's
  gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new ("GstRTPPacketLost",
              "seqnum", G_TYPE_UINT, (guint) 1,
              "timestamp", G_TYPE_UINT64, 33 * GST_MSECOND,
              "duration", G_TYPE_UINT64, 33 * GST_MSECOND, NULL)));

  // The next buffer seq_num = 2
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[1].buf, test_data[1].bufsize, 0, test_data[1].bufsize, NULL,
          NULL));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  // Making shure the GAP event was pushed downstream
  fail_unless_equals_int (gst_harness_events_in_queue (h), 1);
  gst_harness_teardown (h);
}

GST_START_TEST (test_depay_resend_gap_events)
{
  gboolean mbit = __i__ != 0;
  guint8 *intra =
      g_memdup (test_data_no_gap_in_picid[mbit][0].buf,
      test_data_no_gap_in_picid[mbit][0].bufsize);
  guint8 *inter =
      g_memdup (test_data_no_gap_in_picid[mbit][1].buf,
      test_data_no_gap_in_picid[mbit][1].bufsize);
  stop_gap_events_test_data_t test_data[2] = {
    {intra, test_data_no_gap_in_picid[mbit][0].bufsize}
    ,
    {inter, test_data_no_gap_in_picid[mbit][1].bufsize}
  };

  // Introducing a gap in the picture id in the test data
  if (mbit) {
    inter[15] += 1;
  } else {
    inter[14] += 1;
  }
  test_depay_resend_gap_events_base (&test_data[0]);
  g_free (intra);
  g_free (inter);
}

GST_END_TEST;

GST_START_TEST (test_depay_resend_gap_events_extend_pictureid_bitwidth)
{
  guint8 *intra =
      g_memdup (intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0));
  guint8 *inter =
      g_memdup (inter_picid6337_seqnum2, sizeof (inter_picid6337_seqnum2));
  stop_gap_events_test_data_t test_data[2] = {
    {intra, sizeof (intra_picid24_seqnum0)}
    ,
    {inter_nopicid_seqnum2, sizeof (inter_nopicid_seqnum2)}
  };

  // Overriding picture id, so that intra picutre id = 127 and inter = 129
  intra[14] = 0x7f;             // intra has Mbit = 0
  inter[14] = 0x80;             // inter has Mbit = 1
  inter[15] = 0x81;             // 0x7f + 2 = 0x81
  test_depay_resend_gap_events_base (&test_data[0]);
  g_free (intra);
  g_free (inter);
}

GST_END_TEST;

GST_START_TEST (test_depay_resend_gap_events_nopicid)
{
  test_depay_resend_gap_events_base (&test_data_nopicid[__i__][0]);
}

GST_END_TEST;


static Suite *
rtpvp8_suite (void)
{
  Suite *s = suite_create ("rtpvp8");
  TCase *tc_chain;

  suite_add_tcase (s, (tc_chain = tcase_create ("vp8depay")));
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events, 0,
      G_N_ELEMENTS (test_data_no_gap_in_picid));
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events_pictureid_wraps, 0,
      G_N_ELEMENTS (test_data_no_gap_in_picid));
  tcase_add_test (tc_chain,
      test_depay_stop_gap_events_extend_pictureid_bitwidth);
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_events, 0,
      G_N_ELEMENTS (test_data_no_gap_in_picid));
  tcase_add_test (tc_chain,
      test_depay_resend_gap_events_extend_pictureid_bitwidth);
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_events_nopicid, 0,
      G_N_ELEMENTS (test_data_nopicid));

  return s;
}

GST_CHECK_MAIN (rtpvp8);
