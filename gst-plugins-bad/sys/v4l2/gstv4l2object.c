/*
 * GStreamer gstv4l2object.c: base class for V4L2 elements
 * Copyright (C) 2001-2002 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2006 Edgard Lima <edgard.lima@indt.org.br>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This library is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Library General Public License for more details.
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "v4l2_calls.h"
#include "gstv4l2tuner.h"
#ifdef HAVE_XVIDEO
#include "gstv4l2xoverlay.h"
#endif
#include "gstv4l2colorbalance.h"

enum
{
  PROP_0,
  V4L2_STD_OBJECT_PROPS,
};

const GList *
gst_v4l2_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *list = NULL;

  /* well, not perfect, but better than no locking at all.
   * In the worst case we leak a list node, so who cares? */
  GST_CLASS_LOCK (GST_OBJECT_CLASS (klass));

  if (!list) {
    list = g_list_append (NULL, g_object_class_find_property (klass, "device"));
  }

  GST_CLASS_UNLOCK (GST_OBJECT_CLASS (klass));

  return list;
}

static gboolean
gst_v4l2_class_probe_devices (GstElementClass * klass, gboolean check,
    GList ** klass_devices)
{
  static gboolean init = FALSE;
  static GList *devices = NULL;

  if (!init && !check) {
    const gchar *dev_base[] = { "/dev/video", "/dev/v4l2/video", NULL };
    gint base, n, fd;

    while (devices) {
      GList *item = devices;
      gchar *device = item->data;

      devices = g_list_remove (devices, item);
      g_free (device);
    }

    /*
     * detect /dev entries 
     */
    for (n = 0; n < 64; n++) {
      for (base = 0; dev_base[base] != NULL; base++) {
        struct stat s;
        gchar *device = g_strdup_printf ("%s%d",
            dev_base[base],
            n);

        /*
         * does the /dev/ entry exist at all? 
         */
        if (stat (device, &s) == 0) {
          /*
           * yes: is a device attached? 
           */
          if (S_ISCHR (s.st_mode)) {

            if ((fd = open (device, O_RDWR | O_NONBLOCK)) > 0 || errno == EBUSY) {
              if (fd > 0)
                close (fd);

              devices = g_list_append (devices, device);
              break;
            }
          }
        }
        g_free (device);
      }
    }

    init = TRUE;
  }

  *klass_devices = devices;

  return init;
}

void
gst_v4l2_probe_probe_property (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec, GList ** klass_devices)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (probe);

  switch (prop_id) {
    case PROP_DEVICE:
      gst_v4l2_class_probe_devices (klass, FALSE, klass_devices);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

gboolean
gst_v4l2_probe_needs_probe (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec, GList ** klass_devices)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_DEVICE:
      ret = !gst_v4l2_class_probe_devices (klass, TRUE, klass_devices);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;

}

static GValueArray *
gst_v4l2_class_list_devices (GstElementClass * klass, GList ** klass_devices)
{
  GValueArray *array;
  GValue value = { 0 };
  GList *item;

  if (!*klass_devices)
    return NULL;

  array = g_value_array_new (g_list_length (*klass_devices));
  item = *klass_devices;
  g_value_init (&value, G_TYPE_STRING);
  while (item) {
    gchar *device = item->data;

    g_value_set_string (&value, device);
    g_value_array_append (array, &value);

    item = item->next;
  }
  g_value_unset (&value);

  return array;
}

GValueArray *
gst_v4l2_probe_get_values (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec, GList ** klass_devices)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (probe);
  GValueArray *array = NULL;

  switch (prop_id) {
    case PROP_DEVICE:
      array = gst_v4l2_class_list_devices (klass, klass_devices);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return array;
}

#define GST_TYPE_V4L2_DEVICE_FLAGS (gst_v4l2_device_get_type ())
GType
gst_v4l2_device_get_type (void)
{
  static GType v4l2_device_type = 0;

  if (v4l2_device_type == 0) {
    static const GFlagsValue values[] = {
      {V4L2_CAP_VIDEO_CAPTURE, "CAPTURE",
          "Device supports video capture"},
      {V4L2_CAP_VIDEO_OUTPUT, "PLAYBACK",
          "Device supports video playback"},
      {V4L2_CAP_VIDEO_OVERLAY, "OVERLAY",
          "Device supports video overlay"},

      {V4L2_CAP_VBI_CAPTURE, "VBI_CAPTURE",
          "Device supports the VBI capture"},
      {V4L2_CAP_VBI_OUTPUT, "VBI_OUTPUT",
          "Device supports the VBI output"},

      {V4L2_CAP_TUNER, "TUNER",
          "Device has a tuner or modulator"},
      {V4L2_CAP_AUDIO, "AUDIO",
          "Device has audio inputs or outputs"},

      {0, NULL, NULL}
    };

    v4l2_device_type =
        g_flags_register_static ("GstV4l2DeviceTypeFlags", values);
  }

  return v4l2_device_type;
}

