/*
 * GStreamer
 * Copyright (C) 2009 Carl-Anton Ingmarsson <ca.ingmarsson@gmail.com>
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

#ifndef _GST_VDP_VIDEO_SRC_PAD_H_
#define _GST_VDP_VIDEO_SRC_PAD_H_

#include <gst/gst.h>

#include "gstvdpdevice.h"
#include "gstvdpvideobuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_VDP_VIDEO_SRC_PAD             (gst_vdp_video_src_pad_get_type ())
#define GST_VDP_VIDEO_SRC_PAD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VDP_VIDEO_SRC_PAD, GstVdpVideoSrcPad))
#define GST_VDP_VIDEO_SRC_PAD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VDP_VIDEO_SRC_PAD, GstVdpVideoSrcPadClass))
#define GST_IS_VDP_VIDEO_SRC_PAD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VDP_VIDEO_SRC_PAD))
#define GST_IS_VDP_VIDEO_SRC_PAD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VDP_VIDEO_SRC_PAD))
#define GST_VDP_VIDEO_SRC_PAD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VDP_VIDEO_SRC_PAD, GstVdpVideoSrcPadClass))

typedef struct _GstVdpVideoSrcPad GstVdpVideoSrcPad;
typedef struct _GstVdpVideoSrcPadClass GstVdpVideoSrcPadClass;

struct _GstVdpVideoSrcPad
{
  GstPad pad;

  GstCaps *caps;
  GstVdpDevice *device;

  gboolean yuv_output;
  gint width, height;
  guint32 fourcc;

  /* properties */
  gchar *display;
};

struct _GstVdpVideoSrcPadClass
{
  GstPadClass pad_class;
};

GstFlowReturn gst_vdp_video_src_pad_push (GstVdpVideoSrcPad *vdp_pad, GstVdpVideoBuffer *video_buf);
GstFlowReturn gst_vdp_video_src_pad_alloc_buffer (GstVdpVideoSrcPad *vdp_pad, GstVdpVideoBuffer **video_buf);

GstVdpDevice *gst_vdp_video_src_pad_get_device (GstVdpVideoSrcPad *vdp_pad);

gboolean gst_vdp_video_src_pad_set_caps (GstVdpVideoSrcPad *vdp_pad, GstCaps *caps);

GstCaps *gst_vdp_video_src_pad_get_template_caps ();

GstVdpVideoSrcPad * gst_vdp_video_src_pad_new (GstPadTemplate * templ, const gchar * name);

GType gst_vdp_video_src_pad_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _GST_VDP_VIDEO_SRC_PAD_H_ */
