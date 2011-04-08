/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2009-2011 Kaj-Michael Lang <milang@tal.org>
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
 * SECTION:element-dmtx
 *
 * Dmtx scans image buffers for data matrix barcodes, and sends a message if one is found.
 * If the skip property is set to 0, every buffer will be examined, if skip is larger than 0, 
 * then buffers will be pushed to a worker thread (and only if the worker is not busy) so that
 * the pipeline won't stall.
 *
 * The element generate messages named
 * <classname>&quot;barcode&quot;</classname>. The structure containes these
 * fields:
 * <itemizedlist>
 * <listitem>
 *   <para>
 *   #GstClockTime
 *   <classname>&quot;timestamp&quot;</classname>:
 *   the timestamp of the buffer that triggered the message.
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   gchar*
 *   <classname>&quot;type&quot;</classname>:
 *   the symbol type.
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   gchar*
 *   <classname>&quot;symbol&quot;</classname>:
 *   the deteted bar code data.
 *   </para>
 * </listitem>
 * </itemizedlist>
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -m v4l2src ! ffmpegcolorspace ! dmtx ! fakesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include <dmtx.h>

#include "gstdmtx.h"

GST_DEBUG_CATEGORY_STATIC (gst_dmtx_debug);
#define GST_CAT_DEFAULT gst_dmtx_debug

/* Filter signals and args */
enum
{
LAST_SIGNAL
};

enum
{
PROP_0,
PROP_SILENT,
PROP_SCALE,
PROP_STOP_AFTER,
PROP_TIMEOUT,
PROP_SKIP,
PROP_SKIP_DUPS,
PROP_TYPE,
PROP_SCAN_GAP,
PROP_USE_REGION,
PROP_REGION_X_MAX,
PROP_REGION_X_MIN,
PROP_REGION_Y_MAX,
PROP_REGION_Y_MIN,
};

/* the capabilities of the inputs and outputs.
 *
 */
static GstStaticPadTemplate sink_template=
GST_STATIC_PAD_TEMPLATE (
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB ";" GST_VIDEO_CAPS_RGBA ";" GST_VIDEO_CAPS_RGB_16 ";"
	"video/x-raw-gray, "
	"bpp=8, "
	"depth=8, "
	"width=" GST_VIDEO_SIZE_RANGE ", "
	"height=" GST_VIDEO_SIZE_RANGE ", " "framerate=" GST_VIDEO_FPS_RANGE)
);

static GstStaticPadTemplate src_template=
GST_STATIC_PAD_TEMPLATE (
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB ";" GST_VIDEO_CAPS_RGBA ";" GST_VIDEO_CAPS_RGB_16 ";"
	"video/x-raw-gray, "
	"bpp=8, "
	"depth=8, "
	"width=" GST_VIDEO_SIZE_RANGE ", "
	"height=" GST_VIDEO_SIZE_RANGE ", " "framerate=" GST_VIDEO_FPS_RANGE)
);

/* debug category for fltering log messages
 *
 */
#define DEBUG_INIT(bla) \
	GST_DEBUG_CATEGORY_INIT (gst_dmtx_debug, "dmtx", 0, "dmtx");

GST_BOILERPLATE_FULL (Gstdmtx, gst_dmtx, GstBaseTransform, GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_dmtx_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dmtx_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_dmtx_set_caps (GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_dmtx_transform_ip (GstBaseTransform * base, GstBuffer * outbuf);
static gboolean gst_dmtx_start (GstBaseTransform * base);
static gboolean gst_dmtx_stop (GstBaseTransform * base);
static gboolean gst_dmtx_src_event (GstBaseTransform *trans, GstEvent *event);
static gpointer gst_dmtx_worker_thread(gpointer data);

/* GObject vmethod implementations */

static void
gst_dmtx_base_init(gpointer klass)
{
GstElementClass *element_class=GST_ELEMENT_CLASS (klass);

gst_element_class_set_details_simple (element_class,
	"dmtx",
	"Barcode/Filter",
	"Barcode filter using libdmtx",
	" <milang@tal.org>");

gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&src_template));
gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&sink_template));
}

