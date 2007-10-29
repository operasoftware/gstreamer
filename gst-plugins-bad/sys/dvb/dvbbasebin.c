/*
 * dvbbasebin.c - 
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include "dvbbasebin.h"

GST_DEBUG_CATEGORY_STATIC (dvb_base_bin_debug);
#define GST_CAT_DEFAULT dvb_base_bin_debug

static GstElementDetails dvb_base_bin_details = GST_ELEMENT_DETAILS ("DVB bin",
    "Source/Bin/Video",
    "Access descramble and split DVB streams",
    "Alessandro Decina <alessandro@nnva.org>");

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src%d", GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

static GstStaticPadTemplate program_template =
GST_STATIC_PAD_TEMPLATE ("program_%d", GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  PROP_ADAPTER,
  PROP_FRONTEND,
  PROP_DISEQC_SRC,
  PROP_FREQUENCY,
  PROP_POLARITY,
  PROP_SYMBOL_RATE,
  PROP_BANDWIDTH,
  PROP_CODE_RATE_HP,
  PROP_CODE_RATE_LP,
  PROP_GUARD,
  PROP_MODULATION,
  PROP_TRANS_MODE,
  PROP_HIERARCHY,
  PROP_INVERSION,
  PROP_PROGRAM_NUMBERS,
  PROP_STATS_REPORTING_INTERVAL
      /* FILL ME */
};

typedef struct
{
  guint16 pid;
  guint usecount;
} DvbBaseBinStream;

typedef struct
{
  gint program_number;
  guint16 pmt_pid;
  guint16 pcr_pid;
  GObject *pmt;
  GObject *old_pmt;
  gboolean selected;
  gboolean pmt_active;
  gboolean active;
  GstPad *ghost;
} DvbBaseBinProgram;

static void dvb_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void dvb_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void dvb_base_bin_dispose (GObject * object);
static void dvb_base_bin_finalize (GObject * object);

static gboolean dvb_base_bin_ts_pad_probe_cb (GstPad * pad,
    GstBuffer * buf, gpointer user_data);
static GstStateChangeReturn dvb_base_bin_change_state (GstElement * element,
    GstStateChange transition);
static void dvb_base_bin_pat_info_changed_cb (GstElement * mpegtsparse,
    GParamSpec * pspec, DvbBaseBin * dvbbasebin);
static void dvb_base_bin_pmt_info_cb (GstElement * mpegtsparse,
    guint program_number, GObject * pmt, DvbBaseBin * dvbbasebin);
static void dvb_base_bin_pad_added_cb (GstElement * mpegtsparse,
    GstPad * pad, DvbBaseBin * dvbbasebin);
static void dvb_base_bin_pad_removed_cb (GstElement * mpegtsparse,
    GstPad * pad, DvbBaseBin * dvbbasebin);
static GstPad *dvb_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name);
static void dvb_base_bin_release_pad (GstElement * element, GstPad * pad);

static DvbBaseBinStream *
dvb_base_bin_add_stream (DvbBaseBin * dvbbasebin, guint16 pid)
{
  DvbBaseBinStream *stream;

  stream = g_new0 (DvbBaseBinStream, 1);
  stream->pid = pid;
  stream->usecount = 0;

  g_hash_table_insert (dvbbasebin->streams,
      GINT_TO_POINTER ((gint) pid), stream);

  return stream;
}

static DvbBaseBinStream *
dvb_base_bin_get_stream (DvbBaseBin * dvbbasebin, guint16 pid)
{
  return (DvbBaseBinStream *) g_hash_table_lookup (dvbbasebin->streams,
      GINT_TO_POINTER ((gint) pid));
}

static DvbBaseBinProgram *
dvb_base_bin_add_program (DvbBaseBin * dvbbasebin, gint program_number)
{
  DvbBaseBinProgram *program;

  program = g_new0 (DvbBaseBinProgram, 1);
  program->program_number = program_number;
  program->selected = FALSE;
  program->active = FALSE;
  program->pmt_pid = G_MAXUINT16;
  program->pcr_pid = G_MAXUINT16;
  program->pmt = NULL;
  program->old_pmt = NULL;

  g_hash_table_insert (dvbbasebin->programs,
      GINT_TO_POINTER (program_number), program);

  return program;
}

