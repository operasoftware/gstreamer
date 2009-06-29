/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *               <2009> Opera Software, Philip Jagenstedt <philipj@opera.com>
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
#include "config.h"
#endif

#include <gst/gst.h>

extern gboolean gst_ogg_demux_plugin_init (GstPlugin * plugin);
extern GType gst_vorbis_dec_get_type (void);
extern GType gst_theora_dec_get_type (void);

GST_DEBUG_CATEGORY (vorbisdec_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_ogg_demux_plugin_init (plugin))
    return FALSE;

  if (!gst_element_register (plugin, "vorbisdec", GST_RANK_PRIMARY,
          gst_vorbis_dec_get_type ()))
    return FALSE;

  if (!gst_element_register (plugin, "theoradec", GST_RANK_PRIMARY,
          gst_theora_dec_get_type ()))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (vorbisdec_debug, "vorbisdec", 0,
      "vorbis decoding element");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "oggdec",
    "Ogg Vorbis/Theora decoder elements",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