void
gst_v4l2_object_install_properties_helper (GObjectClass * gobject_class)
{

  g_object_class_install_property
      (G_OBJECT_CLASS (gobject_class), PROP_DEVICE,
      g_param_spec_string ("device",
          "Device", "Device location", NULL, G_PARAM_READWRITE));
  g_object_class_install_property
      (G_OBJECT_CLASS (gobject_class),
      PROP_DEVICE_NAME,
      g_param_spec_string ("device_name",
          "Device name", "Name of the device", NULL, G_PARAM_READABLE));
  g_object_class_install_property
      (G_OBJECT_CLASS (gobject_class), PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags",
          "Device type flags",
          GST_TYPE_V4L2_DEVICE_FLAGS, 0, G_PARAM_READABLE));
  g_object_class_install_property
      (gobject_class, PROP_STD,
      g_param_spec_string ("std", "std",
          "standard (norm) to use", NULL, G_PARAM_READWRITE));
  g_object_class_install_property
      (gobject_class, PROP_INPUT,
      g_param_spec_string ("input",
          "input",
          "input/output (channel) to switch to", NULL, G_PARAM_READWRITE));
  g_object_class_install_property
      (gobject_class, PROP_FREQUENCY,
      g_param_spec_ulong ("frequency",
          "frequency",
          "frequency to tune to (in Hz)", 0, G_MAXULONG, 0, G_PARAM_READWRITE));

}

GstV4l2Object *
gst_v4l2_object_new (GstElement * element,
    GstV4l2GetInOutFunction get_in_out_func,
    GstV4l2SetInOutFunction set_in_out_func,
    GstV4l2UpdateFpsFunction update_fps_func)
{

  GstV4l2Object *v4l2object;

  /*
   * some default values 
   */

  v4l2object = g_new0 (GstV4l2Object, 1);

  v4l2object->element = element;
  v4l2object->get_in_out_func = get_in_out_func;
  v4l2object->set_in_out_func = set_in_out_func;
  v4l2object->update_fps_func = update_fps_func;

  v4l2object->video_fd = -1;
  v4l2object->buffer = NULL;
  v4l2object->videodev = g_strdup ("/dev/video0");

  v4l2object->stds = NULL;
  v4l2object->inputs = NULL;
  v4l2object->colors = NULL;

  v4l2object->xwindow_id = 0;

  return v4l2object;

}


void
gst_v4l2_object_destroy (GstV4l2Object ** v4l2object)
{

  if (*v4l2object) {

    if ((*v4l2object)->videodev) {
      g_free ((*v4l2object)->videodev);
      (*v4l2object)->videodev = NULL;
    }

    g_free (*v4l2object);
    *v4l2object = NULL;

  }

}


