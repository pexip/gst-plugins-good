/* GStreamer
 *
 * unit test for matroskamux
 *
 * Copyright (C) <2005> Michal Benes <michal.benes@xeris.cz>
 * Copyright (C) <2017> Havard Graff <havard@pexip.com>
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

#include <unistd.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#define AC3_CAPS_STRING "audio/x-ac3, " \
                        "channels = (int) 1, " \
                        "rate = (int) 8000"
#define VORBIS_TMPL_CAPS_STRING "audio/x-vorbis, " \
                                "channels = (int) 1, " \
                                "rate = (int) 8000"
/* streamheader shouldn't be in the template caps, only in the actual caps */
#define VORBIS_CAPS_STRING VORBIS_TMPL_CAPS_STRING \
                           ", streamheader=(buffer)<10, 2020, 303030>"

#define compare_buffer_to_data(buffer, data)                               \
  G_STMT_START {                                                           \
    fail_unless_equals_int (sizeof (data), gst_buffer_get_size (buffer));  \
    fail_unless (gst_buffer_memcmp (buffer, 0, data, sizeof (data)) == 0); \
  } G_STMT_END

GST_START_TEST (test_ebml_header_v1)
{
  GstBuffer *inbuffer, *outbuffer;

  guint8 data[] = {
    0x1a, 0x45, 0xdf, 0xa3, /* master ID */
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,
    0x42, 0x82, /* doctype */
          0x89, /* 9 bytes */
          0x6d, 0x61, 0x74, 0x72, 0x6f, 0x73, 0x6b, 0x61, 0x00, /* "matroska" */
    0x42, 0x87, /* doctypeversion */
          0x81, /* 1 byte */
          0x01, /* "1" */
    0x42, 0x85, /* doctypereadversion */
          0x81, /* 1 byte */
          0x01, /* "1" */
  };

  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  g_object_set (h->element, "version", 1, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* push a dummy buffer */
  inbuffer = gst_buffer_new_allocate (NULL, 1, 0);
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  outbuffer = gst_harness_pull (h);
  /* verify the header */
  compare_buffer_to_data (outbuffer, data);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_ebml_header_v2)
{
  GstBuffer *inbuffer, *outbuffer;

  guint8 data[] = {
    0x1a, 0x45, 0xdf, 0xa3, /* master ID */
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,
    0x42, 0x82, /* doctype */
          0x89, /* 9 bytes */
          0x6d, 0x61, 0x74, 0x72, 0x6f, 0x73, 0x6b, 0x61, 0x00, /* "matroska" */
    0x42, 0x87, /* doctypeversion */
          0x81, /* 1 byte */
          0x02, /* "2" */
    0x42, 0x85, /* doctypereadversion */
          0x81, /* 1 byte */
          0x02, /* "2" */
  };
  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
     "audio_%u", "src");
  g_object_set (h->element, "version", 2, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* push a dummy buffer */
  inbuffer = gst_buffer_new_allocate (NULL, 1, 0);
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  outbuffer = gst_harness_pull (h);
  /* verify the header */
  compare_buffer_to_data (outbuffer, data);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_vorbis_header)
{
  GstBuffer *inbuffer, *outbuffer;

  guint8 data[12] = {
    0x63, 0xa2, 0x89, 0x02, 0x01, 0x02, 0x10, 0x20, 0x20, 0x30, 0x30, 0x30
  };

  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  gst_harness_set_src_caps_str (h, VORBIS_CAPS_STRING);

  /* push a dummy buffer */
  inbuffer = gst_buffer_new_allocate (NULL, 1, 0);
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  /* first buffer is the ebml-header */
  gst_buffer_unref (gst_harness_pull (h));

  outbuffer = gst_harness_pull (h);

  /* the header should sit at the last 12 bytes of the buffer */
  fail_unless (gst_buffer_memcmp (outbuffer,
      gst_buffer_get_size (outbuffer) - sizeof (data),
      data, sizeof (data)) == 0);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_block_group_v1)
{
  GstBuffer *inbuffer, *outbuffer;

  guint8 data_h0[] = {
    0xa0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0xa1, 0x85, 0x81, 0x00, 0x00, 0x00
  };
  guint8 data0[] = { 0x01 };

  guint8 data_h1[] = {
    0xa0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0xa1, 0x85, 0x81, 0x00, 0x01, 0x00
  };
  guint8 data1[] = { 0x42 };

  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  g_object_set (h->element, "version", 1, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* Buffer 0 */
  inbuffer = gst_buffer_new_wrapped (
      g_memdup (data0, sizeof (data0)), sizeof (data0));
  GST_BUFFER_TIMESTAMP (inbuffer) = 0 * GST_MSECOND;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* pull out headers */
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));

  /* verify header and data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h0);
  gst_buffer_unref (outbuffer);

  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data0);
  gst_buffer_unref (outbuffer);

  /* Buffer 1 */
  inbuffer = gst_buffer_new_wrapped (
      g_memdup (data1, sizeof (data1)), sizeof (data1));
  GST_BUFFER_TIMESTAMP (inbuffer) = 1 * GST_MSECOND;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* verify header and data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h1);
  gst_buffer_unref (outbuffer);

  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data1);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_block_group_v2)
{
  GstBuffer *inbuffer, *outbuffer;
  guint8 data_h0[] = {
    0xa3, 0x85, 0x81, 0x00, 0x00, 0x00,
  };
  guint8 data0[] = { 0x01 };

  guint8 data_h1[] = {
    0xa3, 0x85, 0x81, 0x00, 0x01, 0x00,
  };
  guint8 data1[] = { 0x42 };

  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  g_object_set (h->element, "version", 2, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* Buffer 0 */
  inbuffer = gst_buffer_new_wrapped (
      g_memdup (data0, sizeof (data0)), sizeof (data0));
  GST_BUFFER_TIMESTAMP (inbuffer) = 0 * GST_MSECOND;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* pull out headers */
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));

  /* verify header and data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h0);
  gst_buffer_unref (outbuffer);

  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data0);
  gst_buffer_unref (outbuffer);

  /* Buffer 1 */
  inbuffer = gst_buffer_new_wrapped (
      g_memdup (data1, sizeof (data1)), sizeof (data1));
  GST_BUFFER_TIMESTAMP (inbuffer) = 1 * GST_MSECOND;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* verify header and data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h1);
  gst_buffer_unref (outbuffer);

  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data1);
  gst_buffer_unref (outbuffer);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_reset)
{
  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  g_object_set (h->element, "version", 1, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* push a dummy buffer */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h,
          gst_buffer_new_allocate (NULL, 1, 0)));
  /* expect 2 header buffers */
  fail_unless_equals_int (2, gst_harness_buffers_received (h));

  /* set to NULL and back to PLAYING again (reset) */
  fail_unless_equals_int (GST_STATE_CHANGE_SUCCESS,
      gst_element_set_state (h->element, GST_STATE_NULL));
  fail_unless_equals_int (GST_STATE_CHANGE_SUCCESS,
      gst_element_set_state (h->element, GST_STATE_PLAYING));

  /* push another dummy buffer */
  fail_unless_equals_int (GST_FLOW_OK,
      gst_harness_push (h,
          gst_buffer_new_allocate (NULL, 1, 0)));

  /* we expect the 2 header-buffers sent again */
  fail_unless_equals_int (4, gst_harness_buffers_received (h));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_link_webmmux_webm_sink)
{
  GstHarness *h = gst_harness_new_with_padnames ("webmmux",
      NULL, "src");
  gst_harness_set_sink_caps_str (h, "audio/webm");
  gst_harness_teardown (h);
}

