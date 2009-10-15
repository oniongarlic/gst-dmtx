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
 * Dmtx scan image buffers for barcodes, and sends a signal if one is found.
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
#include <gst/controller/gstcontroller.h>

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
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA ";" GST_VIDEO_CAPS_RGB)
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

/* debug category for fltering log messages
 *
 * FIXME:exchange the string 'Template dmtx' with your description
 */
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_dmtx_debug, "dmtx", 0, "dmtx");

GST_BOILERPLATE_FULL (Gstdmtx, gst_dmtx, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_dmtx_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dmtx_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_dmtx_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

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

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
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
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_dmtx_transform_ip);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_dmtx_init (Gstdmtx *filter, GstdmtxClass * klass)
{
  filter->silent = FALSE;
}

static void
gst_dmtx_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstdmtx *filter = GST_DMTX (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
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
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_dmtx_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  Gstdmtx *filter = GST_DMTX (base);
  guint width, height;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (G_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");
  
  /* Find and decode barcodes from video frame */
  filter->dimg = dmtxImageCreate(GST_BUFFER_DATA(outbuf), width, height, DmtxPack24bppRGB);
  filter->ddec = dmtxDecodeCreate(filter->dimg, 1);
  filter->dreg = dmtxRegionFindNext(filter->ddec, NULL);
  if(filter->dreg != NULL) {
	DmtxMessage *msg;
	msg = dmtxDecodeMatrixRegion(filter->ddec, filter->dreg, DmtxUndefined);
	if(msg != NULL) {
		fputs("output: \"", stdout);
		fwrite(msg->output, sizeof(unsigned char), msg->outputIdx, stdout);
		fputs("\"\n", stdout);
		dmtxMessageDestroy(&msg);
	}
	dmtxRegionDestroy(&filter->dreg);
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
  /* initialize gst controller library */
  gst_controller_init(NULL, NULL);

  return gst_element_register (dmtx, "dmtx", GST_RANK_NONE,
      GST_TYPE_DMTX);
}

/* gstreamer looks for this structure to register dmtxs
 *
 * FIXME:exchange the string 'Template dmtx' with you dmtx description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dmtx",
    "Template dmtx",
    dmtx_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