static DvbBaseBinProgram *
dvb_base_bin_get_program (DvbBaseBin * dvbbasebin, gint program_number)
{
  return (DvbBaseBinProgram *) g_hash_table_lookup (dvbbasebin->programs,
      GINT_TO_POINTER (program_number));
}

/*
static guint signals [LAST_SIGNAL] = { 0 };
*/

GST_BOILERPLATE (DvbBaseBin, dvb_base_bin, GstBin, GST_TYPE_BIN);

static void
dvb_base_bin_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  element_class->request_new_pad = dvb_base_bin_request_new_pad;
  element_class->release_pad = dvb_base_bin_release_pad;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&program_template));

  gst_element_class_set_details (element_class, &dvb_base_bin_details);
}

static void
dvb_base_bin_class_init (DvbBaseBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstElementFactory *dvbsrc_factory;
  GObjectClass *dvbsrc_class;
  typedef struct
  {
    guint prop_id;
    const gchar *prop_name;
  } ProxyedProperty;
  ProxyedProperty *walk;
  ProxyedProperty proxyed_properties[] = {
    {PROP_ADAPTER, "adapter"},
    {PROP_FRONTEND, "frontend"},
    {PROP_DISEQC_SRC, "diseqc-source"},
    {PROP_FREQUENCY, "frequency"},
    {PROP_POLARITY, "polarity"},
    {PROP_SYMBOL_RATE, "symbol-rate"},
    {PROP_BANDWIDTH, "bandwidth"},
    {PROP_CODE_RATE_HP, "code-rate-hp"},
    {PROP_CODE_RATE_LP, "code-rate-lp"},
    {PROP_GUARD, "guard"},
    {PROP_MODULATION, "modulation"},
    {PROP_TRANS_MODE, "trans-mode"},
    {PROP_HIERARCHY, "hierarchy"},
    {PROP_INVERSION, "inversion"},
    {PROP_STATS_REPORTING_INTERVAL, "stats-reporting-interval"},
    {0, NULL}
  };

  element_class = GST_ELEMENT_CLASS (klass);
  element_class->change_state = dvb_base_bin_change_state;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = dvb_base_bin_set_property;
  gobject_class->get_property = dvb_base_bin_get_property;
  gobject_class->dispose = dvb_base_bin_dispose;
  gobject_class->finalize = dvb_base_bin_finalize;

  /* install dvbsrc properties */
  dvbsrc_factory = gst_element_factory_find ("dvbsrc");
  dvbsrc_class = g_type_class_ref (dvbsrc_factory->type);
  walk = proxyed_properties;
  while (walk->prop_name != NULL) {
    GParamSpec *pspec;
    GParamSpec *our_pspec;

    pspec = g_object_class_find_property (dvbsrc_class, walk->prop_name);
    if (pspec != NULL) {
      GType param_type = G_PARAM_SPEC_TYPE (pspec);

      if (param_type == G_TYPE_PARAM_INT) {
        GParamSpecInt *src_pspec = G_PARAM_SPEC_INT (pspec);

        our_pspec = g_param_spec_int (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->minimum, src_pspec->maximum, src_pspec->default_value,
            pspec->flags);
      } else if (param_type == G_TYPE_PARAM_UINT) {
        GParamSpecUInt *src_pspec = G_PARAM_SPEC_UINT (pspec);

        our_pspec = g_param_spec_uint (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->minimum, src_pspec->maximum, src_pspec->default_value,
            pspec->flags);
      } else if (param_type == G_TYPE_PARAM_STRING) {
        GParamSpecString *src_pspec = G_PARAM_SPEC_STRING (pspec);

        our_pspec = g_param_spec_string (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->default_value, pspec->flags);
      } else if (param_type == G_TYPE_PARAM_ENUM) {
        GParamSpecEnum *src_pspec = G_PARAM_SPEC_ENUM (pspec);

        our_pspec = g_param_spec_enum (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            pspec->value_type, src_pspec->default_value, pspec->flags);
      } else {
        g_assert_not_reached ();
      }

      g_object_class_install_property (gobject_class, walk->prop_id, our_pspec);
    } else {
      g_warning ("dvbsrc has no property named %s", walk->prop_name);
    }
    ++walk;
  }
  g_type_class_unref (dvbsrc_class);

  g_object_class_install_property (gobject_class, PROP_PROGRAM_NUMBERS,
      g_param_spec_string ("program-numbers",
          "Program Numbers",
          "Colon separated list of programs", "", G_PARAM_READWRITE));
}

