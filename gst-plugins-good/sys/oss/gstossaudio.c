/* GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstosselement.h"
#include "gstosssink.h"
#include "gstosssrc.h"
#include "gstossgst.h"

static gboolean
plugin_init (GModule *module, GstPlugin *plugin)
{
  if (!gst_library_load ("gstaudio"))
    return FALSE;

  if (!gst_osselement_factory_init (plugin) ||
      !gst_osssrc_factory_init (plugin) ||
      !gst_osssink_factory_init (plugin) ||
      !gst_ossgst_factory_init (plugin)) {
    g_warning ("Failed to register OSS elements!");
    return FALSE;
  }

  return TRUE;
}

GstPluginDesc plugin_desc = {
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "ossaudio",
  plugin_init
};
