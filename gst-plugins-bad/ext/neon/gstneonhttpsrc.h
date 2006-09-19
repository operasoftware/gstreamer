/* vim: set sw=2: -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2; c-indent-level: 2 -*- */
/* GStreamer
 * Copyright (C) <2005> Edgard Lima <edgard.lima@indt.org.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more 
 */

#ifndef __GST_NEONHTTP_SRC_H__
#define __GST_NEONHTTP_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <stdio.h>

G_BEGIN_DECLS

#include <ne_session.h>
#include <ne_request.h>
#include <ne_socket.h>

#define GST_TYPE_NEONHTTP_SRC \
  (gst_neonhttp_src_get_type())
#define GST_NEONHTTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NEONHTTP_SRC,GstNeonhttpSrc))
#define GST_NEONHTTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NEONHTTP_SRC,GstNeonhttpSrcClass))
#define GST_IS_NEONHTTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NEONHTTP_SRC))
#define GST_IS_NEONHTTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NEONHTTP_SRC))

typedef struct _GstNeonhttpSrc GstNeonhttpSrc;
typedef struct _GstNeonhttpSrcClass GstNeonhttpSrcClass;

struct _GstNeonhttpSrc {
  GstPushSrc element;

  /* socket */
  ne_session *session;
  ne_request *request;
  ne_uri uri;
  gchar *uristr;
  ne_uri proxy;
  gchar *user_agent;

  gboolean ishttps;

  guint64 content_size;

  gboolean eos;

  /* icecast/audiocast metadata extraction handling */
  gboolean iradio_mode;
  gboolean lastfm_mode;
  gchar *iradio_name;
  gchar *iradio_genre;
  gchar *iradio_url;
  GstCaps *icy_caps;
  gint icy_metaint;

  /* enable Neon HTTP redirects (HTTP 302 status code) */
  gboolean neon_http_redirect;

  /* enable Neon HTTP debug messages */
  gboolean neon_http_msgs_dbg;
};

struct _GstNeonhttpSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_neonhttp_src_get_type (void);

G_END_DECLS

#endif /* __GST_NEONHTTP_SRC_H__ */
