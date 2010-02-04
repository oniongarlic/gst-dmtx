/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2009 Kaj-Michael Lang <milang@tal.org>
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
 
#ifndef __GST_DMTX_H__
#define __GST_DMTX_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/gstvideofilter.h>

#include <dmtx.h>

G_BEGIN_DECLS

#define GST_TYPE_DMTX \
  (gst_dmtx_get_type())
#define GST_DMTX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DMTX,Gstdmtx))
#define GST_DMTX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DMTX,GstdmtxClass))
#define GST_IS_DMTX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DMTX))
#define GST_IS_DMTX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DMTX))

typedef enum {
	GST_DMTX_TYPE_MATRIX=0,
	GST_DMTX_TYPE_MOSAIC=1
} GstDmtxDataType;

typedef struct _Gstdmtx      Gstdmtx;
typedef struct _GstdmtxClass GstdmtxClass;

struct _Gstdmtx {
	GstVideoFilter element;

	/* Private */
	gboolean silent;
	gboolean skip_dups;
	gint scale;
	gint stop_after;
	gint timeout;
	gint found_count;
	gint skip;
	gchar *last;
	GstDmtxDataType dtype;

	/* Worker thread */
	GThread *thread;
	GAsyncQueue *request_queue;
	gboolean keep_running;

	DmtxDecode  *ddec;
	DmtxImage   *dimg;
	DmtxRegion  *dreg;
	DmtxPackOrder dpo;
	gint width, height, stride, bpp;

};

struct _GstdmtxClass {
	GstVideoFilterClass parent_class;
};

GType gst_dmtx_get_type (void);

G_END_DECLS

#endif /* __GST_DMTX_H__ */
