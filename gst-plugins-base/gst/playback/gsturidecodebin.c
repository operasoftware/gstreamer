/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim@fluendo.com>
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
 * SECTION:element-uridecodebin
 * @short_description: decoder for an uri
 *
 * Decodes data from a URI into raw media.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include <gst/gst.h>
#include <gst/gst-i18n-plugin.h>

#define GST_TYPE_URI_DECODE_BIN \
  (gst_uri_decode_bin_get_type())
#define GST_URI_DECODE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_URI_DECODE_BIN,GstURIDecodeBin))
#define GST_URI_DECODE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_URI_DECODE_BIN,GstURIDecodeBinClass))
#define GST_IS_URI_DECODE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_URI_DECODE_BIN))
#define GST_IS_URI_DECODE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_URI_DECODE_BIN))

typedef struct _GstURIDecodeBin GstURIDecodeBin;
typedef struct _GstURIDecodeBinClass GstURIDecodeBinClass;

struct _GstURIDecodeBin
{
  GstBin parent_instance;

  gchar *uri;

  gboolean is_stream;
  GstElement *source;
  GstElement *queue;
  GSList *decoders;
  GSList *srcpads;
  gint numpads;

  /* for dynamic sources */
  guint src_np_sig_id;          /* new-pad signal id */
  guint src_nmp_sig_id;         /* no-more-pads signal id */
  gint pending;
};

struct _GstURIDecodeBinClass
{
  GstBinClass parent_class;
};

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_uri_decode_bin_debug);
#define GST_CAT_DEFAULT gst_uri_decode_bin_debug

static const GstElementDetails gst_uri_decode_bin_details =
GST_ELEMENT_DETAILS ("URI Decoder",
    "Generic/Bin/Decoder",
    "Autoplug and decode an URI to raw media",
    "Wim Taymans <wim@fluendo.com>");

#define DEFAULT_PROP_URI	NULL
enum
{
  PROP_0,
  PROP_URI,
  PROP_LAST
};

GST_BOILERPLATE (GstURIDecodeBin, gst_uri_decode_bin, GstBin, GST_TYPE_BIN);

static void gst_uri_decode_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_uri_decode_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_uri_decode_bin_finalize (GObject * obj);

static gboolean gst_uri_decode_bin_query (GstElement * element,
    GstQuery * query);
static GstStateChangeReturn gst_uri_decode_bin_change_state (GstElement *
    element, GstStateChange transition);

static void
gst_uri_decode_bin_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_set_details (gstelement_class, &gst_uri_decode_bin_details);
}

static void
gst_uri_decode_bin_class_init (GstURIDecodeBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbin_class = GST_BIN_CLASS (klass);

  gobject_class->set_property = gst_uri_decode_bin_set_property;
  gobject_class->get_property = gst_uri_decode_bin_get_property;
  gobject_class->finalize = gst_uri_decode_bin_finalize;

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to decode",
          DEFAULT_PROP_URI, G_PARAM_READWRITE));

  gstelement_class->query = GST_DEBUG_FUNCPTR (gst_uri_decode_bin_query);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_uri_decode_bin_change_state);
}

static void
gst_uri_decode_bin_init (GstURIDecodeBin * dec, GstURIDecodeBinClass * klass)
{
  dec->uri = g_strdup (DEFAULT_PROP_URI);
}

