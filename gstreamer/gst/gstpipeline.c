/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2004,2005 Wim Taymans <wim@fluendo.com>
 *
 * gstpipeline.c: Overall pipeline management element
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

#include "gst_private.h"

#include "gstpipeline.h"
#include "gstinfo.h"
#include "gstscheduler.h"
#include "gstsystemclock.h"

static GstElementDetails gst_pipeline_details =
GST_ELEMENT_DETAILS ("Pipeline object",
    "Generic/Bin",
    "Complete pipeline object",
    "Erik Walthinsen <omega@cse.ogi.edu>, Wim Taymans <wim@fluendo.com>");

/* Pipeline signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_DELAY 0
#define DEFAULT_PLAY_TIMEOUT  (2*GST_SECOND)
enum
{
  ARG_0,
  ARG_DELAY,
  ARG_PLAY_TIMEOUT,
  /* FILL ME */
};


static void gst_pipeline_base_init (gpointer g_class);
static void gst_pipeline_class_init (gpointer g_class, gpointer class_data);
static void gst_pipeline_init (GTypeInstance * instance, gpointer g_class);

static void gst_pipeline_dispose (GObject * object);
static void gst_pipeline_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_pipeline_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstBusSyncReply pipeline_bus_handler (GstBus * bus, GstMessage * message,
    GstPipeline * pipeline);

static GstClock *gst_pipeline_get_clock_func (GstElement * element);
static GstElementStateReturn gst_pipeline_change_state (GstElement * element);

static GstBinClass *parent_class = NULL;

/* static guint gst_pipeline_signals[LAST_SIGNAL] = { 0 }; */

GType
gst_pipeline_get_type (void)
{
  static GType pipeline_type = 0;

  if (!pipeline_type) {
    static const GTypeInfo pipeline_info = {
      sizeof (GstPipelineClass),
      gst_pipeline_base_init,
      NULL,
      (GClassInitFunc) gst_pipeline_class_init,
      NULL,
      NULL,
      sizeof (GstPipeline),
      0,
      gst_pipeline_init,
      NULL
    };

    pipeline_type =
        g_type_register_static (GST_TYPE_BIN, "GstPipeline", &pipeline_info, 0);
  }
  return pipeline_type;
}

static void
gst_pipeline_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &gst_pipeline_details);
}

static void
gst_pipeline_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);
  GstPipelineClass *klass = GST_PIPELINE_CLASS (g_class);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_pipeline_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_pipeline_get_property);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_DELAY,
      g_param_spec_uint64 ("delay", "Delay",
          "Expected delay needed for elements "
          "to spin up to PLAYING in nanoseconds", 0, G_MAXUINT64, DEFAULT_DELAY,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PLAY_TIMEOUT,
      g_param_spec_uint64 ("play-timeout", "Play Timeout",
          "Max timeout for going " "to PLAYING in nanoseconds", 0, G_MAXUINT64,
          DEFAULT_PLAY_TIMEOUT, G_PARAM_READWRITE));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_pipeline_dispose);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_pipeline_change_state);
  gstelement_class->get_clock = GST_DEBUG_FUNCPTR (gst_pipeline_get_clock_func);
}

static void
gst_pipeline_init (GTypeInstance * instance, gpointer g_class)
{
  GstScheduler *scheduler;
  GstPipeline *pipeline = GST_PIPELINE (instance);
  GstBus *bus;

  /* get an instance of the default scheduler */
  scheduler = gst_scheduler_factory_make (NULL, GST_ELEMENT (pipeline));

  /* FIXME need better error handling */
  if (scheduler == NULL) {
    const gchar *name = gst_scheduler_factory_get_default_name ();

    g_error ("Critical error: could not get scheduler \"%s\"\n"
        "Are you sure you have a registry ?\n"
        "Run gst-register as root if you haven't done so yet.", name);
  }
  bus = g_object_new (gst_bus_get_type (), NULL);
  gst_bus_set_sync_handler (bus,
      (GstBusSyncHandler) pipeline_bus_handler, pipeline);
  pipeline->eosed = NULL;
  pipeline->delay = DEFAULT_DELAY;
  pipeline->play_timeout = DEFAULT_PLAY_TIMEOUT;
  /* we are our own manager */
  GST_ELEMENT_MANAGER (pipeline) = pipeline;
  gst_element_set_bus (GST_ELEMENT (pipeline), bus);
  gst_element_set_scheduler (GST_ELEMENT (pipeline), scheduler);
}

