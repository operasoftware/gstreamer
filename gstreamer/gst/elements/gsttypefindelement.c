/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 *
 * gsttypefind.c: element that detects type of stream
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

/* FIXME: need a better solution for non-seekable streams */

/* way of operation:
 * 1) get a list of all typefind functions sorted best to worst
 * 2) if all elements have been called with all requested data goto 8
 * 3) call all functions once with all available data
 * 4) if a function returns a value >= ARG_MAXIMUM goto 8
 * 5) all functions with a result > ARG_MINIMUM or functions that did not get
 *    all requested data (where peek returned NULL) stay in list
 * 6) seek to requested offset of best function that still has open data 
 *    requests
 * 7) goto 2
 * 8) take best available result and use its caps
 *
 * The element has two scheduling modes:
 *
 * 1) chain based, it will collect buffers and run the typefind function on
 *    the buffer until something is found.
 * 2) getrange based, it will proxy the getrange function to the sinkpad. It
 *    is assumed that the peer element is happy with whatever format we 
 *    eventually read.
 *
 * When the element has no connected srcpad, and the sinkpad can operate in
 * getrange based mode, the element starts its own task to figure out the 
 * type of the stream.
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsttypefindelement.h"
#include "gst/gst_private.h"
#include "gst/gst-i18n-lib.h"
#include "gst/base/gsttypefindhelper.h"

#include <gst/gsttypefind.h>
#include <gst/gstutils.h>
#include <gst/gsterror.h>

GST_DEBUG_CATEGORY_STATIC (gst_type_find_element_debug);
#define GST_CAT_DEFAULT gst_type_find_element_debug

GstElementDetails gst_type_find_element_details =
GST_ELEMENT_DETAILS ("TypeFind",
    "Generic",
    "Finds the media type of a stream",
    "Benjamin Otte <in7y118@public.uni-hamburg.de>");

/* generic templates */
GstStaticPadTemplate type_find_element_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GstStaticPadTemplate type_find_element_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* TypeFind signals and args */
enum
{
  HAVE_TYPE,
  LAST_SIGNAL
};
enum
{
  ARG_0,
  ARG_CAPS,
  ARG_MINIMUM,
  ARG_MAXIMUM
};
enum
{
  MODE_NORMAL,                  /* act as identity */
  MODE_TYPEFIND                 /* do typefinding */
};


#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_type_find_element_debug, "typefind",	  	\
	GST_DEBUG_BG_YELLOW | GST_DEBUG_FG_GREEN, "type finding element");

GST_BOILERPLATE_FULL (GstTypeFindElement, gst_type_find_element, GstElement,
    GST_TYPE_ELEMENT, _do_init);

static void gst_type_find_element_dispose (GObject * object);
static void gst_type_find_element_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_type_find_element_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static const GstEventMask *gst_type_find_element_src_event_mask (GstPad * pad);
static gboolean gst_type_find_element_src_event (GstPad * pad,
    GstEvent * event);
static gboolean gst_type_find_handle_src_query (GstPad * pad,
    GstQueryType type, GstFormat * fmt, gint64 * value);
static GstFlowReturn push_buffer_store (GstTypeFindElement * typefind);

static gboolean gst_type_find_element_handle_event (GstPad * pad,
    GstEvent * event);
static GstFlowReturn gst_type_find_element_chain (GstPad * sinkpad,
    GstBuffer * buffer);
static GstFlowReturn gst_type_find_element_getrange (GstPad * srcpad,
    guint64 offset, guint length, GstBuffer ** buffer);
static gboolean gst_type_find_element_checkgetrange (GstPad * srcpad);
static GstElementStateReturn
gst_type_find_element_change_state (GstElement * element);
static gboolean
gst_type_find_element_activate (GstPad * pad, GstActivateMode mode);

static guint gst_type_find_element_signals[LAST_SIGNAL] = { 0 };

static void
gst_type_find_element_have_type (GstTypeFindElement * typefind,
    guint probability, const GstCaps * caps)
{
  g_assert (typefind->caps == NULL);
  g_assert (caps != NULL);

  GST_INFO_OBJECT (typefind, "found caps %" GST_PTR_FORMAT, caps);
  typefind->caps = gst_caps_copy (caps);
  gst_pad_set_caps (typefind->src, (GstCaps *) caps);
}