gboolean
gst_v4l2_object_set_property_helper (GstV4l2Object * v4l2object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{

  switch (prop_id) {
    case PROP_DEVICE:
      if (v4l2object->videodev)
        g_free (v4l2object->videodev);
      v4l2object->videodev = g_strdup (g_value_get_string (value));
      break;
    case PROP_STD:
      if (GST_V4L2_IS_OPEN (v4l2object)) {
        GstTuner *tuner = GST_TUNER (v4l2object->element);
        GstTunerNorm *norm = gst_tuner_find_norm_by_name (tuner,
            (gchar *)
            g_value_get_string (value));

        if (norm) {
          /* like gst_tuner_set_norm (tuner, norm)
             without g_object_notify */
          gst_v4l2_tuner_set_norm (v4l2object, norm);
        }
      } else {
        g_free (v4l2object->std);
        v4l2object->std = g_value_dup_string (value);
      }
      break;
    case PROP_INPUT:
      if (GST_V4L2_IS_OPEN (v4l2object)) {
        GstTuner *tuner = GST_TUNER (v4l2object->element);
        GstTunerChannel *channel = gst_tuner_find_channel_by_name (tuner,
            (gchar *)
            g_value_get_string (value));

        if (channel) {
          /* like gst_tuner_set_channel (tuner, channel)
             without g_object_notify */
          gst_v4l2_tuner_set_channel (v4l2object, channel);
        }
      } else {
        g_free (v4l2object->input);
        v4l2object->input = g_value_dup_string (value);
      }
      break;
    case PROP_FREQUENCY:
      if (GST_V4L2_IS_OPEN (v4l2object)) {
        GstTuner *tuner = GST_TUNER (v4l2object->element);
        GstTunerChannel *channel = gst_tuner_get_channel (tuner);

        if (channel &&
            GST_TUNER_CHANNEL_HAS_FLAG (channel, GST_TUNER_CHANNEL_FREQUENCY)) {
          /* like
             gst_tuner_set_frequency (tuner, channel, g_value_get_ulong (value))
             without g_object_notify */
          gst_v4l2_tuner_set_frequency (v4l2object, channel,
              g_value_get_ulong (value));
        }
      } else {
        v4l2object->frequency = g_value_get_ulong (value);
      }
      break;
    default:
      return FALSE;
      break;
  }

  return TRUE;

}


gboolean
gst_v4l2_object_get_property_helper (GstV4l2Object * v4l2object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, v4l2object->videodev);
      break;
    case PROP_DEVICE_NAME:
    {
      gchar *new = NULL;

      if (GST_V4L2_IS_OPEN (v4l2object))
        new = (gchar *) v4l2object->vcap.card;
      g_value_set_string (value, new);
      break;
    }
    case PROP_FLAGS:
    {
      guint flags = 0;

      if (GST_V4L2_IS_OPEN (v4l2object)) {
        flags |= v4l2object->vcap.capabilities &
            (V4L2_CAP_VIDEO_CAPTURE |
            V4L2_CAP_VIDEO_OUTPUT |
            V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_TUNER | V4L2_CAP_AUDIO);
        if (v4l2object->vcap.capabilities & V4L2_CAP_AUDIO)
          flags |= V4L2_FBUF_CAP_CHROMAKEY;
      }
      g_value_set_flags (value, flags);
      break;
    }
    case PROP_STD:
      g_value_set_string (value, v4l2object->std);
      break;
    case PROP_INPUT:
      g_value_set_string (value, v4l2object->input);
      break;
    case PROP_FREQUENCY:
      g_value_set_ulong (value, v4l2object->frequency);
      break;
    default:
      return FALSE;
      break;
  }

  return TRUE;

}

static void
gst_v4l2_set_defaults (GstV4l2Object * v4l2object)
{
  GstTunerNorm *norm = NULL;
  GstTunerChannel *channel = NULL;
  GstTuner *tuner = GST_TUNER (v4l2object->element);

  if (v4l2object->std)
    norm = gst_tuner_find_norm_by_name (tuner, v4l2object->std);
  if (norm) {
    gst_tuner_set_norm (tuner, norm);
  } else {
    norm =
        GST_TUNER_NORM (gst_tuner_get_norm (GST_TUNER (v4l2object->element)));
    if (norm) {
      v4l2object->std = g_strdup (norm->label);
      gst_tuner_norm_changed (tuner, norm);
      g_object_notify (G_OBJECT (v4l2object->element), "std");
    }
  }

  if (v4l2object->input)
    channel = gst_tuner_find_channel_by_name (tuner, v4l2object->input);
  if (channel) {
    gst_tuner_set_channel (tuner, channel);
  } else {
    channel =
        GST_TUNER_CHANNEL (gst_tuner_get_channel (GST_TUNER (v4l2object->
                element)));
    v4l2object->input = g_strdup (channel->label);
    gst_tuner_channel_changed (tuner, channel);
    g_object_notify (G_OBJECT (v4l2object->element), "input");
  }

  if (GST_TUNER_CHANNEL_HAS_FLAG (channel, GST_TUNER_CHANNEL_FREQUENCY)) {
    if (v4l2object->frequency != 0) {
      gst_tuner_set_frequency (tuner, channel, v4l2object->frequency);
    } else {
      v4l2object->frequency = gst_tuner_get_frequency (tuner, channel);
      if (v4l2object->frequency == 0) {
        /* guess */
        gst_tuner_set_frequency (tuner, channel, 1000);
      } else {
        g_object_notify (G_OBJECT (v4l2object->element), "frequency");
      }
    }
  }
}


gboolean
gst_v4l2_object_start (GstV4l2Object * v4l2object)
{
  if (gst_v4l2_open (v4l2object))
    gst_v4l2_set_defaults (v4l2object);
  else
    return FALSE;


#ifdef HAVE_XVIDEO
  gst_v4l2_xoverlay_start (v4l2object);
#endif

  return TRUE;
}

gboolean
gst_v4l2_object_stop (GstV4l2Object * v4l2object)
{

#ifdef HAVE_XVIDEO
  gst_v4l2_xoverlay_stop (v4l2object);
#endif

  if (!gst_v4l2_close (v4l2object))
    return FALSE;

  return TRUE;
}
