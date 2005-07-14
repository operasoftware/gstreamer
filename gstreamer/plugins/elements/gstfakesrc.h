/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstfakesrc.h:
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


#ifndef __GST_FAKE_SRC_H__
#define __GST_FAKE_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>

G_BEGIN_DECLS

typedef enum {
  FAKE_SRC_FIRST_LAST_LOOP = 1,
  FAKE_SRC_LAST_FIRST_LOOP,
  FAKE_SRC_PING_PONG,
  FAKE_SRC_ORDERED_RANDOM,
  FAKE_SRC_RANDOM,
  FAKE_SRC_PATTERN_LOOP,
  FAKE_SRC_PING_PONG_PATTERN,
  FAKE_SRC_GET_ALWAYS_SUCEEDS
} GstFakeSrcOutputType;

typedef enum {
  FAKE_SRC_DATA_ALLOCATE = 1,
  FAKE_SRC_DATA_SUBBUFFER
} GstFakeSrcDataType;

typedef enum {
  FAKE_SRC_SIZETYPE_NULL = 1,
  FAKE_SRC_SIZETYPE_FIXED,
  FAKE_SRC_SIZETYPE_RANDOM
} GstFakeSrcSizeType;

typedef enum {
  FAKE_SRC_FILLTYPE_NOTHING = 1,
  FAKE_SRC_FILLTYPE_NULL,
  FAKE_SRC_FILLTYPE_RANDOM,
  FAKE_SRC_FILLTYPE_PATTERN,
  FAKE_SRC_FILLTYPE_PATTERN_CONT
} GstFakeSrcFillType;

#define GST_TYPE_FAKE_SRC \
  (gst_fake_src_get_type())
#define GST_FAKE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FAKE_SRC,GstFakeSrc))
#define GST_FAKE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FAKE_SRC,GstFakeSrcClass))
#define GST_IS_FAKE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FAKE_SRC))
#define GST_IS_FAKE_SRC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FAKE_SRC))

typedef struct _GstFakeSrc GstFakeSrc;
typedef struct _GstFakeSrcClass GstFakeSrcClass;

struct _GstFakeSrc {
  GstBaseSrc     element;

  /*< private >*/
  gboolean	 has_loop;
  gboolean	 has_getrange;

  GstFakeSrcOutputType output;
  GstFakeSrcDataType   data;
  GstFakeSrcSizeType   sizetype;
  GstFakeSrcFillType   filltype;

  guint		sizemin;
  guint		sizemax;
  GstBuffer	*parent;
  guint		parentsize;
  guint		parentoffset;
  guint8	 pattern_byte;
  gchar		*pattern;
  GList		*patternlist;
  gint		 datarate;
  gboolean	 sync;
  GstClock	*clock;
  gint64	 segment_start;
  gint64	 segment_end;
  gboolean	 segment_loop;
  gint		 num_buffers;
  gint		 rt_num_buffers; /* we are going to change this at runtime */
  guint64	 buffer_count;
  gboolean	 silent;
  gboolean	 signal_handoffs;
  gboolean	 dump;

  guint64        bytes_sent;

  gchar		*last_message;
};

struct _GstFakeSrcClass {
  GstBaseSrcClass parent_class;

  /*< public >*/
  /* signals */
  void (*handoff) (GstElement *element, GstBuffer *buf, GstPad *pad);
};

GType gst_fake_src_get_type (void);

G_END_DECLS

#endif /* __GST_FAKE_SRC_H__ */
