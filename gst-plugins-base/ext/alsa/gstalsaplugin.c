/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Thomas Nyberg <thomas@codefactory.se>
 * Copyright (C) 2001-2002 Andy Wingo <apwingo@eos.ncsu.edu>
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstalsa.h"
#include "gstalsasink.h"
#include "gstalsasrc.h"

GST_DEBUG_CATEGORY (alsa_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (alsa_debug, "alsa", 0, "alsa plugins");

  if (!gst_element_register (plugin, "alsasrc", GST_RANK_NONE, GST_TYPE_ALSA_SRC))
    return FALSE;
  if (!gst_element_register (plugin, "alsasink", GST_RANK_NONE, GST_TYPE_ALSA_SINK))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "alsa",
  "ALSA plugin library",
  plugin_init,
  VERSION,
  "LGPL",
  GST_COPYRIGHT,
  GST_PACKAGE,
  GST_ORIGIN
)