static void
dvb_base_bin_reset (DvbBaseBin * dvbbasebin)
{
  if (dvbbasebin->hwcam) {
    cam_device_close (dvbbasebin->hwcam);
    cam_device_free (dvbbasebin->hwcam);
    dvbbasebin->hwcam = NULL;
  }

  if (dvbbasebin->ts_pad) {
    gst_element_release_request_pad (GST_ELEMENT (dvbbasebin->mpegtsparse),
        dvbbasebin->ts_pad);
    dvbbasebin->ts_pad = NULL;
  }
}

static void
dvb_base_bin_init (DvbBaseBin * dvbbasebin, DvbBaseBinClass * klass)
{
  dvbbasebin->dvbsrc = gst_element_factory_make ("dvbsrc", NULL);
  dvbbasebin->buffer_queue = gst_element_factory_make ("queue", NULL);
  dvbbasebin->mpegtsparse = gst_element_factory_make ("mpegtsparse", NULL);
  g_object_connect (dvbbasebin->mpegtsparse,
      "signal::notify::pat-info", dvb_base_bin_pat_info_changed_cb, dvbbasebin,
      "signal::pmt-info", dvb_base_bin_pmt_info_cb, dvbbasebin,
      "signal::pad-added", dvb_base_bin_pad_added_cb, dvbbasebin,
      "signal::pad-removed", dvb_base_bin_pad_removed_cb, dvbbasebin, NULL);

  gst_bin_add_many (GST_BIN (dvbbasebin), dvbbasebin->dvbsrc,
      dvbbasebin->buffer_queue, dvbbasebin->mpegtsparse, NULL);

  gst_element_link_many (dvbbasebin->dvbsrc,
      dvbbasebin->buffer_queue, dvbbasebin->mpegtsparse, NULL);

  dvbbasebin->programs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, g_free);
  dvbbasebin->streams = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, g_free);

  dvbbasebin->pmtlist = NULL;
  dvbbasebin->pmtlist_changed = FALSE;

  dvbbasebin->disposed = FALSE;
  dvb_base_bin_reset (dvbbasebin);
}

