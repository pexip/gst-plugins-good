#ifndef __GST_MULTIUDPSINK_TIMESTAMPING_H__
#define __GST_MULTIUDPSINK_TIMESTAMPING_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixtimestampingmessage.h>
//#include "gstmultiudpsink.h"

G_BEGIN_DECLS
#define GST_TYPE_MULTIUDPSINK_TIMESTAMPING  			 (gst_multiudpsink_timestamping_get_type())
#define GST_MULTIUDPSINK_TIMESTAMPING(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MULTIUDPSINK_TIMESTAMPING,GstMultiUDPSinkTimestamping))
#define GST_MULTIUDPSINK_TIMESTAMPING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MULTIUDPSINK_TIMESTAMPING,GstMultiUDPSinkTimestampingClass))
#define GST_IS_MULTIUDPSINK_TIMESTAMPING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MULTIUDPSINK_TIMESTAMPING))
#define GST_IS_MULTIUDPSINK_TIMESTAMPING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MULTIUDPSINK_TIMESTAMPING))
#define GST_MULTIUDPSINK_TIMESTAMPING_CAST(obj)       ((GstMultiUDPSinkTimestamping*)(obj))

typedef struct _GstMultiUDPSinkTimestamping GstMultiUDPSinkTimestamping;
typedef struct _GstMultiUDPSinkTimestampingClass GstMultiUDPSinkTimestampingClass;

//Need to forward declare this due to cyclic deps.
typedef struct _GstMultiUDPSink GstMultiUDPSink;

struct _GstMultiUDPSinkTimestampingClass {
  GObjectClass parent_class;
};

struct _GstMultiUDPSinkTimestamping {	
  GThread       *thread;
  gboolean       signal_stop;
};


GLIB_AVAILABLE_IN_ALL GstMultiUDPSinkTimestamping * gst_multiudpsink_timestamping_new (GstMultiUDPSink * sink);
GLIB_AVAILABLE_IN_ALL gboolean gst_multiudpsink_timestamping_get_socket_enabled(GSocket *);
GType gst_multiudpsink_timestamping_get_type (void);

G_END_DECLS
#endif