GST_END_TEST;

static gint64 timecodescales[] = {
  GST_USECOND,
  GST_MSECOND,
  GST_MSECOND * 10,
  GST_MSECOND * 100,
  GST_MSECOND * 400,
  /* FAILS: ? GST_MSECOND * 500, a bug?*/
};

GST_START_TEST (test_timecodescale)
{
  GstBuffer *inbuffer, *outbuffer;
  guint8 data_h0[] = {
    0xa3, 0x85, 0x81, 0x00, 0x00, 0x00,
  };
  guint8 data_h1[] = {
    0xa3, 0x85, 0x81, 0x00, 0x01, 0x00,
  };

  GstHarness *h = gst_harness_new_with_padnames ("matroskamux",
      "audio_%u", "src");
  gint64 timecodescale = timecodescales[__i__];

  g_object_set (h->element, "timecodescale", timecodescale, NULL);
  g_object_set (h->element, "version", 2, NULL);
  gst_harness_set_src_caps_str (h, AC3_CAPS_STRING);

  /* Buffer 0 */
  inbuffer = gst_buffer_new_allocate (NULL, 1, 0);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* pull out headers */
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));
  gst_buffer_unref (gst_harness_pull (h));

  /* verify header and drop the data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h0);
  gst_buffer_unref (outbuffer);
  gst_buffer_unref (gst_harness_pull (h));

  /* Buffer 1 */
  inbuffer = gst_buffer_new_allocate (NULL, 1, 0);
  GST_BUFFER_TIMESTAMP (inbuffer) = timecodescale;
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push (h, inbuffer));

  /* verify header and drop the data */
  outbuffer = gst_harness_pull (h);
  compare_buffer_to_data (outbuffer, data_h1);
  gst_buffer_unref (outbuffer);
  gst_buffer_unref (gst_harness_pull (h));

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
matroskamux_suite (void)
{
  Suite *s = suite_create ("matroskamux");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_ebml_header_v1);
  tcase_add_test (tc_chain, test_ebml_header_v2);
  tcase_add_test (tc_chain, test_vorbis_header);
  tcase_add_test (tc_chain, test_block_group_v1);
  tcase_add_test (tc_chain, test_block_group_v2);

  tcase_add_test (tc_chain, test_reset);
  tcase_add_test (tc_chain, test_link_webmmux_webm_sink);
  tcase_add_loop_test (tc_chain, test_timecodescale,
      0, G_N_ELEMENTS (timecodescales));

  return s;
}
GST_CHECK_MAIN (matroskamux);
