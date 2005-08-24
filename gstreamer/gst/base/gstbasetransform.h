/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstbasetransform.h:
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


#ifndef __GST_BASE_TRANSFORM_H__
#define __GST_BASE_TRANSFORM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_BASE_TRANSFORM		(gst_base_transform_get_type())
#define GST_BASE_TRANSFORM(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_TRANSFORM,GstBaseTransform))
#define GST_BASE_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_TRANSFORM,GstBaseTransformClass))
#define GST_BASE_TRANSFORM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_BASE_TRANSFORM,GstBaseTransformClass))
#define GST_IS_BASE_TRANSFORM(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_TRANSFORM))
#define GST_IS_BASE_TRANSFORM_CLASS(obj)(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_TRANSFORM))

/* the names of the templates for the sink and source pads */
#define GST_BASE_TRANSFORM_SINK_NAME	"sink"
#define GST_BASE_TRANSFORM_SRC_NAME	"src"

typedef struct _GstBaseTransform GstBaseTransform;
typedef struct _GstBaseTransformClass GstBaseTransformClass;

struct _GstBaseTransform {
  GstElement	 element;

  /* source and sink pads */
  GstPad	*sinkpad;
  GstPad	*srcpad;

  gboolean	 passthrough;

  gboolean	 in_place;
  guint		 out_size;

  gboolean	 delay_configure;
  gboolean	 pending_configure;

  /*< private >*/
  gpointer       _gst_reserved[GST_PADDING];
};

/**
 * GstBaseTransformClass::transform_caps:
 * @pad: the pad
 * @caps: the caps
 *
 * This method should answer the question "given this pad, and given these
 * caps, what caps would you allow on the other pad inside your element ?"
 */
struct _GstBaseTransformClass {
  GstElementClass parent_class;

  /*< public >*/
  /* virtual methods for subclasses */

  /* given the (non-)fixed simple caps on the pad in the given direction,
   * what can I do on the other pad ? */
  /* FIXME: change to direction */
  GstCaps*	(*transform_caps) (GstBaseTransform *trans, GstPad *pad,
                                   GstCaps *caps);

  /* given caps on one pad, how would you fixate caps on the other pad ? */
  void		(*fixate_caps)	  (GstBaseTransform *trans,
                                   GstPadDirection direction, GstCaps *caps,
                                   GstCaps *othercaps);

  /* given the size of a buffer in the given direction with the given caps,
   * calculate the
   * size of an outgoing buffer with the given outgoing caps; the default
   * implementation uses get_size and keeps the number of units the same */
  guint         (*transform_size) (GstBaseTransform *trans,
                                   GstPadDirection direction,
                                   GstCaps *incaps, guint insize,
                                   GstCaps *outcaps);

  /* get the byte size of one unit for a given caps, -1 on error.
   * Always needs to be implemented if the transform is not in-place. */
  guint         (*get_size)     (GstBaseTransform *trans, GstCaps *caps);

  /* notify the subclass of new caps */
  gboolean      (*set_caps)     (GstBaseTransform *trans, GstCaps *incaps,
                                 GstCaps *outcaps);

  /* start and stop processing, ideal for opening/closing the resource */
  gboolean      (*start)        (GstBaseTransform *trans);
  gboolean      (*stop)         (GstBaseTransform *trans);

  gboolean      (*event)        (GstBaseTransform *trans, GstEvent *event);

  /* transform one incoming buffer to one outgoing buffer.
   * Always needs to be implemented. */
  GstFlowReturn (*transform)    (GstBaseTransform *trans, GstBuffer *inbuf,
                                 GstBuffer *outbuf);

  /* transform a buffer inplace */
  GstFlowReturn (*transform_ip) (GstBaseTransform *trans, GstBuffer *buf);

  /*< private >*/
  gpointer       _gst_reserved[GST_PADDING];
};

void		gst_base_transform_set_passthrough (GstBaseTransform *trans, gboolean passthrough);
gboolean	gst_base_transform_is_passthrough (GstBaseTransform *trans);

GType gst_base_transform_get_type (void);

G_END_DECLS

#endif /* __GST_BASE_TRANSFORM_H__ */
