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
static guint8 inter_picid6337_seqnum2[] = {
  0x80, 0xf4, 0x00, 0x02, 0x49, 0x88, 0xe5, 0x38, 0xa0, 0x6c, 0x65, 0x6c,
  0xcc, 0x98, 0xc1, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0x97, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0x8a, 0x9f, 0x01, 0xbc
};
static guint8 intra_picid24_seqnum0[] = {
  0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
  0x8c, 0x18, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
};
static guint8 inter_picid25_seqnum2[] = {
  0x80, 0xf4, 0x00, 0x02, 0x49, 0x88, 0xe5, 0x38, 0xa0, 0x6c, 0x65, 0x6c,
  0xcc, 0x19, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0x97, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0x8a, 0x9f, 0x01, 0xbc
};
static guint8 intra_nopicid_seqnum0[] = {
  0x80, 0xf4, 0x00, 0x00, 0x49, 0x88, 0xd9, 0xf8, 0xa0, 0x6c, 0x65, 0x6c,
  0x0c, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0xb9, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0xa6, 0x4d, 0x01, 0xa5
};
static guint8 inter_nopicid_seqnum2[] = {
  0x80, 0xf4, 0x00, 0x02, 0x49, 0x88, 0xe5, 0x38, 0xa0, 0x6c, 0x65, 0x6c,
  0x4c, 0x87, 0x01, 0x02, 0x49, 0x3f, 0x1c, 0x12, 0x0e, 0x0c,
  0xd0, 0x1b, 0x97, 0x80, 0x80, 0xb0, 0x18, 0x0f, 0x8a, 0x9f, 0x01, 0xbc
};

typedef struct _stop_gap_events_test_data_t
{
  guint8 *buf;
  gsize   bufsize;
} stop_gap_events_test_data_t;

stop_gap_events_test_data_t test_data_no_gap_in_picid[2][2] = {
  // Pictureid without M bit
  {
    {intra_picid24_seqnum0, sizeof(intra_picid24_seqnum0)},
    {inter_picid25_seqnum2, sizeof(inter_picid25_seqnum2)}
  },
  // Pictureid with M bit
  {
    {intra_picid6336_seqnum0, sizeof(intra_picid6336_seqnum0)},
    {inter_picid6337_seqnum2, sizeof(inter_picid6337_seqnum2)}
  },
};

stop_gap_events_test_data_t test_data_nopicid[3][2] = {
  {
    {intra_picid24_seqnum0, sizeof(intra_picid24_seqnum0)},
    {inter_nopicid_seqnum2, sizeof(inter_nopicid_seqnum2)}
  },
  {
    {intra_nopicid_seqnum0, sizeof(intra_nopicid_seqnum0)},
    {inter_picid25_seqnum2, sizeof(inter_picid25_seqnum2)}
  },
  {
    {intra_nopicid_seqnum0, sizeof(intra_nopicid_seqnum0)},
    {inter_nopicid_seqnum2, sizeof(inter_nopicid_seqnum2)}
  },
};

static void
test_depay_stop_gap_events_base (stop_gap_events_test_data_t * test_data)
{
  GstEvent *event;
  GstHarness *h = gst_harness_new ("rtpvp9depay");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  // The fist buffer has seq_num = 0
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[0].buf, test_data[0].bufsize, 0, test_data[0].bufsize, NULL, NULL));

  // Preparation before pushing gap event. Getting rid of all events which
  // came by this point - segment, caps, etc
  while ((event = gst_harness_try_pull_event (h)) != NULL)
    gst_event_unref (event);

  // The buffer with sequence number = 0x1 is lost, it does not introduce
  // gaps in pictureids like if it was a FEC packet
  gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("GstRTPPacketLost",
          "seqnum", G_TYPE_UINT, (guint) 1,
          "timestamp", G_TYPE_UINT64, 33*GST_MSECOND,
          "duration", G_TYPE_UINT64, 33*GST_MSECOND, NULL)));

  // Making shure the GAP event was not pushed downstream
  fail_unless_equals_int (gst_harness_events_in_queue (h), 0);

  // The next buffer seq_num = 2
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[1].buf, test_data[1].bufsize, 0, test_data[1].bufsize, NULL, NULL));
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

