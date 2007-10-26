/*
 * Farsight Voice+Video library
 *
 *  Copyright 2007 Collabora Ltd, 
 *  Copyright 2007 Nokia Corporation
 *   @author: Olivier Crete <olivier.crete@collabora.co.uk>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvalve.h"

#include <string.h>

GST_DEBUG_CATEGORY (valve_debug);
#define GST_CAT_DEFAULT (valve_debug)

/* elementfactory information */
static const GstElementDetails gst_valve_details =
GST_ELEMENT_DETAILS (
  "Valve element",
  "Filter",
  "This element drops all packets when drop is TRUE",
  "Olivier Crete <olivier.crete@collabora.co.uk>");


static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* Valve signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_DROP,
};




static void gst_valve_set_property (GObject *object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_valve_get_property (GObject *object,
    guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_valve_dispose (GObject *object);

static GstFlowReturn
gst_valve_transform_ip (GstBaseTransform *trans, GstBuffer *buf);
static gboolean
gst_valve_event (GstBaseTransform *trans, GstEvent *event);


static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (valve_debug, "valve", 0, "Valve");
}

GST_BOILERPLATE_FULL (GstValve, gst_valve, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, _do_init);

static void
gst_valve_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_details (element_class, &gst_valve_details);
}

static void
gst_valve_class_init (GstValveClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_valve_dispose);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_valve_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_valve_get_property);

  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_valve_transform_ip);
  gstbasetransform_class->event =
      GST_DEBUG_FUNCPTR (gst_valve_event);
  gstbasetransform_class->src_event =
      GST_DEBUG_FUNCPTR (gst_valve_event);

  g_object_class_install_property (gobject_class, ARG_DROP,
      g_param_spec_boolean ("drop",
        "Drops all buffers if TRUE",
        "If this property if TRUE, the element will drop all buffers, if its FALSE, it will let them through",
          FALSE, G_PARAM_READWRITE));

  parent_class = g_type_class_peek_parent (klass);
}

static void
gst_valve_init (GstValve *valve,
    GstValveClass *klass)
{

  valve->drop = 0;

  gst_base_transform_set_passthrough ((GstBaseTransform *)valve, TRUE);

}

static void
gst_valve_dispose (GObject *object)
{
  GstValve *valve = NULL;
  valve = GST_VALVE (valve);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_valve_set_property (GObject *object,
    guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstValve *valve = GST_VALVE (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    case ARG_DROP:
      GST_OBJECT_LOCK (object);
      valve->drop = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (object);
      break;
  }
}

static void
gst_valve_get_property (GObject *object,
    guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstValve *valve = GST_VALVE (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    case ARG_DROP:
      GST_OBJECT_LOCK (object);
      g_value_set_boolean (value, valve->drop);
      GST_OBJECT_UNLOCK (object);
      break;
  }
}

static GstFlowReturn
gst_valve_transform_ip (GstBaseTransform *trans, GstBuffer *buf)
{
  GstValve *valve = GST_VALVE (trans);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_OBJECT_LOCK (GST_OBJECT (trans));
  if (valve->drop) {
#if GST_VERSION_MAJOR >= 10 &&  GST_VERSION_MICRO >= 13
    ret = GST_BASE_TRANSFORM_FLOW_DROPPED;
#endif
    buf = NULL;
  }
  GST_OBJECT_UNLOCK (GST_OBJECT (trans));

  return ret;
}

static gboolean
gst_valve_event (GstBaseTransform *trans, GstEvent *event)
{
  GstValve *valve = GST_VALVE (trans);
  gboolean ret = TRUE;

  GST_OBJECT_LOCK (GST_OBJECT (trans));
  if (valve->drop)
    ret = FALSE;
  GST_OBJECT_UNLOCK (GST_OBJECT (trans));

  return ret;
}

gboolean
gst_valve_plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "fsvalve",
      GST_RANK_MARGINAL, GST_TYPE_VALVE);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "fsvalve",
    "Valve",
    gst_valve_plugin_init, VERSION, "LGPL", "Farsight", "http://farsight.sf.net")