static void
gst_uri_decode_bin_finalize (GObject * obj)
{
  GstURIDecodeBin *dec = GST_URI_DECODE_BIN (obj);

  g_free (dec->uri);
  dec->uri = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_uri_decode_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstURIDecodeBin *dec = GST_URI_DECODE_BIN (object);

  switch (prop_id) {
    case PROP_URI:
      g_free (dec->uri);
      dec->uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_uri_decode_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstURIDecodeBin *dec = GST_URI_DECODE_BIN (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, dec->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define DEFAULT_QUEUE_SIZE          (3 * GST_SECOND)
#define DEFAULT_QUEUE_MIN_THRESHOLD ((DEFAULT_QUEUE_SIZE * 30) / 100)
#define DEFAULT_QUEUE_THRESHOLD     ((DEFAULT_QUEUE_SIZE * 95) / 100)

static void
unknown_type (GstElement * element, GstPad * pad, GstCaps * caps,
    GstURIDecodeBin * decoder)
{
  gchar *capsstr;

  capsstr = gst_caps_to_string (caps);
  GST_ELEMENT_WARNING (decoder, STREAM, WRONG_TYPE,
      (_("No decoder available for type \'%s\'."), capsstr), (NULL));
  g_free (capsstr);
}

/* add a streaminfo that indicates that the stream is handled by the
 * given element. This usually means that a stream without actual data is
 * produced but one that is sunken by an element. Examples of this are:
 * cdaudio, a hardware decoder/sink, dvd meta bins etc...
 */
static void
add_element_stream (GstElement * element, GstURIDecodeBin * decoder)
{
  g_warning ("add element stream");
}

/* when the decoder element signals that no more pads will be generated, we
 * can commit the current group.
 */
static void
no_more_pads_full (GstElement * element, gboolean subs,
    GstURIDecodeBin * decoder)
{
  gboolean final;

  /* setup phase */
  GST_DEBUG_OBJECT (element, "no more pads, %d pending", decoder->pending);

  GST_OBJECT_LOCK (decoder);
  final = (decoder->pending == 0);

  /* nothing pending, we can exit */
  if (final)
    goto done;

  /* the object has no pending no_more_pads */
  if (!g_object_get_data (G_OBJECT (element), "pending"))
    goto done;
  g_object_set_data (G_OBJECT (element), "pending", NULL);

  decoder->pending--;
  if (decoder->pending != 0)
    final = FALSE;

done:
  GST_OBJECT_UNLOCK (decoder);

  if (final)
    gst_element_no_more_pads (GST_ELEMENT_CAST (decoder));

  return;
}

static void
no_more_pads (GstElement * element, GstURIDecodeBin * decoder)
{
  no_more_pads_full (element, FALSE, decoder);
}

static void
source_no_more_pads (GstElement * element, GstURIDecodeBin * bin)
{
  GST_DEBUG_OBJECT (bin, "No more pads in source element %s.",
      GST_ELEMENT_NAME (element));

  g_signal_handler_disconnect (G_OBJECT (element), bin->src_np_sig_id);
  bin->src_np_sig_id = 0;
  g_signal_handler_disconnect (G_OBJECT (element), bin->src_nmp_sig_id);
  bin->src_nmp_sig_id = 0;

  no_more_pads_full (element, FALSE, bin);
}

/* Called by the signal handlers when a decodebin has 
 * found a new raw pad.  
 */
static void
new_decoded_pad (GstElement * element, GstPad * pad, gboolean last,
    GstURIDecodeBin * decoder)
{
  GstPad *newpad;
  gchar *padname;

  GST_DEBUG_OBJECT (element, "new decoded pad, name: <%s>. Last: %d",
      GST_PAD_NAME (pad), last);

  GST_OBJECT_LOCK (decoder);
  padname = g_strdup_printf ("src%d", decoder->numpads);
  decoder->numpads++;
  GST_OBJECT_UNLOCK (decoder);

  newpad = gst_ghost_pad_new (padname, pad);
  g_free (padname);

  gst_pad_set_active (newpad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (decoder), newpad);
}

/* helper function to lookup stuff in lists */
static gboolean
array_has_value (const gchar * values[], const gchar * value)
{
  gint i;

  for (i = 0; values[i]; i++) {
    if (g_str_has_prefix (value, values[i]))
      return TRUE;
  }
  return FALSE;
}

/* list of URIs that we consider to be streams and that need buffering.
 * We have no mechanism yet to figure this out with a query. */
static const gchar *stream_uris[] = { "http://", "mms://", "mmsh://",
  "mmsu://", "mmst://", NULL
};

/* blacklisted URIs, we know they will always fail. */
static const gchar *blacklisted_uris[] = { NULL };

/* mime types that we don't consider to be media types */
#if 0
static const gchar *no_media_mimes[] = {
  "application/x-executable", "application/x-bzip", "application/x-gzip",
  "application/zip", "application/x-compress", NULL
};
#endif

/* mime types we consider raw media */
static const gchar *raw_mimes[] = {
  "audio/x-raw", "video/x-raw", NULL
};

#define IS_STREAM_URI(uri)          (array_has_value (stream_uris, uri))
#define IS_BLACKLISTED_URI(uri)     (array_has_value (blacklisted_uris, uri))
#define IS_NO_MEDIA_MIME(mime)      (array_has_value (no_media_mimes, mime))
#define IS_RAW_MIME(mime)           (array_has_value (raw_mimes, mime))

/*
 * Generate and configure a source element.
 */
static GstElement *
gen_source_element (GstURIDecodeBin * decoder)
{
  GstElement *source;

  if (!decoder->uri)
    goto no_uri;

  if (!gst_uri_is_valid (decoder->uri))
    goto invalid_uri;

  if (IS_BLACKLISTED_URI (decoder->uri))
    goto uri_blacklisted;

  source = gst_element_make_from_uri (GST_URI_SRC, decoder->uri, "source");
  if (!source)
    goto no_source;

  decoder->is_stream = IS_STREAM_URI (decoder->uri);

  /* make HTTP sources send extra headers so we get icecast
   * metadata in case the stream is an icecast stream */
  if (!strncmp (decoder->uri, "http://", 7) &&
      g_object_class_find_property (G_OBJECT_GET_CLASS (source),
          "iradio-mode")) {
    g_object_set (source, "iradio-mode", TRUE, NULL);
  }
  return source;

  /* ERRORS */
no_uri:
  {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NOT_FOUND,
        (_("No URI specified to play from.")), (NULL));
    return NULL;
  }
invalid_uri:
  {
    GST_ELEMENT_ERROR (decoder, RESOURCE, NOT_FOUND,
        (_("Invalid URI \"%s\"."), decoder->uri), (NULL));
    return NULL;
  }
uri_blacklisted:
  {
    GST_ELEMENT_ERROR (decoder, RESOURCE, FAILED,
        (_("This stream type cannot be played yet.")), (NULL));
    return NULL;
  }
no_source:
  {
    gchar *prot = gst_uri_get_protocol (decoder->uri);

    /* whoops, could not create the source element, dig a little deeper to
     * figure out what might be wrong. */
    if (prot) {
      GST_ELEMENT_ERROR (decoder, RESOURCE, FAILED,
          (_("No URI handler implemented for \"%s\"."), prot), (NULL));
      g_free (prot);
    } else
      goto invalid_uri;

    return NULL;
  }
}

/**
 * has_all_raw_caps:
 * @pad: a #GstPad
 * @all_raw: pointer to hold the result
 *
 * check if the caps of the pad are all raw. The caps are all raw if
 * all of its structures contain audio/x-raw or video/x-raw.
 *
 * Returns: %FALSE @pad has no caps. Else TRUE and @all_raw set t the result.
 */
static gboolean
has_all_raw_caps (GstPad * pad, gboolean * all_raw)
{
  GstCaps *caps;
  gint capssize;
  guint i, num_raw = 0;
  gboolean res = FALSE;

  caps = gst_pad_get_caps (pad);
  if (caps == NULL)
    return FALSE;

  capssize = gst_caps_get_size (caps);
  /* no caps, skip and move to the next pad */
  if (capssize == 0 || gst_caps_is_empty (caps) || gst_caps_is_any (caps))
    goto done;

  /* count the number of raw formats in the caps */
  for (i = 0; i < capssize; ++i) {
    GstStructure *s;
    const gchar *mime_type;

    s = gst_caps_get_structure (caps, i);
    mime_type = gst_structure_get_name (s);

    if (IS_RAW_MIME (mime_type))
      ++num_raw;
  }

  *all_raw = (num_raw == capssize);
  res = TRUE;

done:
  gst_caps_unref (caps);
  return res;
}

/**
 * analyse_source:
 * @decoder: a #GstURIDecodeBin
 * @is_raw: are all pads raw data
 * @have_out: does the source have output
 * @is_dynamic: is this a dynamic source
 *
 * Check the source of @decoder and collect information about it.
 *
 * @is_raw will be set to TRUE if the source only produces raw pads. When this
 * function returns, all of the raw pad of the source will be added
 * to @decoder.
 *
 * @have_out: will be set to TRUE if the source has output pads.
 *
 * @is_dynamic: TRUE if the element will create (more) pads dynamically later
 * on.
 *
 * Returns: FALSE if a fatal error occured while scanning.
 */
static gboolean
analyse_source (GstURIDecodeBin * decoder, gboolean * is_raw,
    gboolean * have_out, gboolean * is_dynamic)
{
  GstIterator *pads_iter;
  gboolean done = FALSE;
  gboolean res = TRUE;

  *have_out = FALSE;
  *is_raw = FALSE;
  *is_dynamic = FALSE;

  pads_iter = gst_element_iterate_src_pads (decoder->source);
  while (!done) {
    GstPad *pad;

    switch (gst_iterator_next (pads_iter, (gpointer) & pad)) {
      case GST_ITERATOR_ERROR:
        res = FALSE;
        /* FALLTROUGH */
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
      case GST_ITERATOR_RESYNC:
        /* reset results and resync */
        *have_out = FALSE;
        *is_raw = FALSE;
        *is_dynamic = FALSE;
        gst_iterator_resync (pads_iter);
        break;
      case GST_ITERATOR_OK:
        /* we now officially have an ouput pad */
        *have_out = TRUE;

        /* if FALSE, this pad has no caps and we continue with the next pad. */
        if (!has_all_raw_caps (pad, is_raw))
          break;

        /* caps on source pad are all raw, we can add the pad */
        if (*is_raw)
          new_decoded_pad (decoder->source, pad, FALSE, decoder);
        break;
    }
  }
  gst_iterator_free (pads_iter);

  if (!*have_out) {
    GstElementClass *elemclass;
    GList *walk;

    /* element has no output pads, check for padtemplates that list SOMETIMES
     * pads. */
    elemclass = GST_ELEMENT_GET_CLASS (decoder->source);

    walk = gst_element_class_get_pad_template_list (elemclass);
    while (walk != NULL) {
      GstPadTemplate *templ;

      templ = (GstPadTemplate *) walk->data;
      if (GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SRC) {
        if (GST_PAD_TEMPLATE_PRESENCE (templ) == GST_PAD_SOMETIMES)
          *is_dynamic = TRUE;
        break;
      }
      walk = g_list_next (walk);
    }
  }

  return res;
}

static void
remove_decoders (GstURIDecodeBin * bin)
{
  GSList *walk;

  for (walk = bin->decoders; walk; walk = g_slist_next (walk)) {
    GstElement *decoder = GST_ELEMENT_CAST (walk->data);

    GST_DEBUG_OBJECT (bin, "removing old decoder element");
    gst_element_set_state (decoder, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (bin), decoder);
  }
  g_slist_free (bin->decoders);
  bin->decoders = NULL;
}

static void
remove_pads (GstURIDecodeBin * bin)
{
  GSList *walk;

  for (walk = bin->srcpads; walk; walk = g_slist_next (walk)) {
    GstPad *pad = GST_PAD_CAST (walk->data);

    GST_DEBUG_OBJECT (bin, "removing old pad");
    gst_pad_set_active (pad, FALSE);
    gst_element_remove_pad (GST_ELEMENT_CAST (bin), pad);
  }
  g_slist_free (bin->srcpads);
  bin->srcpads = NULL;
}

static GstElement *
make_decoder (GstURIDecodeBin * decoder, gboolean use_queue)
{
  GstElement *result, *decodebin;

  /* now create the decoder element */
  decodebin = gst_element_factory_make ("decodebin2", NULL);
  if (!decodebin)
    goto no_decodebin;

  if (use_queue) {
    GstElement *queue;
    GstPad *gpad, *pad;

    queue = gst_element_factory_make ("queue2", NULL);
    if (!queue)
      goto no_queue2;

    /* configure the queue as a buffering element */
    g_object_set (G_OBJECT (queue), "use-buffering", TRUE, NULL);

    result = gst_bin_new ("source-bin");

    gst_bin_add (GST_BIN_CAST (result), queue);
    gst_bin_add (GST_BIN_CAST (result), decodebin);

    gst_element_link (queue, decodebin);

    pad = gst_element_get_pad (queue, "sink");
    gpad = gst_ghost_pad_new (GST_PAD_NAME (pad), pad);
    gst_object_unref (pad);

    gst_pad_set_active (gpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST (result), gpad);
  } else {
    result = decodebin;
  }

  /* set up callbacks to create the links between decoded data
   * and video/audio/subtitle rendering/output. */
  g_signal_connect (G_OBJECT (decodebin),
      "new-decoded-pad", G_CALLBACK (new_decoded_pad), decoder);
  g_signal_connect (G_OBJECT (decodebin), "no-more-pads",
      G_CALLBACK (no_more_pads), decoder);
  g_signal_connect (G_OBJECT (decodebin),
      "unknown-type", G_CALLBACK (unknown_type), decoder);
  g_object_set_data (G_OBJECT (decodebin), "pending", "1");
  decoder->pending++;

  gst_bin_add (GST_BIN_CAST (decoder), result);

  decoder->decoders = g_slist_prepend (decoder->decoders, result);

  return result;

  /* ERRORS */
no_decodebin:
  {
    GST_ELEMENT_ERROR (decoder, CORE, MISSING_PLUGIN,
        (_("Could not create \"decodebin2\" element.")), (NULL));
    return NULL;
  }
no_queue2:
  {
    GST_ELEMENT_ERROR (decoder, CORE, MISSING_PLUGIN,
        (_("Could not create \"queue2\" element.")), (NULL));
    return NULL;
  }
}

static void
remove_source (GstURIDecodeBin * bin)
{
  GstElement *source = bin->source;

  if (source) {
    GST_DEBUG_OBJECT (bin, "removing old src element");
    gst_element_set_state (source, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (bin), source);

    if (bin->src_np_sig_id) {
      g_signal_handler_disconnect (G_OBJECT (source), bin->src_np_sig_id);
      bin->src_np_sig_id = 0;
    }
    if (bin->src_nmp_sig_id) {
      g_signal_handler_disconnect (G_OBJECT (source), bin->src_nmp_sig_id);
      bin->src_nmp_sig_id = 0;
    }
    bin->source = NULL;
  }
}

/* is called when a dynamic source element created a new pad. */
static void
source_new_pad (GstElement * element, GstPad * pad, GstURIDecodeBin * bin)
{
  GstElement *decoder;
  gboolean is_raw;

  GST_DEBUG_OBJECT (bin, "Found new pad %s.%s in source element %s",
      GST_DEBUG_PAD_NAME (pad), GST_ELEMENT_NAME (element));

  /* if this is a pad with all raw caps, we can expose it */
  if (has_all_raw_caps (pad, &is_raw) && is_raw) {
    /* it's all raw, create output pads. */
    new_decoded_pad (element, pad, FALSE, bin);
    return;
  }

  /* not raw, create decoder */
  decoder = make_decoder (bin, FALSE);
  if (!decoder)
    goto no_decodebin;

  /* and link to decoder */
  if (!gst_element_link (bin->source, decoder))
    goto could_not_link;

  GST_DEBUG_OBJECT (bin, "linked decoder to new pad");

  gst_element_set_state (decoder, GST_STATE_PLAYING);

  return;

  /* ERRORS */
no_decodebin:
  {
    /* error was posted */
    return;
  }
could_not_link:
  {
    GST_ELEMENT_ERROR (bin, CORE, NEGOTIATION,
        (NULL), ("Can't link source to decoder element"));
    return;
  }
}

/* construct and run the source and decoder elements until we found
 * all the streams or until a preroll queue has been filled.
*/
static gboolean
setup_source (GstURIDecodeBin * decoder)
{
  gboolean is_raw, have_out, is_dynamic;

  GST_DEBUG_OBJECT (decoder, "setup source");

  /* delete old src */
  remove_source (decoder);

  /* create and configure an element that can handle the uri */
  if (!(decoder->source = gen_source_element (decoder)))
    goto no_source;

  /* state will be merged later - if file is not found, error will be
   * handled by the application right after. */
  gst_bin_add (GST_BIN_CAST (decoder), decoder->source);

  /* remove the old decoders now, if any */
  remove_decoders (decoder);

  /* see if the source element emits raw audio/video all by itself,
   * if so, we can create streams for the pads and be done with it.
   * Also check that is has source pads, if not, we assume it will
   * do everything itself.  */
  if (!analyse_source (decoder, &is_raw, &have_out, &is_dynamic))
    goto invalid_source;

  if (is_raw) {
    GST_DEBUG_OBJECT (decoder, "Source provides all raw data");
    /* source provides raw data, we added the pads and we can now signal a
     * no_more pads because we are done. */
    /* FIXME, actually do this... */
    return TRUE;
  }
  if (!have_out && !is_dynamic) {
    GST_DEBUG_OBJECT (decoder, "Source has no output pads");
    /* create a stream to indicate that this uri is handled by a self
     * contained element. We are now done. */
    add_element_stream (decoder->source, decoder);
    return TRUE;
  }
  if (is_dynamic) {
    GST_DEBUG_OBJECT (decoder, "Source has dynamic output pads");
    /* connect a handler for the new-pad signal */
    decoder->src_np_sig_id =
        g_signal_connect (G_OBJECT (decoder->source), "pad-added",
        G_CALLBACK (source_new_pad), decoder);
    decoder->src_nmp_sig_id =
        g_signal_connect (G_OBJECT (decoder->source), "no-more-pads",
        G_CALLBACK (source_no_more_pads), decoder);
    g_object_set_data (G_OBJECT (decoder->source), "pending", "1");
    decoder->pending++;
  } else {
    GstElement *dec_elem;

    GST_DEBUG_OBJECT (decoder, "Pluggin decodebin to source");

    /* no dynamic source, we can link now */
    dec_elem = make_decoder (decoder, decoder->is_stream);
    if (!dec_elem)
      goto no_decoder;

    if (!gst_element_link (decoder->source, dec_elem))
      goto could_not_link;
  }
  return TRUE;

  /* ERRORS */
no_source:
  {
    /* error message was already posted */
    return FALSE;
  }
invalid_source:
  {
    GST_ELEMENT_ERROR (decoder, CORE, FAILED,
        (_("Source element is invalid.")), (NULL));
    return FALSE;
  }
no_decoder:
  {
    /* message was posted */
    return FALSE;
  }
could_not_link:
  {
    GST_ELEMENT_ERROR (decoder, CORE, NEGOTIATION,
        (NULL), ("Can't link source to decoder element"));
    return FALSE;
  }
}

/* generic struct passed to all query fold methods
 * FIXME, move to core.
 */
typedef struct
{
  GstQuery *query;
  gint64 min;
  gint64 max;
  gboolean seekable;
  gboolean live;
} QueryFold;

typedef void (*QueryInitFunction) (GstURIDecodeBin * dec, QueryFold * fold);
typedef void (*QueryDoneFunction) (GstURIDecodeBin * dec, QueryFold * fold);

/* for duration/position we collect all durations/positions and take
 * the MAX of all valid results */
static void
decoder_query_init (GstURIDecodeBin * dec, QueryFold * fold)
{
  fold->min = 0;
  fold->max = -1;
  fold->seekable = TRUE;
  fold->live = 0;
}

static gboolean
decoder_query_duration_fold (GstPad * item, GValue * ret, QueryFold * fold)
{
  if (gst_pad_query (item, fold->query)) {
    gint64 duration;

    g_value_set_boolean (ret, TRUE);

    gst_query_parse_duration (fold->query, NULL, &duration);

    GST_DEBUG_OBJECT (item, "got duration %" G_GINT64_FORMAT, duration);

    if (duration > fold->max)
      fold->max = duration;
  }
  gst_object_unref (item);
  return TRUE;
}
static void
decoder_query_duration_done (GstURIDecodeBin * dec, QueryFold * fold)
{
  GstFormat format;

  gst_query_parse_duration (fold->query, &format, NULL);
  /* store max in query result */
  gst_query_set_duration (fold->query, format, fold->max);

  GST_DEBUG ("max duration %" G_GINT64_FORMAT, fold->max);
}

static gboolean
decoder_query_position_fold (GstPad * item, GValue * ret, QueryFold * fold)
{
  if (gst_pad_query (item, fold->query)) {
    gint64 position;

    g_value_set_boolean (ret, TRUE);

    gst_query_parse_position (fold->query, NULL, &position);

    GST_DEBUG_OBJECT (item, "got position %" G_GINT64_FORMAT, position);

    if (position > fold->max)
      fold->max = position;
  }

  gst_object_unref (item);
  return TRUE;
}
static void
decoder_query_position_done (GstURIDecodeBin * dec, QueryFold * fold)
{
  GstFormat format;

  gst_query_parse_position (fold->query, &format, NULL);
  /* store max in query result */
  gst_query_set_position (fold->query, format, fold->max);

  GST_DEBUG_OBJECT (dec, "max position %" G_GINT64_FORMAT, fold->max);
}

static gboolean
decoder_query_latency_fold (GstPad * item, GValue * ret, QueryFold * fold)
{
  if (gst_pad_query (item, fold->query)) {
    GstClockTime min, max;
    gboolean live;

    g_value_set_boolean (ret, TRUE);

    gst_query_parse_latency (fold->query, &live, &min, &max);

    GST_DEBUG_OBJECT (item,
        "got latency min %" GST_TIME_FORMAT ", max %" GST_TIME_FORMAT
        ", live %d", GST_TIME_ARGS (min), GST_TIME_ARGS (max), live);

    /* for the combined latency we collect the MAX of all min latencies and
     * the MIN of all max latencies */
    if (min > fold->min)
      fold->min = min;
    if (fold->max == -1)
      fold->max = max;
    else if (max < fold->max)
      fold->max = max;
    if (fold->live == FALSE)
      fold->live = live;
  }

  gst_object_unref (item);
  return TRUE;
}
static void
decoder_query_latency_done (GstURIDecodeBin * dec, QueryFold * fold)
{
  /* store max in query result */
  gst_query_set_latency (fold->query, fold->live, fold->min, fold->max);

  GST_DEBUG_OBJECT (dec,
      "latency min %" GST_TIME_FORMAT ", max %" GST_TIME_FORMAT
      ", live %d", GST_TIME_ARGS (fold->min), GST_TIME_ARGS (fold->max),
      fold->live);
}

/* we are seekable if all srcpads are seekable */
static gboolean
decoder_query_seeking_fold (GstPad * item, GValue * ret, QueryFold * fold)
{
  if (gst_pad_query (item, fold->query)) {
    gboolean seekable;

    g_value_set_boolean (ret, TRUE);
    gst_query_parse_seeking (fold->query, NULL, &seekable, NULL, NULL);

    GST_DEBUG_OBJECT (item, "got seekable %d", seekable);

    if (fold->seekable == TRUE)
      fold->seekable = seekable;
  }
  gst_object_unref (item);

  return TRUE;
}
static void
decoder_query_seeking_done (GstURIDecodeBin * dec, QueryFold * fold)
{
  GstFormat format;

  gst_query_parse_seeking (fold->query, &format, NULL, NULL, NULL);
  gst_query_set_seeking (fold->query, format, fold->seekable, 0, -1);

  GST_DEBUG_OBJECT (dec, "seekable %d", fold->seekable);
}

/* generic fold, return first valid result */
static gboolean
decoder_query_generic_fold (GstPad * item, GValue * ret, QueryFold * fold)
{
  gboolean res;

  if ((res = gst_pad_query (item, fold->query))) {
    g_value_set_boolean (ret, TRUE);
    GST_DEBUG_OBJECT (item, "answered query %p", fold->query);
  }

  gst_object_unref (item);

  /* and stop as soon as we have a valid result */
  return !res;
}


/* we're a bin, the default query handler iterates sink elements, which we don't
 * have normally. We should just query all source pads.
 */
static gboolean
gst_uri_decode_bin_query (GstElement * element, GstQuery * query)
{
  GstURIDecodeBin *decoder;
  gboolean res = FALSE;
  GstIterator *iter;
  GstIteratorFoldFunction fold_func;
  QueryInitFunction fold_init = NULL;
  QueryDoneFunction fold_done = NULL;
  QueryFold fold_data;
  GValue ret = { 0 };

  decoder = GST_URI_DECODE_BIN (element);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
      /* iterate and collect durations */
      fold_func = (GstIteratorFoldFunction) decoder_query_duration_fold;
      fold_init = decoder_query_init;
      fold_done = decoder_query_duration_done;
      break;
    case GST_QUERY_POSITION:
      /* iterate and collect durations */
      fold_func = (GstIteratorFoldFunction) decoder_query_position_fold;
      fold_init = decoder_query_init;
      fold_done = decoder_query_position_done;
      break;
    case GST_QUERY_LATENCY:
      /* iterate and collect durations */
      fold_func = (GstIteratorFoldFunction) decoder_query_latency_fold;
      fold_init = decoder_query_init;
      fold_done = decoder_query_latency_done;
      break;
    case GST_QUERY_SEEKING:
      /* iterate and collect durations */
      fold_func = (GstIteratorFoldFunction) decoder_query_seeking_fold;
      fold_init = decoder_query_init;
      fold_done = decoder_query_seeking_done;
      break;
    default:
      fold_func = (GstIteratorFoldFunction) decoder_query_generic_fold;
      break;
  }

  fold_data.query = query;

  g_value_init (&ret, G_TYPE_BOOLEAN);
  g_value_set_boolean (&ret, FALSE);

  iter = gst_element_iterate_src_pads (element);
  GST_DEBUG_OBJECT (element, "Sending query %p (type %d) to src pads",
      query, GST_QUERY_TYPE (query));

  if (fold_init)
    fold_init (decoder, &fold_data);

  while (TRUE) {
    GstIteratorResult ires;

    ires = gst_iterator_fold (iter, fold_func, &ret, &fold_data);

    switch (ires) {
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        if (fold_init)
          fold_init (decoder, &fold_data);
        g_value_set_boolean (&ret, FALSE);
        break;
      case GST_ITERATOR_OK:
      case GST_ITERATOR_DONE:
        res = g_value_get_boolean (&ret);
        if (fold_done != NULL && res)
          fold_done (decoder, &fold_data);
        goto done;
      default:
        res = FALSE;
        goto done;
    }
  }
done:
  gst_iterator_free (iter);

  return res;
}

static GstStateChangeReturn
gst_uri_decode_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstURIDecodeBin *decoder;

  decoder = GST_URI_DECODE_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!setup_source (decoder))
        goto source_failed;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG ("ready to paused");
      if (ret == GST_STATE_CHANGE_FAILURE)
        goto setup_failed;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG ("paused to ready");
      remove_decoders (decoder);
      remove_pads (decoder);
      remove_source (decoder);
      break;
    default:
      break;
  }
  return ret;

  /* ERRORS */
source_failed:
  {
    return GST_STATE_CHANGE_FAILURE;
  }
setup_failed:
  {
    /* clean up leftover groups */
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_uri_decode_bin_debug, "uridecodebin", 0,
      "URI decoder element");

#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif /* ENABLE_NLS */

  return gst_element_register (plugin, "uridecodebin", GST_RANK_NONE,
      GST_TYPE_URI_DECODE_BIN);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "uridecodebin",
    "URI Decoder bin", plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
