/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *               <2006> Stefan Kost <ensonic@users.sf.net>
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
 * SECTION:element-spectrum
 *
 * <refsect2>
 * The Spectrum element analyzes the frequency spectrum of an audio signal.
 * If <link linkend="GstSpectrum--message">message property</link> is #TRUE it
 * sends analysis results as application message named
 * <classname>&quot;spectrum&quot;</classname> after each interval of time given
 * by the <link linkend="GstSpectrum--interval">interval property</link>.
 * The message's structure contains two fields:
 * <itemizedlist>
 * <listitem>
 *   <para>
 *   #GstClockTime
 *   <classname>&quot;endtime&quot;</classname>:
 *   the end time of the buffer that triggered the message
 *   </para>
 * </listitem>
 * <listitem>
 *   <para>
 *   #GstValueList of #guchar
 *   <classname>&quot;spectrum&quot;</classname>:
 *   the level for each frequency band. A value of 0 maps to the
 *   db value given by the
 *   <link linkend="GstSpectrum--threshold">threshold property.</link>.
 *   </para>
 * </listitem>
 *
 * This element cannot be used with the gst-launch command in a sensible way.
 * The included demo shows how to use it in an application.
 *
 * Last reviewed on 2006-05-21 (0.10.3)
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include <gst/audio/audio.h>
#include "gstspectrum.h"

GST_DEBUG_CATEGORY_STATIC (gst_spectrum_debug);
#define GST_CAT_DEFAULT gst_spectrum_debug

/* elementfactory information */
static const GstElementDetails gst_spectrum_details =
GST_ELEMENT_DETAILS ("Spectrum analyzer",
    "Filter/Analyzer/Audio",
    "Run an FFT on the audio signal, output spectrum data",
    "Erik Walthinsen <omega@cse.ogi.edu>, "
    "Stefan Kost <ensonic@users.sf.net>");

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [1, MAX], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [1, MAX], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

/*
static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);
*/

/* Spectrum properties */
enum
{
  PROP_0,
  PROP_SIGNAL_SPECTRUM,
  PROP_SIGNAL_INTERVAL,
  PROP_BANDS,
  PROP_THRESHOLD
};

GST_BOILERPLATE (GstSpectrum, gst_spectrum, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM);

static void gst_spectrum_dispose (GObject * object);
static void gst_spectrum_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_spectrum_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/*
static gboolean gst_spectrum_set_sink_caps (GstPad * pad, GstCaps * caps);
static GstCaps *gst_spectrum_get_sink_caps (GstPad * pad);
static GstFlowReturn gst_spectrum_chain (GstPad * pad, GstBuffer * buffer);
static GstStateChangeReturn gst_spectrum_change_state (GstElement * element,
    GstStateChange transition);
*/
static gboolean gst_spectrum_set_caps (GstBaseTransform * trans, GstCaps * in,
    GstCaps * out);
static gboolean gst_spectrum_start (GstBaseTransform * trans);
static GstFlowReturn gst_spectrum_transform_ip (GstBaseTransform * trans,
    GstBuffer * in);


#define fixed short
extern int gst_spectrum_fix_fft (fixed fr[], fixed fi[], int m, int inverse);
extern void gst_spectrum_fix_loud (fixed loud[], fixed fr[], fixed fi[], int n,
    int scale_shift);
extern void gst_spectrum_window (fixed fr[], int n);

static void
gst_spectrum_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_set_details (element_class, &gst_spectrum_details);
}

