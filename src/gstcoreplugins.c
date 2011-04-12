/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
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
#  include "config.h"
#endif

#include <gst/gst.h>

extern GType gst_capsfilter_get_type (void);
extern GType gst_fake_sink_get_type (void);
extern GType gst_queue_get_type (void);
extern GType gst_type_find_element_get_type (void);
extern GType gst_multi_queue_get_type (void);

extern gboolean gst_mem_index_plugin_init (GstPlugin * plugin);

struct _elements_entry
{
  const gchar *name;
  guint rank;
    GType (*type) (void);
};


static struct _elements_entry _elements[] = {
  {"capsfilter", GST_RANK_NONE, gst_capsfilter_get_type},
  {"fakesink", GST_RANK_NONE, gst_fake_sink_get_type},
  {"queue", GST_RANK_NONE, gst_queue_get_type},
  {"typefind", GST_RANK_NONE, gst_type_find_element_get_type},
  {"multiqueue", GST_RANK_NONE, gst_multi_queue_get_type},
  {NULL, 0},
};

static gboolean
plugin_init (GstPlugin * plugin)
{
  struct _elements_entry *my_elements = _elements;

  while ((*my_elements).name) {
    if (!gst_element_register (plugin, (*my_elements).name, (*my_elements).rank,
            ((*my_elements).type) ()))
      return FALSE;
    my_elements++;
  }

  if (!gst_mem_index_plugin_init (plugin))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "coreplugins",
    "core GStreamer plugins",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
