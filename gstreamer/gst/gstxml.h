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


#ifndef __GST_XML_H__
#define __GST_XML_H__

#include <gst/gst.h>
#include <gnome-xml/parser.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GST_TYPE_XML \
  (gst_object_get_type())
#define GST_XML(obj) \
  (GTK_CHECK_CAST((obj),GST_TYPE_XML,GstXML))
#define GST_XML_CLASS(klass) \
  (GTK_CHECK_CLASS_CAST((klass),GST_TYPE_XML,GstXMLClass))
#define GST_IS_XML(obj) \
  (GTK_CHECK_TYPE((obj),GST_TYPE_XML))
#define GST_IS_XML_CLASS(obj) \
  (GTK_CHECK_CLASS_TYPE((klass),GST_TYPE_XML))

typedef struct _GstXML GstXML;
typedef struct _GstXMLClass GstXMLClass;

struct _GstXML {
  GtkObject object;
};

struct _GstXMLClass {
  GtkObjectClass parent_class;
};

GtkType gst_xml_get_type(void);


/* create an XML document out of a pipeline */
xmlDocPtr gst_xml_write(GstElement *element);

GstXML *gst_xml_new(const guchar *fname, const guchar *root);

GstElement *gst_xml_get_element(GstXML *xml, const guchar *name);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GST_XML_H__ */