static void
gst_pipeline_dispose (GObject * object)
{
  GstPipeline *pipeline = GST_PIPELINE (object);

  gst_scheduler_reset (GST_ELEMENT_SCHEDULER (object));
  gst_object_replace ((GstObject **) & pipeline->fixed_clock, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_pipeline_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPipeline *pipeline = GST_PIPELINE (object);

  GST_LOCK (pipeline);
  switch (prop_id) {
    case ARG_DELAY:
      pipeline->delay = g_value_get_uint64 (value);
      break;
    case ARG_PLAY_TIMEOUT:
      pipeline->play_timeout = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_UNLOCK (pipeline);
}

static void
gst_pipeline_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPipeline *pipeline = GST_PIPELINE (object);

  GST_LOCK (pipeline);
  switch (prop_id) {
    case ARG_DELAY:
      g_value_set_uint64 (value, pipeline->delay);
      break;
    case ARG_PLAY_TIMEOUT:
      g_value_set_uint64 (value, pipeline->play_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_UNLOCK (pipeline);
}

static gboolean
is_eos (GstPipeline * pipeline)
{
  GstIterator *sinks;
  gboolean result = TRUE;
  gboolean done = FALSE;

  sinks = gst_bin_iterate_sinks (GST_BIN (pipeline));
  while (!done) {
    gpointer data;

    switch (gst_iterator_next (sinks, &data)) {
      case GST_ITERATOR_OK:
      {
        GstElement *element = GST_ELEMENT (data);
        GList *eosed;
        GstElementState state, pending;
        GstElementStateReturn complete;
        gchar *name;

        complete = gst_element_get_state (element, &state, &pending, NULL);
        name = gst_element_get_name (element);

        if (complete == GST_STATE_ASYNC) {
          GST_DEBUG ("element %s still performing state change", name);
          result = FALSE;
          done = TRUE;
          goto done;
        } else if (state != GST_STATE_PLAYING) {
          GST_DEBUG ("element %s not playing %d %d", name, state, pending);
          goto done;
        }
        eosed = g_list_find (pipeline->eosed, element);
        if (!eosed) {
          result = FALSE;
          done = TRUE;
        }
      done:
        g_free (name);
        gst_object_unref (GST_OBJECT (element));
        break;
      }
      case GST_ITERATOR_RESYNC:
        result = TRUE;
        gst_iterator_resync (sinks);
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }
  gst_iterator_free (sinks);
  return result;
}

/* FIXME, make me threadsafe */
static GstBusSyncReply
pipeline_bus_handler (GstBus * bus, GstMessage * message,
    GstPipeline * pipeline)
{
  GstBusSyncReply result = GST_BUS_PASS;
  gboolean posteos = FALSE;

  /* we don't want messages from the streaming thread while we're doing the 
   * state change. We do want them from the state change functions. */

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      if (GST_MESSAGE_SRC (message) != GST_OBJECT (pipeline)) {
        GST_LOCK (bus);
        pipeline->eosed =
            g_list_prepend (pipeline->eosed, GST_MESSAGE_SRC (message));
        GST_UNLOCK (bus);
        if (is_eos (pipeline)) {
          posteos = TRUE;
        }
        /* we drop all EOS messages */
        result = GST_BUS_DROP;
        gst_message_unref (message);
      }
      break;
    case GST_MESSAGE_ERROR:
      break;
    default:
      break;
  }

  if (posteos) {
    gst_bus_post (bus, gst_message_new_eos (GST_OBJECT (pipeline)));
  }

  return result;
}

/**
 * gst_pipeline_new:
 * @name: name of new pipeline
 *
 * Create a new pipeline with the given name.
 *
 * Returns: newly created GstPipeline
 *
 * MT safe.
 */
GstElement *
gst_pipeline_new (const gchar * name)
{
  return gst_element_factory_make ("pipeline", name);
}

/* MT safe */
static GstElementStateReturn
gst_pipeline_change_state (GstElement * element)
{
  /* implement me */
  return GST_STATE_FAILURE;
}

/**
 * gst_pipeline_get_scheduler:
 * @pipeline: the pipeline
 *
 * Gets the #GstScheduler of this pipeline.
 *
 * Returns: a GstScheduler.
 *
 * MT safe.
 */
GstScheduler *
gst_pipeline_get_scheduler (GstPipeline * pipeline)
{
  return gst_element_get_scheduler (GST_ELEMENT (pipeline));
}

/**
 * gst_pipeline_get_bus:
 * @pipeline: the pipeline
 *
 * Gets the #GstBus of this pipeline.
 *
 * Returns: a GstBus
 *
 * MT safe.
 */
GstBus *
gst_pipeline_get_bus (GstPipeline * pipeline)
{
  return gst_element_get_bus (GST_ELEMENT (pipeline));
}

static GstClock *
gst_pipeline_get_clock_func (GstElement * element)
{
  GstClock *clock = NULL;
  GstPipeline *pipeline = GST_PIPELINE (element);

  /* if we have a fixed clock, use that one */
  GST_LOCK (pipeline);
  if (GST_FLAG_IS_SET (pipeline, GST_PIPELINE_FLAG_FIXED_CLOCK)) {
    clock = pipeline->fixed_clock;
    gst_object_ref (GST_OBJECT (clock));
    GST_UNLOCK (pipeline);

    GST_CAT_DEBUG (GST_CAT_CLOCK, "pipeline using fixed clock %p (%s)",
        clock, clock ? GST_STR_NULL (GST_OBJECT_NAME (clock)) : "-");
  } else {
    GST_UNLOCK (pipeline);
    clock =
        GST_ELEMENT_CLASS (parent_class)->get_clock (GST_ELEMENT (pipeline));
    /* no clock, use a system clock */
    if (!clock) {
      clock = gst_system_clock_obtain ();
      /* we unref since this function is not supposed to increase refcount
       * of clock object returned; this is ok since the systemclock always
       * has a refcount of at least one in the current code. */
      gst_object_unref (GST_OBJECT (clock));
      GST_CAT_DEBUG (GST_CAT_CLOCK, "pipeline obtained system clock: %p (%s)",
          clock, clock ? GST_STR_NULL (GST_OBJECT_NAME (clock)) : "-");
    } else {
      GST_CAT_DEBUG (GST_CAT_CLOCK, "pipeline obtained clock: %p (%s)",
          clock, clock ? GST_STR_NULL (GST_OBJECT_NAME (clock)) : "-");
    }
  }
  return clock;
}

/**
 * gst_pipeline_get_clock:
 * @pipeline: the pipeline
 *
 * Gets the current clock used by the pipeline.
 *
 * Returns: a GstClock
 */
GstClock *
gst_pipeline_get_clock (GstPipeline * pipeline)
{
  g_return_val_if_fail (GST_IS_PIPELINE (pipeline), NULL);

  return gst_pipeline_get_clock_func (GST_ELEMENT (pipeline));
}


/**
 * gst_pipeline_use_clock:
 * @pipeline: the pipeline
 * @clock: the clock to use
 *
 * Force the pipeline to use the given clock. The pipeline will
 * always use the given clock even if new clock providers are added
 * to this pipeline.
 *
 * MT safe.
 */
void
gst_pipeline_use_clock (GstPipeline * pipeline, GstClock * clock)
{
  g_return_if_fail (GST_IS_PIPELINE (pipeline));

  GST_LOCK (pipeline);
  GST_FLAG_SET (pipeline, GST_PIPELINE_FLAG_FIXED_CLOCK);

  gst_object_replace ((GstObject **) & pipeline->fixed_clock,
      (GstObject *) clock);
  GST_LOCK (pipeline);

  GST_CAT_DEBUG (GST_CAT_CLOCK, "pipeline using fixed clock %p (%s)", clock,
      (clock ? GST_OBJECT_NAME (clock) : "nil"));
}

/**
 * gst_pipeline_set_clock:
 * @pipeline: the pipeline
 * @clock: the clock to set
 *
 * Set the clock for the pipeline. The clock will be distributed
 * to all the elements managed by the pipeline.
 *
 * MT safe.
 */
void
gst_pipeline_set_clock (GstPipeline * pipeline, GstClock * clock)
{
  g_return_if_fail (pipeline != NULL);
  g_return_if_fail (GST_IS_PIPELINE (pipeline));

  GST_ELEMENT_CLASS (parent_class)->set_clock (GST_ELEMENT (pipeline), clock);
}

/**
 * gst_pipeline_auto_clock:
 * @pipeline: the pipeline
 *
 * Let the pipeline select a clock automatically.
 *
 * MT safe.
 */
void
gst_pipeline_auto_clock (GstPipeline * pipeline)
{
  g_return_if_fail (pipeline != NULL);
  g_return_if_fail (GST_IS_PIPELINE (pipeline));

  GST_LOCK (pipeline);
  GST_FLAG_UNSET (pipeline, GST_PIPELINE_FLAG_FIXED_CLOCK);

  gst_object_replace ((GstObject **) & pipeline->fixed_clock, NULL);
  GST_UNLOCK (pipeline);

  GST_CAT_DEBUG (GST_CAT_CLOCK, "pipeline using automatic clock");
}
