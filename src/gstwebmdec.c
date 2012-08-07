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
#include "gstvp8lib.h"

extern gboolean gst_matroska_demux_plugin_init (GstPlugin * plugin);
extern GType gst_vp8_dec_get_type (void);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!verify_lib_init())
      return FALSE;

  if (!gst_matroska_demux_plugin_init (plugin))
      return FALSE;

  if (!gst_element_register (plugin, "opera_vp8dec", GST_RANK_PRIMARY + 1,
          gst_vp8_dec_get_type ()))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "webmdec",
    "WebM decoder elements",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
