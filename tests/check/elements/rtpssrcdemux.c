/* GStreamer
 *
 * unit test for rtpssrcdemux element
 *
 * Copyright 2016 Pexip
 *  @author: Stian Selnes <stian@pexip.com>
 *  @author: Håvard Graff <havard@pexip.com>
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

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/gst.h>

static GstBuffer *
generate_test_buffer (guint seq_num, guint ssrc)
{
  GstBuffer *buf;
  guint8 *payload;
  guint i;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  gsize size = 10;

  buf = gst_rtp_buffer_new_allocate (size, 0, 0);
  GST_BUFFER_DTS (buf) = GST_MSECOND * 20 * seq_num;
  GST_BUFFER_PTS (buf) = GST_MSECOND * 20 * seq_num;

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_payload_type (&rtp, 0);
  gst_rtp_buffer_set_seq (&rtp, seq_num);
  gst_rtp_buffer_set_timestamp (&rtp, 160 * seq_num);
  gst_rtp_buffer_set_ssrc (&rtp, ssrc);

  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < size; i++)
    payload[i] = 0xff;

  gst_rtp_buffer_unmap (&rtp);

  return buf;
}

static void
new_ssrc_pad_found (GstElement * element, guint ssrc, GstPad * pad,
    GSList ** src_h)
{
  GstHarness *h;
  (void) ssrc;

  h = gst_harness_new_with_element (element, NULL, NULL);
  gst_harness_add_element_src_pad (h, pad);
  *src_h = g_slist_prepend (*src_h, h);
}

GST_START_TEST (test_rtpssrcdemux_max_streams)
{
  GstHarness *h = gst_harness_new_with_padnames ("rtpssrcdemux", "sink", NULL);
  GSList *src_h = NULL;

  g_object_set (h->element, "max-streams", 64, NULL);
  gst_harness_set_src_caps_str (h, "application/x-rtp");
  g_signal_connect (h->element,
      "new-ssrc-pad", (GCallback) new_ssrc_pad_found, &src_h);
  gst_harness_play (h);

  for (int i = 0; i < 128; ++i) {
    fail_unless_equals_int (GST_FLOW_OK,
        gst_harness_push (h, generate_test_buffer (0, i)));
  }

  fail_unless_equals_int (g_slist_length (src_h), 64);
  g_slist_free_full (src_h, (GDestroyNotify) gst_harness_teardown);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
rtpssrcdemux_suite (void)
{
  Suite *s = suite_create ("rtpssrcdemux");
  TCase *tc_chain;

  tc_chain = tcase_create ("general");
  tcase_add_test (tc_chain, test_rtpssrcdemux_max_streams);
  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (rtpssrcdemux)
