/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gst.h: Main header for GStreamer, apps should include this
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


#ifndef __GST_H__
#define __GST_H__

#include <gst/gstinfo.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstbuffer.h>
#include <gst/gstcpu.h>
#include <gst/gstclock.h>
#include <gst/gstelement.h>
#include <gst/gstextratypes.h>
#include <gst/gstbin.h>
#include <gst/gstpipeline.h>
#include <gst/gstthread.h>
#include <gst/gsttype.h>
#include <gst/gstcaps.h>
#include <gst/gstprops.h>
#include <gst/gstplugin.h>
#include <gst/gstutils.h>
#include <gst/gsttrace.h>
#include <gst/gstxml.h>
#include <gst/gsttee.h>
#include <gst/cothreads.h>

/* initialize GST */
void gst_init(int *argc,char **argv[]);

void gst_main		(void);
void gst_main_quit	(void);

#include <gst/gstlog.h>

#endif /* __GST_H__ */
