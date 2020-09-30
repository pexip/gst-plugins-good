/* GStreamer
 * Copyright (C) 2020 Camilo Celis Guzman <camilo@pexip.com>
 *
 * gstrtphdrextroi.h: RoI (Region of Interest) header extensions
 *   for the Audio/Video RTP Profile
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

#ifndef __GST_RTPHDREXT_ROI_H__
#define __GST_RTPHDREXT_ROI_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtphdrext.h>

G_BEGIN_DECLS

GST_RTP_API
GType gst_rtp_header_extension_roi_get_type (void);
#define GST_TYPE_RTP_HEADER_EXTENSION_ROI (gst_rtp_header_extension_roi_get_type())
#define GST_RTP_HEADER_EXTENSION_ROI(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_HEADER_EXTENSION_ROI,GstRTPHeaderExtensionRoI))
#define GST_RTP_HEADER_EXTENSION_ROI_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_HEADER_EXTENSION_ROI,GstRTPHeaderExtensionRoIClass))
#define GST_RTP_HEADER_EXTENSION_ROI_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_RTP_HEADER_EXTENSION_ROI,GstRTPHeaderExtensionRoIClass))
#define GST_IS_RTP_HEADER_EXTENSION_ROI(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_HEADER_EXTENSION_ROI))
#define GST_IS_RTP_HEADER_EXTENSION_ROI_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_HEADER_EXTENSION_ROI))
#define GST_RTP_HEADER_EXTENSION_ROI_CAST(obj) ((GstRTPHeaderExtensionRoI *)(obj))

typedef struct _GstRTPHeaderExtensionRoI      GstRTPHeaderExtensionRoI;
typedef struct _GstRTPHeaderExtensionRoIClass GstRTPHeaderExtensioRoIClass;

/**
 * GstRTPHeaderExtensionRoI:
 * @parent: the parent #GstRTPHeaderExtension
 *
 * Instance struct for a RoI (Region of Interest) RTP Audio/Video header extension.
 *
 * Uses the extension uri 'urn:roi (FIXME)'
 */
struct _GstRTPHeaderExtensionRoI
{
  GstRTPHeaderExtension parent;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstRTPHeaderExtensionRoIClass:
 * @parent_class: the parent class
 *
 * Base class for RTP AV Header extensions.
 */
struct _GstRTPHeaderExtensionRoIClass
{
  GstRTPHeaderExtensionClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

G_END_DECLS

#endif /* __GST_RTPHDREXT_ROI_H__ */