static void
gst_spectrum_class_init (GstSpectrumClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_spectrum_set_property;
  gobject_class->get_property = gst_spectrum_get_property;
  gobject_class->dispose = gst_spectrum_dispose;

  /*element->change_state = gst_spectrum_change_state; */
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_spectrum_set_caps);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_spectrum_start);
  trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_spectrum_transform_ip);
  trans_class->passthrough_on_same_caps = TRUE;

  g_object_class_install_property (gobject_class, PROP_SIGNAL_SPECTRUM,
      g_param_spec_boolean ("message", "Message",
          "Post a level message for each passed interval",
          TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SIGNAL_INTERVAL,
      g_param_spec_uint64 ("interval", "Interval",
          "Interval of time between message posts (in nanoseconds)",
          1, G_MAXUINT64, GST_SECOND / 10, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_BANDS,
      g_param_spec_uint ("bands", "Bands", "number of frequency bands",
          0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_int ("threshold", "Threshold",
          "db threshold for result, maps to 0", G_MININT, 0, -60,
          G_PARAM_READWRITE));

  GST_DEBUG_CATEGORY_INIT (gst_spectrum_debug, "spectrum", 0,
      "audio spectrum analyser element");
}

static void
gst_spectrum_init (GstSpectrum * spectrum, GstSpectrumClass * g_class)
{
  /*
     spectrum->sinkpad =
     gst_pad_new_from_static_template (&sink_template_factory, "sink");
     gst_pad_set_chain_function (spectrum->sinkpad, gst_spectrum_chain);
     gst_pad_set_setcaps_function (spectrum->sinkpad, gst_spectrum_set_sink_caps);
     gst_pad_set_getcaps_function (spectrum->sinkpad, gst_spectrum_get_sink_caps);
     gst_element_add_pad (GST_ELEMENT (spectrum), spectrum->sinkpad);

     spectrum->srcpad =
     gst_pad_new_from_static_template (&src_template_factory, "src");
     gst_element_add_pad (GST_ELEMENT (spectrum), spectrum->srcpad);
   */

  spectrum->adapter = gst_adapter_new ();

  spectrum->interval = GST_SECOND / 10;
  spectrum->bands = 128;
  spectrum->base = 9;
  spectrum->len = 1024;         /* 2 ^ (base+1) */

  spectrum->loud = g_malloc (spectrum->len * sizeof (gint16));
  spectrum->im = g_malloc0 (spectrum->len * sizeof (gint16));
  spectrum->re = g_malloc0 (spectrum->len * sizeof (gint16));
  spectrum->spect = g_malloc (spectrum->bands * sizeof (guchar));
}

static void
gst_spectrum_dispose (GObject * object)
{
  GstSpectrum *spectrum = GST_SPECTRUM (object);

  if (spectrum->adapter) {
    g_object_unref (spectrum->adapter);
    spectrum->adapter = NULL;
  }

  g_free (spectrum->re);
  g_free (spectrum->im);
  g_free (spectrum->loud);
  g_free (spectrum->spect);

  spectrum->re = NULL;
  spectrum->im = NULL;
  spectrum->loud = NULL;
  spectrum->spect = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_spectrum_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpectrum *filter = GST_SPECTRUM (object);

  switch (prop_id) {
    case PROP_SIGNAL_SPECTRUM:
      filter->message = g_value_get_boolean (value);
      break;
    case PROP_SIGNAL_INTERVAL:
      filter->interval = g_value_get_uint64 (value);
      break;
    case PROP_BANDS:
      filter->bands = g_value_get_uint (value);
      g_free (filter->spect);
      filter->spect = g_malloc (filter->bands * sizeof (guchar));
      GST_DEBUG_OBJECT (filter, "reallocation, spect = %p, bands =%d ",
          filter->spect, filter->bands);
      break;
    case PROP_THRESHOLD:
      filter->threshold = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_spectrum_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSpectrum *filter = GST_SPECTRUM (object);

  switch (prop_id) {
    case PROP_SIGNAL_SPECTRUM:
      g_value_set_boolean (value, filter->message);
      break;
    case PROP_SIGNAL_INTERVAL:
      g_value_set_uint64 (value, filter->interval);
      break;
    case PROP_BANDS:
      g_value_set_uint (value, filter->bands);
      break;
    case PROP_THRESHOLD:
      g_value_set_int (value, filter->threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
static gboolean
gst_spectrum_set_sink_caps (GstPad * pad, GstCaps * caps)
{
  GstSpectrum *spectrum = GST_SPECTRUM (gst_pad_get_parent (pad));
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gboolean ret;

  ret = gst_structure_get_int (structure, "channels", &spectrum->channels);
  GST_INFO ("Got channels = %d", spectrum->channels);

  gst_object_unref (spectrum);

  return ret;
}

static GstCaps *
gst_spectrum_get_sink_caps (GstPad * pad)
{
  GstSpectrum *spectrum = GST_SPECTRUM (gst_pad_get_parent (pad));
  GstCaps *res;
  GstStructure *structure;

  res = gst_caps_copy (gst_pad_get_pad_template_caps (spectrum->sinkpad));
  if (spectrum->channels) {
    structure = gst_caps_get_structure (res, 0);
    gst_structure_set (structure, "channels", G_TYPE_INT, spectrum->channels,
        NULL);
    GST_INFO ("Fixate channels = %d", spectrum->channels);
  }
  gst_object_unref (spectrum);
  return res;
}
*/

static gboolean
gst_spectrum_set_caps (GstBaseTransform * trans, GstCaps * in, GstCaps * out)
{
  GstSpectrum *filter = GST_SPECTRUM (trans);
  GstStructure *structure;

  structure = gst_caps_get_structure (in, 0);
  gst_structure_get_int (structure, "rate", &filter->rate);
  gst_structure_get_int (structure, "channels", &filter->channels);

  return TRUE;
}

static gboolean
gst_spectrum_start (GstBaseTransform * trans)
{
  GstSpectrum *filter = GST_SPECTRUM (trans);

  gst_adapter_clear (filter->adapter);
  filter->num_frames = 0;

  return TRUE;
}

static GstMessage *
gst_spectrum_message_new (GstSpectrum * spectrum, GstClockTime endtime)
{
  GstStructure *s;
  GValue v = { 0, };
  GValue *l;
  guint i;
  guchar *spect = spectrum->spect;

  GST_DEBUG_OBJECT (spectrum, "preparing message, spect = %p, bands =%d ",
      spect, spectrum->bands);

  s = gst_structure_new ("spectrum", "endtime", GST_TYPE_CLOCK_TIME,
      endtime, NULL);

  g_value_init (&v, GST_TYPE_LIST);
  /* will copy-by-value */
  gst_structure_set_value (s, "spectrum", &v);
  g_value_unset (&v);

  g_value_init (&v, G_TYPE_UCHAR);
  l = (GValue *) gst_structure_get_value (s, "spectrum");
  for (i = 0; i < spectrum->bands; i++) {
    g_value_set_uchar (&v, spect[i]);
    gst_value_list_append_value (l, &v);        /* copies by value */
  }
  g_value_unset (&v);

  return gst_message_new_element (GST_OBJECT (spectrum), s);
}

static GstFlowReturn
gst_spectrum_transform_ip (GstBaseTransform * trans, GstBuffer * in)
{
  GstSpectrum *spectrum = GST_SPECTRUM (trans);
  gint16 *samples;
  gint wanted;
  gint i, j, k;
  gint32 acc;
  gfloat pos, step;
  guchar *spect = spectrum->spect;
  GstClockTime endtime = GST_BUFFER_TIMESTAMP (in);
  GstClockTime blktime =
      GST_FRAMES_TO_CLOCK_TIME (spectrum->len, spectrum->rate);

  GST_LOG ("transform : %ld bytes", GST_BUFFER_SIZE (in));

  gst_adapter_push (spectrum->adapter, gst_buffer_ref (in));
  /* required number of bytes */
  wanted = spectrum->channels * spectrum->len * sizeof (gint16);
  /* FIXME: 4.0 was 2.0 before, but that include the mirrored spectrum */
  step = (gfloat) spectrum->len / (spectrum->bands * 4.0);

  while (gst_adapter_available (spectrum->adapter) > wanted) {

    GST_LOG ("  adapter loop");
    samples = (gint16 *) gst_adapter_take (spectrum->adapter, wanted);

    for (i = 0, j = 0; i < spectrum->len; i++) {
      for (k = 0, acc = 0; k < spectrum->channels; k++)
        acc += samples[j++];
      spectrum->re[i] = (gint16) (acc / spectrum->channels);
    }

    GST_LOG ("  fft");

    gst_spectrum_window (spectrum->re, spectrum->len);
    gst_spectrum_fix_fft (spectrum->re, spectrum->im, spectrum->base, FALSE);
    gst_spectrum_fix_loud (spectrum->loud, spectrum->re, spectrum->im,
        spectrum->len, 0);

    GST_LOG ("  resampling");

    /* resample to requested number of bands */
    for (i = 0, pos = 0.0; i < spectrum->bands; i++, pos += step) {
      if (spectrum->loud[(gint) pos] > spectrum->threshold) {
        spect[i] = spectrum->loud[(gint) pos] - spectrum->threshold;
        /*
           if (spect[i] > 15);
           spect[i] = 15;
         */
      } else
        /* treat as silence */
        spect[i] = 0;
    }

    GST_LOG ("  send message?");
    //ret = gst_pad_push (spectrum->srcpad, outbuf);
    spectrum->num_frames += spectrum->len;
    endtime += blktime;
    /* do we need to message ? */
    if (spectrum->num_frames >=
        GST_CLOCK_TIME_TO_FRAMES (spectrum->interval, spectrum->rate)) {
      if (spectrum->message) {
        GstMessage *m = gst_spectrum_message_new (spectrum, endtime);

        GST_LOG ("  sending message");
        gst_element_post_message (GST_ELEMENT (spectrum), m);
      }
      spectrum->num_frames = 0;
    }
  }

  return GST_FLOW_OK;
}

/*
static GstStateChangeReturn
gst_spectrum_change_state (GstElement * element, GstStateChange transition)
{
  GstSpectrum *spectrum = GST_SPECTRUM (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_adapter_clear (spectrum->adapter);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}
*/

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "spectrum", GST_RANK_NONE,
      GST_TYPE_SPECTRUM);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "spectrum",
    "Run an FFT on the audio signal, output spectrum data",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
