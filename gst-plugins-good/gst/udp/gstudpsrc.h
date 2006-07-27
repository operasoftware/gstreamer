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


#ifndef __GST_UDPSRC_H__
#define __GST_UDPSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "gstudpnetutils.h"

#include "gstudp.h"

#define GST_TYPE_UDPSRC \
  (gst_udpsrc_get_type())
#define GST_UDPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_UDPSRC,GstUDPSrc))
#define GST_UDPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_UDPSRC,GstUDPSrcClass))
#define GST_IS_UDPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_UDPSRC))
#define GST_IS_UDPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_UDPSRC))

typedef struct _GstUDPSrc GstUDPSrc;
typedef struct _GstUDPSrcClass GstUDPSrcClass;

struct _GstUDPSrc {
  GstPushSrc parent;

  /* properties */
  gchar *uri;
  int port;
  gchar *multi_group;
  gint ttl;
  gint buffer_size;

  int sock;
  int control_sock[2];

  struct sockaddr_in myaddr;
  struct ip_mreq multi_addr;

  GstCaps *caps;
};

struct _GstUDPSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_udpsrc_get_type(void);

G_END_DECLS


#endif /* __GST_UDPSRC_H__ */