/* initialize the dmtx's class */
static void
gst_dmtx_class_init(GstdmtxClass * klass)
{
GObjectClass *gobject_class;

gobject_class=(GObjectClass *) klass;
gobject_class->set_property=gst_dmtx_set_property;
gobject_class->get_property=gst_dmtx_get_property;

g_object_class_install_property (gobject_class, PROP_SILENT, g_param_spec_boolean ("silent", "Silent", "Turn of bus messages", FALSE, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_SKIP_DUPS, g_param_spec_boolean ("skip_dups", "Skip duplicates", "Send message for first match only", FALSE, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_SCALE, g_param_spec_int ("scale", "Scaling", "Scale input for faster operation", 1, 4, 1, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_TIMEOUT, g_param_spec_int ("timeout", "Timeout", "Try this long to find a code in a frame", 10, 5000, 100, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_STOP_AFTER, g_param_spec_int ("stop_after", "Stop after", "Send EOS after this many matches, set to 0 to keep going", 0, 500, 0, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_SKIP, g_param_spec_int ("skip", "Skip frames", "Use every x frame", 0, 30, 15, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_TYPE, g_param_spec_int ("type", "Matrix or Mosiac", "Scan for matrix=0 or mosaic=1", 0, 1, 0, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_SCAN_GAP, g_param_spec_int ("scan-gap", "Scan gap", "Scan gap size", 1, 32, 1, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_USE_REGION, g_param_spec_boolean ("use-region", "Use region", "Use region settings", FALSE, G_PARAM_READWRITE));

g_object_class_install_property (gobject_class, PROP_REGION_X_MAX, g_param_spec_int ("region-x-max", "x-max", "Region x max", 1, 8192, 1, G_PARAM_WRITABLE));
g_object_class_install_property (gobject_class, PROP_REGION_X_MIN, g_param_spec_int ("region-x-min", "x-min", "Region x min", 0, 8192, 0, G_PARAM_WRITABLE));
g_object_class_install_property (gobject_class, PROP_REGION_Y_MAX, g_param_spec_int ("region-y-max", "y-max", "Region y max", 1, 8192, 1, G_PARAM_WRITABLE));
g_object_class_install_property (gobject_class, PROP_REGION_Y_MIN, g_param_spec_int ("region-y-min", "y-min", "Region y min", 0, 8192, 1, G_PARAM_WRITABLE));

GST_BASE_TRANSFORM_CLASS (klass)->set_caps=GST_DEBUG_FUNCPTR (gst_dmtx_set_caps);
GST_BASE_TRANSFORM_CLASS (klass)->transform_ip=GST_DEBUG_FUNCPTR (gst_dmtx_transform_ip);
GST_BASE_TRANSFORM_CLASS (klass)->start=GST_DEBUG_FUNCPTR (gst_dmtx_start);
GST_BASE_TRANSFORM_CLASS (klass)->stop=GST_DEBUG_FUNCPTR (gst_dmtx_stop);
GST_BASE_TRANSFORM_CLASS (klass)->src_event=GST_DEBUG_FUNCPTR (gst_dmtx_src_event);
}

static gboolean
gst_dmtx_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps)
{
Gstdmtx *filter;
GstStructure *structure;
gboolean ret;
gint w, h, depth, bpp;

filter=GST_DMTX(btrans);
structure=gst_caps_get_structure(incaps, 0);

ret=gst_structure_get_int(structure, "width", &w);
ret&=gst_structure_get_int(structure, "height", &h);
ret&=gst_structure_get_int(structure, "depth", &depth);
ret&=gst_structure_get_int(structure, "bpp", &bpp);

if (!ret)
	return FALSE;

filter->width=w;
filter->height=h;
filter->bpp=bpp;

switch (filter->bpp) {
case 8:
	filter->dpo=DmtxPack8bppK;
break;
case 16:
	filter->dpo=DmtxPack16bppRGB;
break;
case 24:
	filter->dpo=DmtxPack24bppRGB;
break;
case 32:
	filter->dpo=DmtxPack32bppRGBX;
break;
default:
	return FALSE;
break;
}

return TRUE;
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_dmtx_init(Gstdmtx *filter, GstdmtxClass * klass)
{
filter->silent=FALSE;
filter->width=0;
filter->height=0;
filter->scale=1;
filter->timeout=100;
filter->stop_after=0;
filter->found_count=0;
filter->skip=15;
filter->last=NULL;
filter->request_queue=NULL;
filter->thread=NULL;
filter->keep_running=FALSE;
filter->dtype=GST_DMTX_TYPE_MATRIX;
filter->use_region=FALSE;
}

static gboolean
gst_dmtx_src_event(GstBaseTransform *trans, GstEvent *event)
{
return TRUE;
}

static gboolean
gst_dmtx_start_thread(Gstdmtx *filter)
{
filter->request_queue=g_async_queue_new();
filter->keep_running=TRUE;
filter->thread=g_thread_create(gst_dmtx_worker_thread, filter, TRUE, NULL);
if (!filter->thread) {
	g_warning("dmtx: thread start failed");
	return FALSE;
}
return TRUE;
}

static void
gst_dmtx_stop_thread(Gstdmtx *filter)
{
gchar *dummy="s";
if (filter->thread) {
	filter->keep_running=FALSE;
	g_async_queue_push(filter->request_queue, dummy);
	g_thread_join(filter->thread);
	filter->thread=NULL;
}
if (filter->request_queue) {
	g_async_queue_unref(filter->request_queue);
	filter->request_queue=NULL;
}
}

static gboolean
gst_dmtx_start(GstBaseTransform *base)
{
Gstdmtx *filter=GST_DMTX(base);

if (filter->skip>0)
	return gst_dmtx_start_thread(filter);

return TRUE;
}

static gboolean
gst_dmtx_stop(GstBaseTransform *base)
{
gst_dmtx_stop_thread(GST_DMTX(base));

return TRUE;
}

static void
gst_dmtx_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
Gstdmtx *filter=GST_DMTX(object);

switch (prop_id) {
	case PROP_SKIP_DUPS:
		GST_OBJECT_LOCK(object);
		filter->skip_dups=g_value_get_boolean (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SILENT:
		GST_OBJECT_LOCK(object);
		filter->silent=g_value_get_boolean (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SCALE:
		GST_OBJECT_LOCK(object);
		filter->scale=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_TIMEOUT:
		GST_OBJECT_LOCK(object);
		filter->timeout=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_STOP_AFTER:
		GST_OBJECT_LOCK(object);
		filter->stop_after=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SKIP:
		GST_OBJECT_LOCK(object);
		filter->skip=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_TYPE:
		GST_OBJECT_LOCK(object);
		filter->dtype=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SCAN_GAP:
		GST_OBJECT_LOCK(object);
		filter->scan_gap=g_value_get_int (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_USE_REGION:
		GST_OBJECT_LOCK(object);
		filter->use_region=g_value_get_boolean (value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_REGION_X_MAX:
		GST_OBJECT_LOCK(object);
		filter->x_max=g_value_get_int(value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_REGION_X_MIN:
		GST_OBJECT_LOCK(object);
		filter->x_min=g_value_get_int(value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_REGION_Y_MAX:
		GST_OBJECT_LOCK(object);
		filter->y_max=g_value_get_int(value);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_REGION_Y_MIN:
		GST_OBJECT_LOCK(object);
		filter->y_min=g_value_get_int(value);
		GST_OBJECT_UNLOCK(object);
	break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
}
}

static void
gst_dmtx_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
Gstdmtx *filter=GST_DMTX (object);

switch (prop_id) {
	case PROP_SKIP_DUPS:
		GST_OBJECT_LOCK(object);
		g_value_set_boolean (value, filter->skip_dups);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SILENT:
		GST_OBJECT_LOCK(object);
		g_value_set_boolean (value, filter->silent);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SCALE:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->scale);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_TIMEOUT:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->timeout);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_STOP_AFTER:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->stop_after);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SKIP:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->skip);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_TYPE:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->dtype);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_SCAN_GAP:
		GST_OBJECT_LOCK(object);
		g_value_set_int (value, filter->scan_gap);
		GST_OBJECT_UNLOCK(object);
	break;
	case PROP_USE_REGION:
		GST_OBJECT_LOCK(object);
		g_value_set_boolean (value, filter->use_region);
		GST_OBJECT_UNLOCK(object);
	break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
}
}

inline GstMessage *
gst_dmtx_message_new(Gstdmtx *filter, DmtxMessage *msg, GstBuffer *outbuf)
{
GstStructure *s;
GString *tmp;

tmp=g_string_new_len(msg->output, msg->outputSize);
if (filter->skip_dups && g_strcmp0(msg->output, filter->last)==0)
	return NULL;

filter->last=g_string_free(tmp, FALSE);
s=gst_structure_new ("barcode", 
	"timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (outbuf),
	"type", G_TYPE_STRING, filter->dtype==GST_DMTX_TYPE_MATRIX ? "datamatrix" : "datamosaic",
	"symbol", G_TYPE_STRING, filter->last, 
	NULL);

return gst_message_new_element(GST_OBJECT(filter), s);
}

/* GstBaseTransform vmethod implementations */

static GstFlowReturn
gst_dmtx_transform_ip_sync(GstBaseTransform *base, GstBuffer *outbuf)
{
Gstdmtx *filter=GST_DMTX (base);
DmtxTime tm;

if (filter->timeout>0)
	tm=dmtxTimeAdd(dmtxTimeNow(), filter->timeout);

filter->dimg=dmtxImageCreate(GST_BUFFER_DATA(outbuf), filter->width, filter->height, filter->dpo);
filter->ddec=dmtxDecodeCreate(filter->dimg, filter->scale);
filter->dreg=dmtxRegionFindNext(filter->ddec, filter->timeout>0 ? &tm : NULL);

dmtxDecodeSetProp(filter->ddec, DmtxPropScanGap, filter->scan_gap);

if (filter->use_region) {
	dmtxDecodeSetProp(filter->ddec, DmtxPropXmin, filter->x_min);
	dmtxDecodeSetProp(filter->ddec, DmtxPropXmax, filter->x_max > filter->width ? filter->width : filter->x_max);
	dmtxDecodeSetProp(filter->ddec, DmtxPropYmin, filter->y_min);
	dmtxDecodeSetProp(filter->ddec, DmtxPropYmax, filter->y_max > filter->height ? filter->height : filter->y_max);
}

if (filter->dreg!=NULL) {
	DmtxMessage *msg;
	GstMessage *m;

	if (filter->dtype==GST_DMTX_TYPE_MATRIX)
		msg=dmtxDecodeMatrixRegion(filter->ddec, filter->dreg, DmtxUndefined);
	else
		msg=dmtxDecodeMosaicRegion(filter->ddec, filter->dreg, DmtxUndefined);

	if(msg != NULL) {
		filter->found_count++;
		if (!filter->silent) {
			m=gst_dmtx_message_new(filter, msg, outbuf);
			if (m)
				gst_element_post_message (GST_ELEMENT (filter), m);
		}
		if (filter->stop_after>0 && filter->found_count>=filter->stop_after)
			gst_pad_push_event(base->srcpad, gst_event_new_eos());
		dmtxMessageDestroy(&msg);
	}
	dmtxRegionDestroy(&filter->dreg);
}

dmtxDecodeDestroy(&filter->ddec);
dmtxImageDestroy(&filter->dimg);

return GST_FLOW_OK;
}

/**
 * gst_dmtx_transform_ip:
 *
 * gstreamer transformation plugin entry point.
 */
static GstFlowReturn
gst_dmtx_transform_ip(GstBaseTransform *base, GstBuffer *outbuf)
{
Gstdmtx *filter=GST_DMTX (base);
gint r;

if (filter->skip==0)
	return gst_dmtx_transform_ip_sync(base, outbuf);

g_return_val_if_fail(filter->thread, GST_FLOW_ERROR);
g_return_val_if_fail(filter->request_queue, GST_FLOW_ERROR);

/* Push the buffer to the thread if it's waiting for us */
if (filter->skip>0 && (outbuf->offset % filter->skip)!=0)
	return GST_FLOW_OK;

r=g_async_queue_length(filter->request_queue);
if (r<=0) {
	GstBuffer *buffer=gst_buffer_copy(outbuf);
	g_async_queue_push(filter->request_queue, buffer);
}

return GST_FLOW_OK;
}

/**
 * gst_dmtx_worker_thread:
 *
 * As barcode analysis can be slow and would stall the pipeline we push the buffer to analyze to this helper thread.
 *
 */
static gpointer
gst_dmtx_worker_thread(gpointer data)
{
GstBaseTransform *base=(GstBaseTransform *)data;
Gstdmtx *filter=(Gstdmtx *)data;

g_async_queue_ref(filter->request_queue);
while (TRUE) {
	gpointer bdata;
	GstBuffer *buffer;

	bdata=g_async_queue_pop(filter->request_queue);
	if (filter->keep_running==FALSE)
		break;

	buffer=(GstBuffer *)bdata;

	GST_OBJECT_LOCK(GST_OBJECT(filter));
	gst_dmtx_transform_ip_sync(base, buffer);
	GST_OBJECT_UNLOCK(GST_OBJECT(filter));
	gst_buffer_unref(buffer);
}
g_async_queue_unref(filter->request_queue);
return NULL;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
dmtx_init (GstPlugin *dmtx)
{
return gst_element_register(dmtx, "dmtx", GST_RANK_NONE, GST_TYPE_DMTX);
}

/* gstreamer looks for this structure to register dmtxs
 *
 */
GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"dmtx",
	"Data Matrix barcodes decoder element",
	dmtx_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/"
)