static void
gst_type_find_element_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&type_find_element_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&type_find_element_sink_template));
  gst_element_class_set_details (gstelement_class,
      &gst_type_find_element_details);
}
static void
gst_type_find_element_class_init (GstTypeFindElementClass * typefind_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (typefind_class);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (typefind_class);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_type_find_element_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_type_find_element_get_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_type_find_element_dispose);

  typefind_class->have_type = gst_type_find_element_have_type;

  g_object_class_install_property (gobject_class, ARG_CAPS,
      g_param_spec_boxed ("caps", _("caps"),
          _("detected capabilities in stream"), gst_caps_get_type (),
          G_PARAM_READABLE));
  g_object_class_install_property (gobject_class, ARG_MINIMUM,
      g_param_spec_uint ("minimum", _("minimum"),
          "minimum probability required to accept caps", GST_TYPE_FIND_MINIMUM,
          GST_TYPE_FIND_MAXIMUM, GST_TYPE_FIND_MINIMUM, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_MINIMUM,
      g_param_spec_uint ("maximum", _("maximum"),
          "probability to stop typefinding", GST_TYPE_FIND_MINIMUM,
          GST_TYPE_FIND_MAXIMUM, GST_TYPE_FIND_MAXIMUM, G_PARAM_READWRITE));

  gst_type_find_element_signals[HAVE_TYPE] = g_signal_new ("have_type",
      G_TYPE_FROM_CLASS (typefind_class), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstTypeFindElementClass, have_type), NULL, NULL,
      gst_marshal_VOID__UINT_BOXED, G_TYPE_NONE, 2,
      G_TYPE_UINT, GST_TYPE_CAPS | G_SIGNAL_TYPE_STATIC_SCOPE);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_type_find_element_change_state);
}
static void
gst_type_find_element_init (GstTypeFindElement * typefind)
{
  /* sinkpad */
  typefind->sink =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&type_find_element_sink_template), "sink");
  gst_pad_set_activate_function (typefind->sink,
      gst_type_find_element_activate);
  gst_pad_set_chain_function (typefind->sink, gst_type_find_element_chain);
  gst_pad_set_event_function (typefind->sink,
      gst_type_find_element_handle_event);
  gst_element_add_pad (GST_ELEMENT (typefind), typefind->sink);
  /* srcpad */
  typefind->src =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&type_find_element_src_template), "src");
  gst_pad_set_activate_function (typefind->src, gst_type_find_element_activate);
  gst_pad_set_checkgetrange_function (typefind->src,
      gst_type_find_element_checkgetrange);
  gst_pad_set_getrange_function (typefind->src, gst_type_find_element_getrange);
  gst_pad_set_event_function (typefind->src, gst_type_find_element_src_event);
  gst_pad_set_event_mask_function (typefind->src,
      gst_type_find_element_src_event_mask);
  gst_pad_set_query_function (typefind->src,
      GST_DEBUG_FUNCPTR (gst_type_find_handle_src_query));
  gst_pad_use_fixed_caps (typefind->src);
  gst_element_add_pad (GST_ELEMENT (typefind), typefind->src);

  typefind->caps = NULL;
  typefind->min_probability = 1;
  typefind->max_probability = GST_TYPE_FIND_MAXIMUM;

  typefind->store = gst_buffer_store_new ();
}
static void
gst_type_find_element_dispose (GObject * object)
{
  GstTypeFindElement *typefind = GST_TYPE_FIND_ELEMENT (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);

  if (typefind->store) {
    g_object_unref (typefind->store);
    typefind->store = NULL;
  }
}
static void
gst_type_find_element_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTypeFindElement *typefind;

  g_return_if_fail (GST_IS_TYPE_FIND_ELEMENT (object));

  typefind = GST_TYPE_FIND_ELEMENT (object);

  switch (prop_id) {
    case ARG_MINIMUM:
      typefind->min_probability = g_value_get_uint (value);
      g_object_notify (object, "minimum");
      break;
    case ARG_MAXIMUM:
      typefind->max_probability = g_value_get_uint (value);
      g_object_notify (object, "maximum");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
static void
gst_type_find_element_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTypeFindElement *typefind;

  g_return_if_fail (GST_IS_TYPE_FIND_ELEMENT (object));

  typefind = GST_TYPE_FIND_ELEMENT (object);

  switch (prop_id) {
    case ARG_CAPS:
      g_value_set_boxed (value, typefind->caps);
      break;
    case ARG_MINIMUM:
      g_value_set_uint (value, typefind->min_probability);
      break;
    case ARG_MAXIMUM:
      g_value_set_uint (value, typefind->max_probability);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_type_find_handle_src_query (GstPad * pad,
    GstQueryType type, GstFormat * fmt, gint64 * value)
{
  GstTypeFindElement *typefind =
      GST_TYPE_FIND_ELEMENT (gst_pad_get_parent (pad));
  gboolean res;

  res = gst_pad_query (GST_PAD_PEER (typefind->sink), type, fmt, value);
  if (!res)
    return FALSE;

  if (type == GST_QUERY_POSITION && typefind->store != NULL) {
    /* FIXME: this code assumes that there's no discont in the queue */
    switch (*fmt) {
      case GST_FORMAT_BYTES:
        *value -= gst_buffer_store_get_size (typefind->store, 0);
        break;
      default:
        /* FIXME */
        break;
    }
  }

  return TRUE;
}

static const GstEventMask *
gst_type_find_element_src_event_mask (GstPad * pad)
{
  static const GstEventMask mask[] = {
    {GST_EVENT_SEEK,
        GST_SEEK_METHOD_SET | GST_SEEK_METHOD_CUR | GST_SEEK_METHOD_END |
          GST_SEEK_FLAG_FLUSH},
    /* add more if you want, event masks suck and need to die anyway */
    {0,}
  };

  return mask;
}

static gboolean
gst_type_find_element_src_event (GstPad * pad, GstEvent * event)
{
  GstTypeFindElement *typefind = GST_TYPE_FIND_ELEMENT (GST_PAD_PARENT (pad));

  if (typefind->mode != MODE_NORMAL) {
    /* need to do more? */
    gst_data_unref (GST_DATA (event));
    return FALSE;
  }
  return gst_pad_event_default (pad, event);
}
typedef struct
{
  GstTypeFindFactory *factory;
  gint probability;
  GstCaps *caps;
  gint64 requested_offset;
  guint requested_size;

  GList *buffers;
  GstTypeFindElement *self;
}
TypeFindEntry;

static inline TypeFindEntry *
new_entry (void)
{
  return g_new0 (TypeFindEntry, 1);
}
static void
free_entry_buffers (TypeFindEntry * entry)
{
  g_list_foreach (entry->buffers, (GFunc) gst_data_unref, NULL);
  g_list_free (entry->buffers);
  entry->buffers = NULL;
}
static void
free_entry (TypeFindEntry * entry)
{
  free_entry_buffers (entry);

  if (entry->caps)
    gst_caps_unref (entry->caps);
  g_free (entry);
}
static void
start_typefinding (GstTypeFindElement * typefind)
{
  g_assert (typefind->possibilities == NULL);

  GST_DEBUG_OBJECT (typefind, "starting typefinding");
  gst_pad_set_caps (typefind->src, NULL);
  if (typefind->caps) {
    gst_caps_replace (&typefind->caps, NULL);
  }
  typefind->mode = MODE_TYPEFIND;
  typefind->stream_length_available = TRUE;
  typefind->stream_length = 0;
}
static void
stop_typefinding (GstTypeFindElement * typefind)
{
  GstElementState state;

  /* stop all typefinding and set mode back to normal */
  gboolean push_cached_buffers;

  gst_element_get_state (GST_ELEMENT (typefind), &state, NULL, NULL);

  push_cached_buffers = (state >= GST_STATE_PAUSED);

  GST_DEBUG_OBJECT (typefind, "stopping typefinding%s",
      push_cached_buffers ? " and pushing cached buffers" : "");
  if (typefind->possibilities != NULL) {
    /* this should only happen on PAUSED => READY or EOS */
    GST_LOG_OBJECT (typefind, "freeing remaining %u typefind functions",
        g_list_length (typefind->possibilities));
    g_list_foreach (typefind->possibilities, (GFunc) free_entry, NULL);
    g_list_free (typefind->possibilities);
    typefind->possibilities = NULL;
  }
  //typefind->mode = MODE_TRANSITION;

  if (!push_cached_buffers) {
    gst_buffer_store_clear (typefind->store);
  } else {
    typefind->mode = MODE_NORMAL;
    /* push out our queued buffers here */
    push_buffer_store (typefind);
  }
}

static GstFlowReturn
push_buffer_store (GstTypeFindElement * typefind)
{
  guint size = gst_buffer_store_get_size (typefind->store, 0);
  GstBuffer *buffer;
  GstFlowReturn ret;

  if (size && (buffer = gst_buffer_store_get_buffer (typefind->store, 0, size))) {
    GST_DEBUG_OBJECT (typefind, "pushing cached data (%u bytes)", size);
    ret = gst_pad_push (typefind->src, buffer);
  } else {
    size = 0;
    ret = GST_FLOW_ERROR;
  }

  gst_buffer_store_clear (typefind->store);

  return ret;
}

static guint64
find_element_get_length (gpointer data)
{
  TypeFindEntry *entry = (TypeFindEntry *) data;
  GstTypeFindElement *typefind = entry->self;
  GstFormat format = GST_FORMAT_BYTES;

  if (!typefind->stream_length_available) {
    GST_LOG_OBJECT (entry->self,
        "'%s' called get_length () but we know it's not available",
        GST_PLUGIN_FEATURE_NAME (entry->factory));
    return 0;
  }
  if (entry->self->stream_length == 0) {
    typefind->stream_length_available =
        gst_pad_query (GST_PAD_PEER (entry->self->sink), GST_QUERY_TOTAL,
        &format, &entry->self->stream_length);
    if (format != GST_FORMAT_BYTES)
      typefind->stream_length_available = FALSE;
    if (!typefind->stream_length_available) {
      GST_DEBUG_OBJECT (entry->self,
          "'%s' called get_length () but it's not available",
          GST_PLUGIN_FEATURE_NAME (entry->factory));
      return 0;
    } else {
      GST_DEBUG_OBJECT (entry->self,
          "'%s' called get_length () and it's %" G_GUINT64_FORMAT " bytes",
          GST_PLUGIN_FEATURE_NAME (entry->factory), entry->self->stream_length);
    }
  }

  return entry->self->stream_length;
}

static gboolean
gst_type_find_element_handle_event (GstPad * pad, GstEvent * event)
{
  gboolean res = FALSE;
  TypeFindEntry *entry;
  GstTypeFindElement *typefind = GST_TYPE_FIND_ELEMENT (GST_PAD_PARENT (pad));

  GST_DEBUG_OBJECT (typefind, "got event %d in mode %d", GST_EVENT_TYPE (event),
      typefind->mode);

  switch (typefind->mode) {
    case MODE_TYPEFIND:
      switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_EOS:
          /* this should only happen when we got all available data */
          entry =
              (TypeFindEntry *) typefind->possibilities ? typefind->
              possibilities->data : NULL;
          if (entry && entry->probability >= typefind->min_probability) {
            GST_INFO_OBJECT (typefind,
                "'%s' is the best typefind left after we got all data, using it now (probability %u)",
                GST_PLUGIN_FEATURE_NAME (entry->factory), entry->probability);
            g_signal_emit (typefind, gst_type_find_element_signals[HAVE_TYPE],
                0, entry->probability, entry->caps);
            stop_typefinding (typefind);
            push_buffer_store (typefind);
            res = gst_pad_event_default (pad, event);
          } else {
            res = gst_pad_event_default (pad, event);
            GST_ELEMENT_ERROR (typefind, STREAM, TYPE_NOT_FOUND, (NULL),
                (NULL));
            stop_typefinding (typefind);
          }
          break;
        default:
          gst_data_unref (GST_DATA (event));
          res = TRUE;
          break;
      }
      break;
    case MODE_NORMAL:
      if (GST_EVENT_TYPE (event) == GST_EVENT_DISCONTINUOUS) {
        start_typefinding (typefind);
        gst_event_unref (event);
        res = TRUE;
      } else {
        res = gst_pad_event_default (pad, event);
      }
      break;
    default:
      g_assert_not_reached ();
  }
  return res;
}
static guint8 *
find_peek (gpointer data, gint64 offset, guint size)
{
  GstBuffer *buf;
  TypeFindEntry *entry = (TypeFindEntry *) data;

  GST_LOG_OBJECT (entry->self, "'%s' called peek (%" G_GINT64_FORMAT ", %u)",
      GST_PLUGIN_FEATURE_NAME (entry->factory), offset, size);
  if (offset >= 0) {
    buf = gst_buffer_store_get_buffer (entry->self->store, offset, size);
  } else {
    /* FIXME: can we do this easily without querying length? */
    guint64 length = find_element_get_length (data);

    if (length == 0) {
      buf = NULL;
    } else {
      buf =
          gst_buffer_store_get_buffer (entry->self->store, length + offset,
          size);
    }
  }

  if (buf) {
    entry->buffers = g_list_prepend (entry->buffers, buf);
    return GST_BUFFER_DATA (buf);
  } else {
    if (entry->requested_size == 0) {
      GST_LOG_OBJECT (entry->self,
          "setting requested peek (%" G_GINT64_FORMAT ", %u) on '%s'", offset,
          size, GST_PLUGIN_FEATURE_NAME (entry->factory));
      entry->requested_offset = offset;
      entry->requested_size = size;
    }
    return NULL;
  }
}
static void
find_suggest (gpointer data, guint probability, const GstCaps * caps)
{
  TypeFindEntry *entry = (TypeFindEntry *) data;

  GST_LOG_OBJECT (entry->self, "'%s' called suggest (%u, %" GST_PTR_FORMAT ")",
      GST_PLUGIN_FEATURE_NAME (entry->factory), probability, caps);
  if (((gint) probability) > entry->probability) {
    entry->probability = probability;
    gst_caps_replace (&entry->caps, (GstCaps *) caps);
  }
}

static gint
compare_type_find_entry (gconstpointer a, gconstpointer b)
{
  TypeFindEntry *one = (TypeFindEntry *) a;
  TypeFindEntry *two = (TypeFindEntry *) b;

  if (one->probability == two->probability) {
    /* FIXME: can be improved by analyzing requests */
    return 0;
  } else {
    return two->probability - one->probability;
  }
}

static gint
compare_type_find_factory (gconstpointer fac1, gconstpointer fac2)
{
  return GST_PLUGIN_FEATURE (fac1)->rank - GST_PLUGIN_FEATURE (fac2)->rank;
}

static GstFlowReturn
gst_type_find_element_chain (GstPad * pad, GstBuffer * buffer)
{
  GstTypeFindElement *typefind;
  GList *entries;
  TypeFindEntry *entry;
  GList *walk;
  GstFlowReturn res = GST_FLOW_OK;
  GstTypeFind find = { find_peek, find_suggest, NULL, find_element_get_length };

  typefind = GST_TYPE_FIND_ELEMENT (GST_PAD_PARENT (pad));

  switch (typefind->mode) {
    case MODE_NORMAL:
      return gst_pad_push (typefind->src, buffer);
    case MODE_TYPEFIND:{
      guint64 current_offset;

      gst_buffer_store_add_buffer (typefind->store, buffer);
      current_offset = GST_BUFFER_OFFSET_IS_VALID (buffer) ?
          GST_BUFFER_OFFSET (buffer) + GST_BUFFER_SIZE (buffer) :
          gst_buffer_store_get_size (typefind->store, 0);
      gst_buffer_unref (buffer);

      if (typefind->possibilities == NULL) {
        /* not yet started, get all typefinding functions into our "queue" */
        GList *all_factories = gst_type_find_factory_get_list ();

        GST_INFO_OBJECT (typefind, "starting with %u typefinding functions",
            g_list_length ((GList *) all_factories));

        all_factories = g_list_sort (all_factories, compare_type_find_factory);
        walk = all_factories;
        while (all_factories) {
          entry = new_entry ();

          entry->factory = GST_TYPE_FIND_FACTORY (all_factories->data);
          entry->self = typefind;
          entry->probability = 0;
          typefind->possibilities =
              g_list_prepend (typefind->possibilities, entry);
          all_factories = g_list_next (all_factories);
        }
        g_list_free (all_factories);
      }
      /* call every typefind function once */
      walk = entries = typefind->possibilities;
      GST_INFO_OBJECT (typefind, "iterating %u typefinding functions",
          g_list_length (entries));
      typefind->possibilities = NULL;
      while (walk) {
        find.data = entry = (TypeFindEntry *) walk->data;
        walk = g_list_next (walk);
        entry->probability = 0;
        entry->requested_offset = 0;
        entry->requested_size = 0;
        gst_type_find_factory_call_function (entry->factory, &find);
        free_entry_buffers (entry);
        if (entry->probability == 0 && entry->requested_size == 0) {
          GST_DEBUG_OBJECT (typefind,
              "'%s' was removed - no chance of being the right plugin",
              GST_PLUGIN_FEATURE_NAME (entry->factory));
          free_entry (entry);
        } else if (entry->probability >= typefind->max_probability) {
          /* wooha, got caps */
          GstCaps *found_caps = entry->caps;
          guint probability = entry->probability;

          GST_INFO_OBJECT (typefind,
              "'%s' returned %u/%u probability, using it NOW",
              GST_PLUGIN_FEATURE_NAME (entry->factory), probability,
              typefind->max_probability);
          while (walk) {
            free_entry ((TypeFindEntry *) walk->data);
            walk = g_list_next (walk);
          }
          walk = typefind->possibilities;
          while (walk) {
            free_entry (walk->data);
            walk = g_list_next (walk);
          }
          typefind->possibilities = NULL;
          g_list_free (typefind->possibilities);
          g_signal_emit (typefind, gst_type_find_element_signals[HAVE_TYPE], 0,
              probability, found_caps);
          free_entry (entry);
        } else {
          typefind->possibilities =
              g_list_prepend (typefind->possibilities, entry);
        }
      }
      g_list_free (entries);
      /* we may now already have caps or we might be left without functions to try */
      if (typefind->caps) {
        stop_typefinding (typefind);
      } else if (typefind->possibilities == NULL) {
      error:
        GST_ELEMENT_ERROR (typefind, STREAM, TYPE_NOT_FOUND, (NULL), (NULL));
        stop_typefinding (typefind);
      } else {
        /* set up typefind element for next iteration */
        typefind->possibilities =
            g_list_sort (typefind->possibilities, compare_type_find_entry);

        /* look for typefind functions that require data without seeking */
        for (walk = typefind->possibilities; walk; walk = g_list_next (walk)) {
          entry = (TypeFindEntry *) walk->data;
          if (entry->requested_offset <= current_offset &&
              entry->requested_offset + entry->requested_size > current_offset)
            break;
        }
        if (!walk) {
          /* find out if we should seek */
        restart:
          for (walk = typefind->possibilities; walk; walk = g_list_next (walk)) {
            entry = (TypeFindEntry *) walk->data;
            if (entry->requested_size > 0) {
              /* FIXME: need heuristic to find out if we should seek */
              gint64 seek_offset;
              GstEvent *event;

              seek_offset =
                  entry->requested_offset >
                  0 ? entry->
                  requested_offset : find_element_get_length (entry) +
                  entry->requested_offset;
              seek_offset +=
                  gst_buffer_store_get_size (typefind->store, seek_offset);
              event =
                  gst_event_new_seek (GST_FORMAT_BYTES | GST_SEEK_METHOD_SET,
                  seek_offset);
              if (gst_pad_send_event (GST_PAD_PEER (typefind->sink), event)) {
                /* done seeking */
                GST_DEBUG_OBJECT (typefind,
                    "'%s' was reset - seeked to %" G_GINT64_FORMAT,
                    GST_PLUGIN_FEATURE_NAME (entry->factory), seek_offset);
                break;
              } else if (entry->requested_offset < 0) {
                /* impossible to seek */
                GST_DEBUG_OBJECT (typefind,
                    "'%s' was reset - couldn't seek to %" G_GINT64_FORMAT,
                    GST_PLUGIN_FEATURE_NAME (entry->factory), seek_offset);
                if (entry->probability == 0) {
                  free_entry (entry);
                  typefind->possibilities =
                      g_list_delete_link (typefind->possibilities, walk);
                  /* FIXME: too many gotos */
                  if (!typefind->possibilities)
                    goto error;
                  /* we modified the list, let's restart */
                  goto restart;
                } else {
                  entry->requested_size = 0;
                  entry->requested_offset = 0;
                }
              }
            }
          }
        }
        /* throw out all entries that can't get more data */
        walk = g_list_next (typefind->possibilities);
        while (walk) {
          GList *cur = walk;

          entry = (TypeFindEntry *) walk->data;
          walk = g_list_next (walk);
          if (entry->requested_size == 0) {
            GST_DEBUG_OBJECT (typefind,
                "'%s' was removed - higher possibilities available",
                GST_PLUGIN_FEATURE_NAME (entry->factory));
            free_entry (entry);
            typefind->possibilities =
                g_list_delete_link (typefind->possibilities, cur);
          }
        }
        if (g_list_next (typefind->possibilities) == NULL) {
          entry = (TypeFindEntry *) typefind->possibilities->data;
          if (entry->probability > typefind->min_probability) {
            GST_INFO_OBJECT (typefind,
                "'%s' is the only typefind left, using it now (probability %u)",
                GST_PLUGIN_FEATURE_NAME (entry->factory), entry->probability);
            g_signal_emit (typefind, gst_type_find_element_signals[HAVE_TYPE],
                0, entry->probability, entry->caps);
            free_entry (entry);
            g_list_free (typefind->possibilities);
            typefind->possibilities = NULL;
            stop_typefinding (typefind);
          }
        }
      }
      break;
    }
    default:
      g_assert_not_reached ();
      return GST_FLOW_ERROR;
  }

  return res;
}

static gboolean
gst_type_find_element_checkgetrange (GstPad * srcpad)
{
  GstTypeFindElement *typefind;

  typefind = GST_TYPE_FIND_ELEMENT (GST_PAD_PARENT (srcpad));

  return gst_pad_check_pull_range (typefind->sink);
}

static GstFlowReturn
gst_type_find_element_getrange (GstPad * srcpad,
    guint64 offset, guint length, GstBuffer ** buffer)
{
  GstTypeFindElement *typefind;

  typefind = GST_TYPE_FIND_ELEMENT (GST_PAD_PARENT (srcpad));

  return gst_pad_pull_range (typefind->sink, offset, length, buffer);
}

static gboolean
do_typefind (GstTypeFindElement * typefind)
{
  GstCaps *caps;
  GstPad *peer;

  peer = gst_pad_get_peer (typefind->sink);
  if (peer) {
    gst_pad_peer_set_active (typefind->sink, GST_ACTIVATE_PULL);

    caps = gst_type_find_helper (peer, 0);
    gst_pad_set_caps (typefind->src, caps);

    if (caps) {
      g_signal_emit (typefind, gst_type_find_element_signals[HAVE_TYPE],
          0, 100, caps);
    }

    gst_object_unref (GST_OBJECT (peer));
  }

  return TRUE;
}

static gboolean
gst_type_find_element_activate (GstPad * pad, GstActivateMode mode)
{
  gboolean result;
  GstTypeFindElement *typefind;

  typefind = GST_TYPE_FIND_ELEMENT (GST_OBJECT_PARENT (pad));

  switch (mode) {
    case GST_ACTIVATE_PUSH:
    case GST_ACTIVATE_PULL:
      result = TRUE;
      break;
    default:
      result = TRUE;
      break;
  }

  return result;
}

static GstElementStateReturn
gst_type_find_element_change_state (GstElement * element)
{
  GstElementState transition;
  GstElementStateReturn ret;
  GstTypeFindElement *typefind;

  typefind = GST_TYPE_FIND_ELEMENT (element);

  transition = GST_STATE_TRANSITION (element);
  switch (transition) {
    case GST_STATE_READY_TO_PAUSED:
      do_typefind (typefind);

      //start_typefinding (typefind);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element);

  switch (transition) {
    case GST_STATE_PAUSED_TO_READY:
      //stop_typefinding (typefind);
      gst_caps_replace (&typefind->caps, NULL);
      break;
    default:
      break;
  }

  return ret;
}