static void
dvb_base_bin_dispose (GObject * object)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  if (!dvbbasebin->disposed) {
    /* remove mpegtsparse BEFORE dvbsrc, since the mpegtsparse::pad-removed
     * signal handler uses dvbsrc */
    dvb_base_bin_reset (dvbbasebin);
    gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->mpegtsparse);
    gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->dvbsrc);
    gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->buffer_queue);
    dvbbasebin->disposed = TRUE;
  }

  if (G_OBJECT_CLASS (parent_class)->dispose)
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
dvb_base_bin_finalize (GObject * object)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  g_hash_table_destroy (dvbbasebin->streams);
  g_hash_table_destroy (dvbbasebin->programs);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dvb_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  switch (prop_id) {
    case PROP_ADAPTER:
    case PROP_DISEQC_SRC:
    case PROP_FRONTEND:
    case PROP_FREQUENCY:
    case PROP_POLARITY:
    case PROP_SYMBOL_RATE:
    case PROP_BANDWIDTH:
    case PROP_CODE_RATE_HP:
    case PROP_CODE_RATE_LP:
    case PROP_GUARD:
    case PROP_MODULATION:
    case PROP_TRANS_MODE:
    case PROP_HIERARCHY:
    case PROP_INVERSION:
    case PROP_STATS_REPORTING_INTERVAL:
      /* FIXME: check if we can tune (state < PLAYING || program-numbers == "") */
      g_object_set_property (G_OBJECT (dvbbasebin->dvbsrc), pspec->name, value);
      break;
    case PROP_PROGRAM_NUMBERS:
      g_object_set_property (G_OBJECT (dvbbasebin->mpegtsparse), pspec->name,
          value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
dvb_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  switch (prop_id) {
    case PROP_ADAPTER:
    case PROP_FRONTEND:
    case PROP_DISEQC_SRC:
    case PROP_FREQUENCY:
    case PROP_POLARITY:
    case PROP_SYMBOL_RATE:
    case PROP_BANDWIDTH:
    case PROP_CODE_RATE_HP:
    case PROP_CODE_RATE_LP:
    case PROP_GUARD:
    case PROP_MODULATION:
    case PROP_TRANS_MODE:
    case PROP_HIERARCHY:
    case PROP_INVERSION:
    case PROP_STATS_REPORTING_INTERVAL:
      g_object_get_property (G_OBJECT (dvbbasebin->dvbsrc), pspec->name, value);
      break;
    case PROP_PROGRAM_NUMBERS:
      g_object_get_property (G_OBJECT (dvbbasebin->mpegtsparse), pspec->name,
          value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GstPad *
dvb_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name)
{
  GstPad *pad;
  GstPad *ghost;
  gchar *pad_name;

  if (name == NULL)
    name = GST_PAD_TEMPLATE_NAME_TEMPLATE (templ);

  pad =
      gst_element_get_request_pad (GST_DVB_BASE_BIN (element)->mpegtsparse,
      name);
  if (pad == NULL)
    return NULL;

  pad_name = gst_pad_get_name (pad);
  ghost = gst_ghost_pad_new (pad_name, pad);
  g_free (pad_name);
  gst_element_add_pad (element, ghost);

  return ghost;
}

static void
dvb_base_bin_release_pad (GstElement * element, GstPad * pad)
{
  GstGhostPad *ghost;
  GstPad *target;

  g_return_if_fail (GST_IS_DVB_BASE_BIN (element));

  ghost = GST_GHOST_PAD (pad);
  target = gst_ghost_pad_get_target (ghost);
  gst_element_release_request_pad (GST_ELEMENT (GST_DVB_BASE_BIN (element)->
          mpegtsparse), target);
  gst_object_unref (target);

  gst_element_remove_pad (element, pad);
}

static void
dvb_base_bin_reset_pmtlist (DvbBaseBin * dvbbasebin)
{
  CamConditionalAccessPmtFlag flag;
  GList *walk;
  GObject *pmt;

  walk = dvbbasebin->pmtlist;
  while (walk) {
    if (walk->prev == NULL) {
      if (walk->next == NULL)
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_ONLY;
      else
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_FIRST;
    } else {
      if (walk->next == NULL)
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_LAST;
      else
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_MORE;
    }

    pmt = G_OBJECT (walk->data);
    cam_device_set_pmt (dvbbasebin->hwcam, pmt, flag);

    walk = walk->next;
  }

  dvbbasebin->pmtlist_changed = FALSE;
}

static gboolean
dvb_base_bin_ts_pad_probe_cb (GstPad * pad, GstBuffer * buf, gpointer user_data)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (user_data);

  if (dvbbasebin->hwcam) {
    cam_device_poll (dvbbasebin->hwcam);

    if (dvbbasebin->pmtlist_changed) {
      if (cam_device_ready (dvbbasebin->hwcam)) {
        GST_DEBUG_OBJECT (dvbbasebin, "pmt list changed");
        dvb_base_bin_reset_pmtlist (dvbbasebin);
      } else {
        GST_DEBUG_OBJECT (dvbbasebin, "pmt list changed but CAM not ready");
      }
    }
  }

  return TRUE;
}

static void
dvb_base_bin_init_cam (DvbBaseBin * dvbbasebin)
{
  gint adapter;
  gchar *ca_file;

  g_object_get (dvbbasebin->dvbsrc, "adapter", &adapter, NULL);
  /* TODO: handle multiple cams */
  ca_file = g_strdup_printf ("/dev/dvb/adapter%d/ca0", adapter);
  if (g_file_test (ca_file, G_FILE_TEST_EXISTS)) {
    dvbbasebin->hwcam = cam_device_new ();
    if (cam_device_open (dvbbasebin->hwcam, ca_file)) {
      /* HACK: poll the cam in a buffer probe */
      dvbbasebin->ts_pad =
          gst_element_get_request_pad (dvbbasebin->mpegtsparse, "src%d");
      gst_pad_add_buffer_probe (dvbbasebin->ts_pad,
          G_CALLBACK (dvb_base_bin_ts_pad_probe_cb), dvbbasebin);
    } else {
      GST_ERROR_OBJECT (dvbbasebin, "could not open %s", ca_file);
      cam_device_free (dvbbasebin->hwcam);
      dvbbasebin->hwcam = NULL;
    }
  }

  g_free (ca_file);
}

static GstStateChangeReturn
dvb_base_bin_change_state (GstElement * element, GstStateChange transition)
{
  DvbBaseBin *dvbbasebin;
  GstStateChangeReturn ret;

  dvbbasebin = GST_DVB_BASE_BIN (element);
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      dvb_base_bin_init_cam (dvbbasebin);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      dvb_base_bin_reset (dvbbasebin);
      break;
    default:
      break;
  }

  return ret;
}

static void
foreach_stream_build_filter (gpointer key, gpointer value, gpointer user_data)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (user_data);
  DvbBaseBinStream *stream = (DvbBaseBinStream *) value;
  gchar *tmp, *pid;

  g_assert (stream->usecount >= 0);

  GST_DEBUG ("stream %d usecount %d", stream->pid, stream->usecount);

  if (stream->usecount > 0) {
    /* TODO: use g_strjoinv FTW */
    tmp = dvbbasebin->filter;
    pid = g_strdup_printf ("%d", stream->pid);
    dvbbasebin->filter = g_strjoin (":", pid, dvbbasebin->filter, NULL);

    g_free (pid);
    g_free (tmp);
  }
}

