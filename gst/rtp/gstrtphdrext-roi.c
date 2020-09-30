/* GStreamer
 * Copyright (C) <2020> Camilo Celis Guzman <camilo@pexip.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gstrtphdrext-roi
 * @title: GstRtphdrext-RoI
 * @short_description: Helper methods for dealing with RTP header extensions
 * in the Video RTP Profile for 'urn:roi (FIXME:TBD)
 * @see_also: #GstRTPHeaderExtension, #GstRTPBasePayload, #GstRTPBaseDepayload, gstrtpbuffer
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/rtp/gstrtpbuffer.h>

#include <gst/video/video.h>

#include "gstrtphdrext-roi.h"

GST_DEBUG_CATEGORY_STATIC (rtphdrext_roi_debug);
#define GST_CAT_DEFAULT (rtphdrext_roi_debug)

#define gst_gl_base_filter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRTPHeaderExtensionRoI,
    gst_rtp_header_extension_roi, GST_TYPE_RTP_HEADER_EXTENSION,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "rtphdrext-roi", 0,
        "RTP RoI (Region of Interest) Header Extensions");
    );

#define EXTMAP_STR "urn:roi" // FIXME

static const gchar * gst_rtp_header_extension_roi_get_uri (GstRTPHeaderExtension * ext);
static GstRTPHeaderExtensionFlags gst_rtp_header_extension_roi_get_supported_flags (GstRTPHeaderExtension * ext);
static gsize gst_rtp_header_extension_roi_get_max_size (GstRTPHeaderExtension * ext, const GstBuffer * buffer);
static gsize gst_rtp_header_extension_roi_write (GstRTPHeaderExtension * ext, const GstBuffer * input_meta, GstRTPHeaderExtensionFlags write_flags, GstBuffer * output, guint8 * data, gsize size);
static gboolean gst_rtp_header_extension_roi_read (GstRTPHeaderExtension * ext, GstRTPHeaderExtensionFlags read_flags, const guint8 * data, gsize size, GstBuffer * buffer);

enum
{
  PROP_0,
  PROP_N_STREAMS,
};

static void
gst_rtp_header_extension_roi_class_init (GstRTPHeaderExtension3GPPOrientationClass * klass)
{
  GstRTPHeaderExtensionClass *rtp_hdr_class;
  GstElementClass *gstelement_class;

  rtp_hdr_class = (GstRTPHeaderExtensionClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  rtp_hdr_class->get_uri = gst_rtp_header_extension_roi_get_uri;
  rtp_hdr_class->get_supported_flags = gst_rtp_header_extension_roi_get_supported_flags;
  rtp_hdr_class->get_max_size = gst_rtp_header_extension_roi_get_max_size;
  rtp_hdr_class->write = gst_rtp_header_extension_roi_write;
  rtp_hdr_class->read = gst_rtp_header_extension_roi_read;

  gst_element_class_set_static_metadata (gstelement_class,
      "RoI (Region of Interest)", GST_RTP_HDREXT_ELEMENT_CLASS,
      "Extends RTP packets to add RoI",
      "Camilo Celis Guzman <camilo@pexip.com>");
}

static void
gst_rtp_header_extension_roi_init (GstRTPHeaderExtension3GPPOrientation * orientation)
{
}

static const gchar *
gst_rtp_header_extension_roi_get_uri (GstRTPHeaderExtension * ext)
{
  return EXTMAP_STR;
}

static GstRTPHeaderExtensionFlags
gst_rtp_header_extension_roi_get_supported_flags (GstRTPHeaderExtension * ext)
{
	// FIXME: Do we support both?
  return GST_RTP_HEADER_EXTENSION_ONE_BYTE | GST_RTP_HEADER_EXTENSION_TWO_BYTE;
}

static gsize
gst_rtp_header_extension_roi_get_max_size (GstRTPHeaderExtension *
    ext, const GstBuffer * buffer)
{
  return 1;
}

static gsize
gst_rtp_header_extension_roi_write (GstRTPHeaderExtension * ext,
    const GstBuffer * input_meta, GstRTPHeaderExtensionFlags write_flags,
    GstBuffer * output, guint8 * data, gsize size)
{
  const GstVideoRegionOfInterestMeta *meta;
  gsize written = 0;

  g_return_val_if_fail (size >=
      gst_rtp_header_extension_roi_get_max_size (ext, NULL), -1);
  g_return_val_if_fail (write_flags &
      gst_rtp_header_extension_roi_get_supported_flags (ext), -1);

	meta = gst_buffer_get_video_region_of_interest_meta ((GstBuffer *) input_meta);
  if (!meta) {
    data[0] = 0;
    return 1;
  }

  written = 1;

	/* TODO */

	GST_LOG_OBJECT (ext, "wrote %" G_GSIZE_FORMAT " bytes from region of interest"
      "as bytes 0x%x", written, data[0]);

  return written;
}

static gboolean
gst_rtp_header_extension_roi_read (GstRTPHeaderExtension * ext,
    GstRTPHeaderExtensionFlags read_flags, const guint8 * data, gsize size,
    GstBuffer * buffer)
{
	(void) ext;
	(void) read_flags;
	(void) buffer;
  g_return_val_if_fail (size >= 1, FALSE);

	/* TODO */

	GST_LOG_OBJECT (ext, "read byte 0x%x to region of interest", data[0]);

  return gst_buffer_add_video_region_of_interest_meta (buffer, orientation) != NULL;
}
