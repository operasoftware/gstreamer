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


#ifndef __GST_PAD_H__
#define __GST_PAD_H__


#include <gnome-xml/parser.h>

#include <gst/gstobject.h>
#include <gst/gstbuffer.h>
#include <gst/cothreads.h>
#include <gst/gstcaps.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GST_TYPE_PAD                 	(gst_pad_get_type ())
#define GST_PAD(obj)                 	(GTK_CHECK_CAST ((obj), GST_TYPE_PAD,GstPad))
#define GST_PAD_CLASS(klass)         	(GTK_CHECK_CLASS_CAST ((klass), GST_TYPE_PAD,GstPadClass))
#define GST_IS_PAD(obj)              	(GTK_CHECK_TYPE ((obj), GST_TYPE_PAD))
#define GST_IS_PAD_CLASS(obj)        	(GTK_CHECK_CLASS_TYPE ((klass), GST_TYPE_PAD))

// quick test to see if the pad is connected
#define GST_PAD_CONNECTED(pad) 		((pad) && (pad)->peer != NULL)
#define GST_PAD_CAN_PULL(pad) 		((pad) && (pad)->pullfunc != NULL)

typedef struct _GstPad GstPad;
typedef struct _GstPadClass GstPadClass;

/* this defines the functions used to chain buffers
 * pad is the sink pad (so the same chain function can be used for N pads)
 * buf is the buffer being passed */
typedef void (*GstPadChainFunction) (GstPad *pad,GstBuffer *buf);
typedef void (*GstPadPullFunction) (GstPad *pad);
typedef void (*GstPadPullRegionFunction) (GstPad *pad, gulong offset, gulong size);
typedef void (*GstPadPushFunction) (GstPad *pad);
typedef void (*GstPadQoSFunction) (GstPad *pad, glong qos_message);

typedef enum {
  GST_PAD_UNKNOWN,
  GST_PAD_SRC,
  GST_PAD_SINK,
} GstPadDirection;

typedef enum {
  GST_PAD_DISABLED		= GST_OBJECT_FLAG_LAST,

  GST_PAD_FLAG_LAST		= GST_OBJECT_FLAG_LAST+2,
} GstPadFlags;

struct _GstPad {
  GstObject object;

  gchar *name;
  GstCaps *caps;

  cothread_state *threadstate;

  GstPadDirection direction;

  GstPad *peer;

  GstBuffer *bufpen;

  GstPadChainFunction chainfunc;
  GstPadPushFunction pushfunc;
  GstPadPullFunction pullfunc;
  GstPadPullRegionFunction pullregionfunc;
  GstPadQoSFunction qosfunc;

  GstObject *parent;
  GList *ghostparents;
};

struct _GstPadClass {
  GstObjectClass parent_class;

  /* signal callbacks */
  void (*set_active)	(GstPad *pad, gboolean active);
  void (*caps_changed)	(GstPad *pad, GstCaps *newcaps);
};

typedef enum {
  GST_PAD_ALWAYS,
  GST_PAD_SOMETIMES,
} GstPadPresence;

typedef struct _GstPadTemplate GstPadTemplate;

/* template */
struct _GstPadTemplate {
  gchar           *name_template;
  GstPadDirection direction;
  GstPadPresence  presence;
  GstCaps  	  *caps;
};

/* factory */
typedef gpointer GstPadFactoryEntry;
typedef GstPadFactoryEntry GstPadFactory[];
#define GST_PAD_FACTORY_ALWAYS 		GINT_TO_POINTER(GST_PAD_ALWAYS)
#define GST_PAD_FACTORY_SOMETIMES 	GINT_TO_POINTER(GST_PAD_SOMETIMES)

#define GST_PAD_FACTORY_SRC	 	GINT_TO_POINTER(GST_PAD_SRC)
#define GST_PAD_FACTORY_SINK 		GINT_TO_POINTER(GST_PAD_SINK)

GtkType 		gst_pad_get_type		(void);

GstPad*			gst_pad_new			(gchar *name, GstPadDirection direction);
#define 		gst_pad_destroy(pad) 		gst_object_destroy (GST_OBJECT (pad))
GstPad*			gst_pad_new_from_template	(GstPadTemplate *temp, gchar *name);

GstPadDirection 	gst_pad_get_direction		(GstPad *pad);

void 			gst_pad_set_chain_function	(GstPad *pad, GstPadChainFunction chain);
void 			gst_pad_set_pull_function	(GstPad *pad, GstPadPullFunction pull);
void			gst_pad_set_pullregion_function	(GstPad *pad, GstPadPullRegionFunction pullregion);
void 			gst_pad_set_qos_function	(GstPad *pad, GstPadQoSFunction qos);

void	 		gst_pad_set_caps		(GstPad *pad, GstCaps *caps);
GstCaps* 		gst_pad_get_caps		(GstPad *pad);

void 			gst_pad_set_name		(GstPad *pad, const gchar *name);
const gchar*		gst_pad_get_name		(GstPad *pad);

void 			gst_pad_set_parent		(GstPad *pad, GstObject *parent);
GstObject*		gst_pad_get_parent		(GstPad *pad);
void 			gst_pad_add_ghost_parent	(GstPad *pad, GstObject *parent);
void 			gst_pad_remove_ghost_parent	(GstPad *pad, GstObject *parent);
GList*			gst_pad_get_ghost_parents	(GstPad *pad);

GstPad*			gst_pad_get_peer		(GstPad *pad);

void 			gst_pad_connect			(GstPad *srcpad, GstPad *sinkpad);
void 			gst_pad_disconnect		(GstPad *srcpad, GstPad *sinkpad);

void 			gst_pad_push			(GstPad *pad, GstBuffer *buffer);
GstBuffer*		gst_pad_pull			(GstPad *pad);
GstBuffer*		gst_pad_pull_region		(GstPad *pad, gulong offset, gulong size);
void 			gst_pad_handle_qos		(GstPad *pad, glong qos_message);

xmlNodePtr 		gst_pad_save_thyself		(GstPad *pad, xmlNodePtr parent);
void 			gst_pad_load_and_connect	(xmlNodePtr parent, GstObject *element, GHashTable *elements);


/* factory */
GstPadTemplate*		gst_padtemplate_new		(GstPadFactory *factory);
GstPadTemplate*		gst_padtemplate_create		(gchar *name_template, 
		                                         GstPadDirection direction, GstPadPresence presence,
							 GstCaps *caps);

xmlNodePtr 		gst_padtemplate_save_thyself	(GstPadTemplate *pad, xmlNodePtr parent);
GstPadTemplate*		gst_padtemplate_load_thyself	(xmlNodePtr parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_PAD_H__ */     