static void
dvb_base_bin_rebuild_filter (DvbBaseBin * dvbbasebin)
{
  g_hash_table_foreach (dvbbasebin->streams,
      foreach_stream_build_filter, dvbbasebin);

  if (dvbbasebin->filter == NULL)
    /* fix dvbsrc to handle NULL filter */
    dvbbasebin->filter = g_strdup ("");

  GST_INFO_OBJECT (dvbbasebin, "rebuilt filter %s", dvbbasebin->filter);

  /* FIXME: find a way to not add unwanted pids controlled by app */
  g_object_set (dvbbasebin->dvbsrc, "pids", dvbbasebin->filter, NULL);
  g_free (dvbbasebin->filter);
  dvbbasebin->filter = NULL;
}

static void
dvb_base_bin_remove_pmt_streams (DvbBaseBin * dvbbasebin, GObject * pmt)
{
  GValueArray *streams;
  gint program_number;
  gint i;
  GValue *value;
  GObject *streamobj;
  DvbBaseBinStream *stream;
  guint pid;
  guint stream_type;

  g_object_get (pmt, "program-number", &program_number,
      "stream-info", &streams, NULL);

  for (i = 0; i < streams->n_values; ++i) {
    value = g_value_array_get_nth (streams, i);
    streamobj = g_value_get_object (value);

    g_object_get (streamobj, "pid", &pid, "stream-type", &stream_type, NULL);

    stream = dvb_base_bin_get_stream (dvbbasebin, (guint16) pid);
    if (stream == NULL) {
      GST_WARNING_OBJECT (dvbbasebin, "removing unknown stream %d ??", pid);
      continue;
    }

    --stream->usecount;
  }
}

