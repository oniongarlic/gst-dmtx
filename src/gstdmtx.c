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

/**
 * SECTION:element-dmtx
 *
 * Dmtx scans image buffers for data matrix barcodes, and sends a message if one is found.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! dmtx ! fakesink silent=TRUE
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
  GOT_BARCODE,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_BOX,
  PROP_SCALE,
  PROP_STOP_AFTER,
  PROP_TIMEOUT,
};

/* the capabilities of the inputs and outputs.
 *
 * RGB only for now.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB ";" GST_VIDEO_CAPS_RGBA)
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB ";" GST_VIDEO_CAPS_RGBA)
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

/* GObject vmethod implementations */

static void
gst_dmtx_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

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
gst_dmtx_class_init (GstdmtxClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_dmtx_set_property;
  gobject_class->get_property = gst_dmtx_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_BOX,
    g_param_spec_boolean ("box", "Box", "Draw a box around found barcode",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SCALE,
    g_param_spec_int ("scale", "Scaling", "Scale input for faster operation",
          1, 4, 1, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
    g_param_spec_int ("timeout", "Timeout", "Try this long to find a code in a frame",
          10, 5000, 100, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_STOP_AFTER,
    g_param_spec_int ("stop_after", "Stop after", "Send EOS after this many matches, set to 0 to keep going",
          0, 500, 0, G_PARAM_READWRITE));

  GST_BASE_TRANSFORM_CLASS (klass)->set_caps = GST_DEBUG_FUNCPTR (gst_dmtx_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip = GST_DEBUG_FUNCPTR (gst_dmtx_transform_ip);
}

static gboolean
gst_dmtx_set_caps (GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps)
{
  Gstdmtx *filter;
  GstStructure *structure;
  gboolean ret;
  gint w, h, depth, bpp;

  filter = GST_DMTX (btrans);
  structure = gst_caps_get_structure (incaps, 0);

  ret = gst_structure_get_int (structure, "width", &w);
  ret &= gst_structure_get_int (structure, "height", &h);
  ret &= gst_structure_get_int (structure, "depth", &depth);
  ret &= gst_structure_get_int (structure, "bpp", &bpp);

  if (!ret)
	return FALSE;

  g_debug("Got caps");

  filter->width=w;
  filter->height=h;
  filter->bpp=bpp;

  return TRUE;
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_dmtx_init (Gstdmtx *filter, GstdmtxClass * klass)
{
  filter->silent = FALSE;
  filter->width=0;
  filter->height=0;
  filter->scale=1;
  filter->timeout=100;
  filter->stop_after=0;
  filter->found_count=0;
}

static void
gst_dmtx_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstdmtx *filter = GST_DMTX (object);

  switch (prop_id) {
    case PROP_BOX:
      filter->draw_box = g_value_get_boolean (value);
    break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
    break;
    case PROP_SCALE:
      filter->scale = g_value_get_int (value);
    break;
    case PROP_TIMEOUT:
      filter->timeout = g_value_get_int (value);
    break;
    case PROP_STOP_AFTER:
      filter->stop_after = g_value_get_int (value);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_dmtx_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstdmtx *filter = GST_DMTX (object);

  switch (prop_id) {
    case PROP_BOX:
      g_value_set_boolean (value, filter->draw_box);
    break;
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
    break;
    case PROP_SCALE:
      g_value_set_int (value, filter->scale);
    break;
    case PROP_TIMEOUT:
      g_value_set_int (value, filter->timeout);
    break;
    case PROP_STOP_AFTER:
      g_value_set_int (value, filter->stop_after);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static GstMessage *
gst_dmtx_message_new (Gstdmtx * dmtx, DmtxMessage *msg)
{
GstStructure *s;
GString *tmp;

tmp=g_string_new_len(msg->output, msg->outputIdx);
s=gst_structure_new ("barcode", 
	"message", G_TYPE_STRING, g_string_free(tmp, FALSE), 
	NULL);
return gst_message_new_element (GST_OBJECT(dmtx), s);;
}

static void
gst_dmtx_buffer_draw_box(GstBuffer *buf, Gstdmtx *filter)
{
/* XXX: Write this */
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_dmtx_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  Gstdmtx *filter = GST_DMTX (base);
  DmtxPackOrder dpo;

  g_debug("Transforming: %d %dx%d", filter->bpp, filter->width, filter->height);

  switch (filter->bpp) {
  case 8:
	dpo=DmtxPack8bppK;
  break;
  case 16:
	dpo=DmtxPack16bppRGB;
  break;
  case 24:
	dpo=DmtxPack24bppRGB;
  break;
  case 32:
	dpo=DmtxPack32bppRGBX;
  break;
  default:
	g_warning("Invalid bpp: %d", filter->bpp);
	return GST_FLOW_UNEXPECTED;
  break;
  }
  
  /* Find and decode barcodes from video frame */
  g_debug("Creating filter: %d %d", filter->timeout, filter->scale);
  if (filter->timeout>0)
	dmtxTimeAdd(dmtxTimeNow(), filter->timeout);
  else
	dmtxTimeAdd(dmtxTimeNow(), 100);

  filter->dimg = dmtxImageCreate(GST_BUFFER_DATA(outbuf), filter->width, filter->height, dpo);
  filter->ddec = dmtxDecodeCreate(filter->dimg, filter->scale);
  filter->dreg = dmtxRegionFindNext(filter->ddec, NULL);
  if(filter->dreg != NULL) {
	DmtxMessage *msg;
	GstMessage *m;

	msg = dmtxDecodeMatrixRegion(filter->ddec, filter->dreg, DmtxUndefined);
	if(msg != NULL) {
		g_debug("Found: %d", msg->outputIdx);
		fwrite(msg->output, sizeof(unsigned char), msg->outputIdx, stdout);
		m=gst_dmtx_message_new(filter, msg);
		filter->found_count++;
		if (filter->draw_box)
			gst_dmtx_buffer_draw_box(outbuf);
		gst_element_post_message (GST_ELEMENT (filter), m);
		if (filter->stop_after>0 && filter->found_count>=filter->stop_after)
			gst_pad_push_event(base->srcpad, gst_event_new_eos());
		dmtxMessageDestroy(&msg);
	}
	dmtxRegionDestroy(&filter->dreg);
  } else {
	g_debug("Nothing found");
  }

  dmtxDecodeDestroy(&filter->ddec);
  dmtxImageDestroy(&filter->dimg);

  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
dmtx_init (GstPlugin * dmtx)
{
  return gst_element_register (dmtx, "dmtx", GST_RANK_NONE, GST_TYPE_DMTX);
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
