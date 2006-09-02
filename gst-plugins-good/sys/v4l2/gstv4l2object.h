/* GStreamer
 *
 * Copyright (C) 2001-2002 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *               2006 Edgard Lima <edgard.lima@indt.org.br>
 *
 * gstv4l2object.h: base class for V4L2 elements
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

#ifndef __GST_V4L2_OBJECT_H__
#define __GST_V4L2_OBJECT_H__

/* Because of some really cool feature in video4linux1, also known as
 * 'not including sys/types.h and sys/time.h', we had to include it
 * ourselves. In all their intelligence, these people decided to fix
 * this in the next version (video4linux2) in such a cool way that it
 * breaks all compilations of old stuff...
 * The real problem is actually that linux/time.h doesn't use proper
 * macro checks before defining types like struct timeval. The proper
 * fix here is to either fuck the kernel header (which is what we do
 * by defining _LINUX_TIME_H, an innocent little hack) or by fixing it
 * upstream, which I'll consider doing later on. If you get compiler
 * errors here, check your linux/time.h && sys/time.h header setup.
 */
#include <sys/types.h>
#include <linux/types.h>
#define _LINUX_TIME_H
#define __user
#include <linux/videodev2.h>

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <gst/interfaces/propertyprobe.h>

G_BEGIN_DECLS

#define GST_V4L2_OBJECT(obj) (GstV4l2Object *)(obj)

typedef struct _GstV4l2Object GstV4l2Object;
typedef struct _GstV4l2ObjectClassHelper GstV4l2ObjectClassHelper;
typedef struct _GstV4l2Xv GstV4l2Xv;

typedef gboolean  (*GstV4l2GetInOutFunction) (GstV4l2Object * v4l2object, gint * input);
typedef gboolean  (*GstV4l2SetInOutFunction) (GstV4l2Object * v4l2object, gint input);
typedef gboolean  (*GstV4l2UpdateFpsFunction)   (GstV4l2Object * v4l2object);

struct _GstV4l2Object {
  GstElement * element;

  /* the video device */
  char *videodev;

  /* the video-device's file descriptor */
  gint video_fd;

  /* the video buffer (mmap()'ed) */
  guint8 **buffer;

  /* the video device's capabilities */
  struct v4l2_capability vcap;

  /* the video device's window properties */
  struct v4l2_window vwin;

  /* some more info about the current input's capabilities */
  struct v4l2_input vinput;

  /* lists... */
  GList *colors;
  GList *stds;
  GList *inputs;

  /* properties */
  gchar *std;
  gchar *input;
  gulong frequency;


  /* X-overlay */
  GstV4l2Xv *xv;
  gulong xwindow_id;

  /* funcs */
  GstV4l2GetInOutFunction get_in_out_func;
  GstV4l2SetInOutFunction set_in_out_func;
  GstV4l2UpdateFpsFunction   update_fps_func;

};

struct _GstV4l2ObjectClassHelper {
  /* probed devices */
  GList *devices;

};


GType gst_v4l2_object_get_type(void);


#define V4L2_STD_OBJECT_PROPS			\
    PROP_DEVICE,					\
    PROP_DEVICE_NAME,				\
    PROP_FLAGS,					\
    PROP_STD,					\
    PROP_INPUT,					\
    PROP_FREQUENCY


extern GstV4l2Object *
gst_v4l2_object_new (GstElement * element,
                    GstV4l2GetInOutFunction get_in_out_func,
                    GstV4l2SetInOutFunction set_in_out_func,
		    GstV4l2UpdateFpsFunction   update_fps_func);

extern void
gst_v4l2_object_destroy (GstV4l2Object ** v4l2object);

extern void
gst_v4l2_object_install_properties_helper (GObjectClass *gobject_class);

extern gboolean
gst_v4l2_object_set_property_helper (GstV4l2Object *v4l2object,
				    guint prop_id, const GValue * value, GParamSpec * pspec);
extern gboolean
gst_v4l2_object_get_property_helper (GstV4l2Object *v4l2object,
				    guint prop_id, GValue * value, GParamSpec * pspec);

extern gboolean gst_v4l2_object_start (GstV4l2Object *v4l2object);
extern gboolean gst_v4l2_object_stop (GstV4l2Object *v4l2object);

extern const GList *
gst_v4l2_probe_get_properties (GstPropertyProbe * probe);

extern void
gst_v4l2_probe_probe_property (GstPropertyProbe * probe,
			       guint prop_id,
                               const GParamSpec * pspec,
                               GList ** klass_devices);

extern gboolean
gst_v4l2_probe_needs_probe (GstPropertyProbe * probe,
			    guint prop_id,
                            const GParamSpec * pspec,
                            GList ** klass_devices);

extern GValueArray *
gst_v4l2_probe_get_values (GstPropertyProbe * probe,
			   guint prop_id,
                           const GParamSpec * pspec,
                           GList ** klass_devices);

#define GST_IMPLEMENT_V4L2_PROBE_METHODS(Type_Class, interface_as_function)                 \
                                                                                            \
static void                                                                                 \
interface_as_function ## _probe_probe_property (GstPropertyProbe * probe,                   \
			                        guint prop_id,                              \
                                                const GParamSpec * pspec)                   \
{                                                                                           \
  Type_Class *this_class = (Type_Class*) probe;                                             \
  gst_v4l2_probe_probe_property (probe, prop_id, pspec,                                     \
                                        &this_class->v4l2_class_devices);	            \
}                                                                                           \
                                                                                            \
static gboolean                                                                             \
interface_as_function ## _probe_needs_probe (GstPropertyProbe * probe,                      \
			                     guint prop_id,                                 \
                                             const GParamSpec * pspec)                      \
{                                                                                           \
  Type_Class *this_class = (Type_Class*) probe;                                             \
  return gst_v4l2_probe_needs_probe (probe, prop_id, pspec,                                 \
                                        &this_class->v4l2_class_devices);	            \
}                                                                                           \
                                                                                            \
static GValueArray *                                                                        \
interface_as_function ## _probe_get_values (GstPropertyProbe * probe,                       \
			                    guint prop_id,                                  \
                                            const GParamSpec * pspec)                       \
{                                                                                           \
  Type_Class *this_class = (Type_Class*) probe;                                             \
  return gst_v4l2_probe_get_values (probe, prop_id, pspec,                                  \
                                    &this_class->v4l2_class_devices);	                    \
}                                                                                           \
                                                                                            \
static void								                    \
interface_as_function ## _property_probe_interface_init (GstPropertyProbeInterface * iface) \
{                                                                                           \
  iface->get_properties = gst_v4l2_probe_get_properties;                                    \
  iface->probe_property = interface_as_function ## _probe_probe_property;                   \
  iface->needs_probe = interface_as_function ## _probe_needs_probe;                         \
  iface->get_values = interface_as_function ## _probe_get_values;                                            \
}

G_END_DECLS

#endif /* __GST_V4L2_OBJECT_H__ */
