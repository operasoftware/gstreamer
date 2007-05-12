/* GStreamer
 * Copyright (C) <2005,2006> Wim Taymans <wim@fluendo.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "rtspurl.h"

#define RTSP_PROTO      "rtsp://"
#define RTSP_PROTO_LEN  7
#define RTSPU_PROTO     "rtspu://"
#define RTSPU_PROTO_LEN 8
#define RTSPT_PROTO     "rtspt://"
#define RTSPT_PROTO_LEN 8

/* format is rtsp[u]://[user:passwd@]host[:port]/abspath[?query] */

RTSPResult
rtsp_url_parse (const gchar * urlstr, RTSPUrl ** url)
{
  RTSPUrl *res;
  gchar *p, *delim, *at, *col;

  g_return_val_if_fail (urlstr != NULL, RTSP_EINVAL);
  g_return_val_if_fail (url != NULL, RTSP_EINVAL);

  res = g_new0 (RTSPUrl, 1);

  p = (gchar *) urlstr;
  if (g_str_has_prefix (p, RTSP_PROTO)) {
    res->transports =
        RTSP_LOWER_TRANS_TCP | RTSP_LOWER_TRANS_UDP |
        RTSP_LOWER_TRANS_UDP_MCAST;
    p += RTSP_PROTO_LEN;
  } else if (g_str_has_prefix (p, RTSPU_PROTO)) {
    res->transports = RTSP_LOWER_TRANS_UDP | RTSP_LOWER_TRANS_UDP_MCAST;
    p += RTSPU_PROTO_LEN;
  } else if (g_str_has_prefix (p, RTSPT_PROTO)) {
    res->transports = RTSP_LOWER_TRANS_TCP;
    p += RTSPT_PROTO_LEN;
  } else
    goto invalid;

  delim = strpbrk (p, "/?");
  at = strchr (p, '@');

  if (at && delim && at > delim)
    at = NULL;

  if (at) {
    col = strchr (p, ':');

    /* must have a ':' and it must be before the '@' */
    if (col == NULL || col > at)
      goto invalid;

    res->user = g_strndup (p, col - p);
    col++;
    res->passwd = g_strndup (col, at - col);

    /* move to host */
    p = at + 1;
  }

  col = strchr (p, ':');
  /* we have a ':' and a delimiter but the ':' is after the delimiter, it's
   * not really part of the hostname */
  if (col && delim && col >= delim)
    col = NULL;

  if (col) {
    res->host = g_strndup (p, col - p);
    p = col + 1;
    res->port = strtoul (p, (char **) &p, 10);
    if (delim)
      p = delim;
  } else {
    /* no port specified, set to 0. _get_port() will return the default port. */
    res->port = 0;
    if (!delim) {
      res->host = g_strdup (p);
      p = NULL;
    } else {
      res->host = g_strndup (p, delim - p);
      p = delim;
    }
  }

  if (p && *p == '/') {
    delim = strchr (p, '?');
    if (!delim) {
      res->abspath = g_strdup (p);
      p = NULL;
    } else {
      res->abspath = g_strndup (p, delim - p);
      p = delim;
    }
  } else {
    res->abspath = g_strdup ("/");
  }

  if (p && *p == '?')
    res->query = g_strdup (p + 1);

  *url = res;

  return RTSP_OK;

  /* ERRORS */
invalid:
  {
    rtsp_url_free (res);
    return RTSP_EINVAL;
  }
}

void
rtsp_url_free (RTSPUrl * url)
{
  if (url == NULL)
    return;

  g_free (url->user);
  g_free (url->passwd);
  g_free (url->host);
  g_free (url->abspath);
  g_free (url->query);
  g_free (url);
}

RTSPResult
rtsp_url_set_port (RTSPUrl * url, guint16 port)
{
  g_return_val_if_fail (url != NULL, RTSP_EINVAL);

  url->port = port;

  return RTSP_OK;
}

RTSPResult
rtsp_url_get_port (RTSPUrl * url, guint16 * port)
{
  g_return_val_if_fail (url != NULL, RTSP_EINVAL);
  g_return_val_if_fail (port != NULL, RTSP_EINVAL);

  /* if a port was specified, use that else use the default port. */
  if (url->port != 0)
    *port = url->port;
  else
    *port = RTSP_DEFAULT_PORT;

  return RTSP_OK;
}

gchar *
rtsp_url_get_request_uri (RTSPUrl * url)
{
  gchar *uri;

  g_return_val_if_fail (url != NULL, NULL);

  if (url->port != 0) {
    uri = g_strdup_printf ("rtsp://%s:%u/%s%s%s", url->host, url->port,
        url->abspath, url->query ? "?" : "", url->query ? url->query : "");
  } else {
    uri = g_strdup_printf ("rtsp://%s/%s%s%s", url->host, url->abspath,
        url->query ? "?" : "", url->query ? url->query : "");
  }

  return uri;
}
