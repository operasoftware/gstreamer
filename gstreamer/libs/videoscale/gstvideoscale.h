/* Gnome-Streamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_VIDEOSCALE_H__
#define __GST_VIDEOSCALE_H__

#include <gst/gst.h>

typedef enum {
  GST_VIDEOSCALE_NEAREST,
  GST_VIDEOSCALE_BILINEAR,
  GST_VIDEOSCALE_BICUBIC
} GstVideoScaleMethod;

typedef struct _GstVideoScale GstVideoScale;

struct _GstVideoScale {
  guint source_width;
  guint source_height;
  guint dest_width;
  guint dest_height;
  guint format;
  GstVideoScaleMethod method;
  /* private */
  guchar copy_row[4096];
  guchar *temp;
  void (*scale) (GstVideoScale *scale, unsigned char *src, unsigned char *dest);
  void (*scaler) (GstVideoScale *scale, unsigned char *src, unsigned char *dest, int sw, int sh, int dw, int dh);
  unsigned char (*filter) (unsigned char *src, double x, double y, int sw, int sh);
};

GstVideoScale *gst_videoscale_new(int sw, int sh, int dw, int dh, int format, GstVideoScaleMethod method);
void gst_videoscale_destroy(GstVideoScale *scale);

#endif /* __GST_VIDEOSCALE_H__ */