GST_START_TEST (test_depay_stop_gap_events_extend_pictureid_bitwidth)
{
  guint8 *intra = g_memdup (intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0));
  guint8 *inter = g_memdup (inter_picid6337_seqnum2, sizeof (inter_picid6337_seqnum2));
  stop_gap_events_test_data_t test_data[2] = {
    {intra, sizeof(intra_picid24_seqnum0)},
    {inter, sizeof(inter_picid6337_seqnum2)}
  };

  // Overriding picture id, so that intra picutre id = 127 and inter = 128
  intra[13] = 0x7f; // intra has Mbit = 0
  inter[13] = 0x80; // inter has Mbit = 1
  inter[14] = 0x80; // 0x7f + 1 = 0x80
  test_depay_stop_gap_events_base (&test_data[0]);
  g_free(intra);
  g_free(inter);
}
GST_END_TEST;

static void
test_depay_resend_gap_events_base (stop_gap_events_test_data_t * test_data)
{
  GstEvent *event;
  GstHarness *h = gst_harness_new ("rtpvp9depay");
  gst_harness_set_src_caps_str (h, RTP_VP9_CAPS_STR);

  // The fist buffer has seq_num = 0
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[0].buf, test_data[0].bufsize, 0, test_data[0].bufsize, NULL, NULL));

  // Preparation before pushing gap event. Getting rid of all events which
  // came by this point - segment, caps, etc
  while ((event = gst_harness_try_pull_event (h)) != NULL)
    gst_event_unref (event);

  // The buffer with sequence number = 0x1 is lost, but this time
  // the lost buffer is a frame, that's why there is a gap in pictureid's
  gst_harness_push_event (h, gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("GstRTPPacketLost",
          "seqnum", G_TYPE_UINT, (guint) 1,
          "timestamp", G_TYPE_UINT64, 33*GST_MSECOND,
          "duration", G_TYPE_UINT64, 33*GST_MSECOND, NULL)));

  // The next buffer seq_num = 2
  gst_harness_push (h, gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          test_data[1].buf, test_data[1].bufsize, 0, test_data[1].bufsize, NULL, NULL));
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  // Making shure the GAP event was pushed downstream
  fail_unless_equals_int (gst_harness_events_in_queue (h), 1);
  gst_harness_teardown (h);
}

GST_START_TEST (test_depay_resend_gap_events)
{
  gboolean mbit = __i__ != 0;
  guint8 *intra = g_memdup (test_data_no_gap_in_picid[mbit][0].buf, test_data_no_gap_in_picid[mbit][0].bufsize);
  guint8 *inter = g_memdup (test_data_no_gap_in_picid[mbit][1].buf, test_data_no_gap_in_picid[mbit][1].bufsize);
  stop_gap_events_test_data_t test_data[2] = {
    {intra, test_data_no_gap_in_picid[mbit][0].bufsize},
    {inter, test_data_no_gap_in_picid[mbit][1].bufsize}
  };

  // Introducing a gap in the picture id in the test data
  if (mbit) {
    inter[14] += 1;
  } else {
    inter[13] += 1;
  }
  test_depay_resend_gap_events_base (&test_data[0]);
  g_free(intra);
  g_free(inter);
}
GST_END_TEST;

GST_START_TEST (test_depay_resend_gap_events_extend_pictureid_bitwidth)
{
  guint8 *intra = g_memdup (intra_picid24_seqnum0, sizeof (intra_picid24_seqnum0));
  guint8 *inter = g_memdup (inter_picid6337_seqnum2, sizeof (inter_picid6337_seqnum2));
  stop_gap_events_test_data_t test_data[2] = {
    {intra, sizeof(intra_picid24_seqnum0)},
    {inter_nopicid_seqnum2, sizeof(inter_nopicid_seqnum2)}
  };

  // Overriding picture id, so that intra picutre id = 127 and inter = 129
  intra[13] = 0x7f; // intra has Mbit = 0
  inter[13] = 0x80; // inter has Mbit = 1
  inter[14] = 0x81; // 0x7f + 2 = 0x81
  test_depay_resend_gap_events_base (&test_data[0]);
  g_free(intra);
  g_free(inter);
}
GST_END_TEST;

GST_START_TEST (test_depay_resend_gap_events_nopicid)
{
  test_depay_resend_gap_events_base (&test_data_nopicid[__i__][0]);
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
  tcase_add_loop_test (tc_chain, test_depay_stop_gap_events, 0, G_N_ELEMENTS (test_data_no_gap_in_picid));
  tcase_add_test (tc_chain, test_depay_stop_gap_events_extend_pictureid_bitwidth);
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_events, 0, G_N_ELEMENTS (test_data_no_gap_in_picid));
  tcase_add_test (tc_chain, test_depay_resend_gap_events_extend_pictureid_bitwidth);
  tcase_add_loop_test (tc_chain, test_depay_resend_gap_events_nopicid, 0, G_N_ELEMENTS (test_data_nopicid));

  return s;
}

GST_CHECK_MAIN (rtpvp9);