static void
dvb_base_bin_add_pmt_streams (DvbBaseBin * dvbbasebin, GObject * pmt)
{
  DvbBaseBinStream *stream;
  GValueArray *streams;
  gint program_number;
  gint i;
  GValue *value;
  GObject *streamobj;
  guint pid;
  guint stream_type;

  g_object_get (pmt, "program-number", &program_number,
      "stream-info", &streams, NULL);

  for (i = 0; i < streams->n_values; ++i) {
    value = g_value_array_get_nth (streams, i);
    streamobj = g_value_get_object (value);

    g_object_get (streamobj, "pid", &pid, "stream-type", &stream_type, NULL);
    GST_DEBUG ("filtering stream %d stream_type %d", pid, stream_type);

    stream = dvb_base_bin_get_stream (dvbbasebin, (guint16) pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, (guint16) pid);

    ++stream->usecount;
  }
}

static void
dvb_base_bin_activate_program (DvbBaseBin * dvbbasebin,
    DvbBaseBinProgram * program)
{
  DvbBaseBinStream *stream;
  guint pid;

  if (program->old_pmt) {
    dvb_base_bin_remove_pmt_streams (dvbbasebin, program->old_pmt);
    dvbbasebin->pmtlist = g_list_remove (dvbbasebin->pmtlist, program->old_pmt);
  }

  /* activate the PMT and PCR streams. If the PCR stream is in the PMT its
   * usecount will be incremented by 2 here and decremented by 2 when the
   * program is deactivated */
  if (!program->pmt_active) {
    stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, program->pmt_pid);
    stream->usecount += 1;
    program->pmt_active = TRUE;
  }

  if (program->pmt) {
    guint16 old_pcr_pid;

    old_pcr_pid = program->pcr_pid;
    g_object_get (program->pmt, "pcr-pid", &pid, NULL);
    program->pcr_pid = pid;
    if (old_pcr_pid != G_MAXUINT16 && old_pcr_pid != program->pcr_pid)
      dvb_base_bin_get_stream (dvbbasebin, old_pcr_pid)->usecount--;

    stream = dvb_base_bin_get_stream (dvbbasebin, program->pcr_pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, program->pcr_pid);
    stream->usecount += 1;

    dvb_base_bin_add_pmt_streams (dvbbasebin, program->pmt);
    dvbbasebin->pmtlist = g_list_append (dvbbasebin->pmtlist, program->pmt);
    dvbbasebin->pmtlist_changed = TRUE;
    program->active = TRUE;
  }

  dvb_base_bin_rebuild_filter (dvbbasebin);
}

static void
dvb_base_bin_deactivate_program (DvbBaseBin * dvbbasebin,
    DvbBaseBinProgram * program)
{
  DvbBaseBinStream *stream;

  stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
  if (stream != NULL)
    stream->usecount -= 1;

  stream = dvb_base_bin_get_stream (dvbbasebin, program->pcr_pid);
  if (stream != NULL)
    stream->usecount -= 1;

  if (program->pmt) {
    dvb_base_bin_remove_pmt_streams (dvbbasebin, program->pmt);
    dvbbasebin->pmtlist = g_list_remove (dvbbasebin->pmtlist, program->pmt);
    dvbbasebin->pmtlist_changed = TRUE;
  }

  dvb_base_bin_rebuild_filter (dvbbasebin);
  program->pmt_active = FALSE;
  program->active = FALSE;
}

