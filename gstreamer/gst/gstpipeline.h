/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstpipeline.h: Header for GstPipeline element
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


#ifndef __GST_PIPELINE_H__
#define __GST_PIPELINE_H__

#include <gst/gstbin.h>

G_BEGIN_DECLS

extern GstElementDetails gst_pipeline_details;

#define GST_TYPE_PIPELINE 		(gst_pipeline_get_type ())
#define GST_PIPELINE(obj) 		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PIPELINE, GstPipeline))
#define GST_IS_PIPELINE(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PIPELINE))
#define GST_PIPELINE_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PIPELINE, GstPipelineClass))
#define GST_IS_PIPELINE_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PIPELINE))
#define GST_PIPELINE_GET_CLASS(obj) 	(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PIPELINE, GstPipelineClass))

typedef struct _GstPipeline GstPipeline;
typedef struct _GstPipelineClass GstPipelineClass;

struct _GstPipeline {
  GstBin 	 bin;

  gpointer	 dummy[32];
};

struct _GstPipelineClass {
  GstBinClass parent_class;

  gpointer	 dummy[32];
};

GType		gst_pipeline_get_type		(void);
GstElement*	gst_pipeline_new		(const gchar *name);
#define		gst_pipeline_destroy(pipeline)	gst_object_destroy(GST_OBJECT(pipeline))


G_END_DECLS

#endif /* __GST_PIPELINE_H__ */