static void
dvb_base_bin_pat_info_changed_cb (GstElement * mpegtsparse,
    GParamSpec * pspec, DvbBaseBin * dvbbasebin)
{
  DvbBaseBinProgram *program;
  DvbBaseBinStream *stream;
  GValueArray *pat_info;
  GValue *value;
  GObject *program_info;
  gint program_number;
  guint pid;
  guint16 old_pmt_pid;
  gint i;
  gboolean rebuild_filter = FALSE;

  g_object_get (mpegtsparse, "pat-info", &pat_info, NULL);

  for (i = 0; i < pat_info->n_values; ++i) {
    value = g_value_array_get_nth (pat_info, i);
    program_info = g_value_get_object (value);

    g_object_get (program_info, "program-number", &program_number,
        "pid", &pid, NULL);

    program = dvb_base_bin_get_program (dvbbasebin, program_number);
    if (program == NULL)
      program = dvb_base_bin_add_program (dvbbasebin, program_number);

    old_pmt_pid = program->pmt_pid;
    program->pmt_pid = pid;

    if (program->selected) {
      /* PAT update */
      if (old_pmt_pid != G_MAXUINT16 && old_pmt_pid != program->pmt_pid)
        dvb_base_bin_get_stream (dvbbasebin, old_pmt_pid)->usecount -= 1;

      stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
      if (stream == NULL)
        stream = dvb_base_bin_add_stream (dvbbasebin, program->pmt_pid);

      stream->usecount += 1;

      rebuild_filter = TRUE;
    }
  }

  g_value_array_free (pat_info);

  if (rebuild_filter)
    dvb_base_bin_rebuild_filter (dvbbasebin);
}

static void
dvb_base_bin_pmt_info_cb (GstElement * mpegtsparse,
    guint program_number, GObject * pmt, DvbBaseBin * dvbbasebin)
{
  DvbBaseBinProgram *program;

  program = dvb_base_bin_get_program (dvbbasebin, program_number);
  if (program == NULL) {
    GST_WARNING ("got PMT for program %d but program not in PAT",
        program_number);
    program = dvb_base_bin_add_program (dvbbasebin, program_number);
  }

  program->old_pmt = program->pmt;
  program->pmt = g_object_ref (pmt);

  /* activate the program if it's selected and either it's not active or its pmt
   * changed */
  if (program->selected && (!program->active || program->old_pmt != NULL))
    dvb_base_bin_activate_program (dvbbasebin, program);

  if (program->old_pmt) {
    g_object_unref (program->old_pmt);
    program->old_pmt = NULL;
  }
}

static gint
get_pad_program_number (GstPad * pad)
{
  gchar *progstr;
  gchar *name;

  name = gst_pad_get_name (pad);

  if (strncmp (name, "program_", 8) != 0) {
    g_free (name);
    return -1;
  }

  progstr = strstr (name, "_");
  g_free (name);
  if (progstr == NULL)
    return -1;

  return strtol (++progstr, NULL, 10);
}

static void
dvb_base_bin_pad_added_cb (GstElement * mpegtsparse,
    GstPad * pad, DvbBaseBin * dvbbasebin)
{
  DvbBaseBinProgram *program;
  gint program_number;

  program_number = get_pad_program_number (pad);
  if (program_number == -1)
    return;

  program = dvb_base_bin_get_program (dvbbasebin, program_number);
  if (program == NULL)
    program = dvb_base_bin_add_program (dvbbasebin, program_number);
  program->selected = TRUE;
  program->ghost = gst_ghost_pad_new (gst_pad_get_name (pad), pad);
  gst_pad_set_active (program->ghost, TRUE);
  gst_element_add_pad (GST_ELEMENT (dvbbasebin), program->ghost);

  /* if the program has a pmt, activate it now, otherwise it will get activated
   * when there's a PMT */
  if (!program->active && program->pmt_pid != G_MAXUINT16)
    dvb_base_bin_activate_program (dvbbasebin, program);
}

static void
dvb_base_bin_pad_removed_cb (GstElement * mpegtsparse,
    GstPad * pad, DvbBaseBin * dvbbasebin)
{
  DvbBaseBinProgram *program;
  gint program_number;

  program_number = get_pad_program_number (pad);
  if (program_number == -1)
    return;

  program = dvb_base_bin_get_program (dvbbasebin, program_number);
  program->selected = FALSE;
  dvb_base_bin_deactivate_program (dvbbasebin, program);
  gst_element_remove_pad (GST_ELEMENT (dvbbasebin), program->ghost);
  program->ghost = NULL;
}

gboolean
gst_dvb_base_bin_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (dvb_base_bin_debug, "dvbbasebin", 0, "DVB bin");

  cam_init ();

  return gst_element_register (plugin, "dvbbasebin",
      GST_RANK_NONE, GST_TYPE_DVB_BASE_BIN);
}
