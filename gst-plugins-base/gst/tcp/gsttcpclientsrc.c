/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
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

#include <gst/gst-i18n-plugin.h>
#include "gsttcp.h"
#include "gsttcpclientsrc.h"
#include <string.h>             /* memset */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#ifdef HAVE_FIONREAD_IN_SYS_FILIO
#include <sys/filio.h>
#endif

GST_DEBUG_CATEGORY (tcpclientsrc_debug);
#define GST_CAT_DEFAULT tcpclientsrc_debug

#define MAX_READ_SIZE			4 * 1024

/* elementfactory information */
static GstElementDetails gst_tcpclientsrc_details =
GST_ELEMENT_DETAILS ("TCP Client source",
    "Source/Network",
    "Receive data as a client over the network via TCP",
    "Thomas Vander Stichele <thomas at apestaart dot org>");

/* TCPClientSrc signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_HOST,
  ARG_PORT,
  ARG_PROTOCOL
};

static void gst_tcpclientsrc_base_init (gpointer g_class);
static void gst_tcpclientsrc_class_init (GstTCPClientSrc * klass);
static void gst_tcpclientsrc_init (GstTCPClientSrc * tcpclientsrc);

static GstCaps *gst_tcpclientsrc_getcaps (GstPad * pad);

static GstData *gst_tcpclientsrc_get (GstPad * pad);
static GstElementStateReturn gst_tcpclientsrc_change_state (GstElement *
    element);

static void gst_tcpclientsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tcpclientsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_tcpclientsrc_set_clock (GstElement * element, GstClock * clock);

static GstElementClass *parent_class = NULL;

/*static guint gst_tcpclientsrc_signals[LAST_SIGNAL] = { 0 }; */

GType
gst_tcpclientsrc_get_type (void)
{
  static GType tcpclientsrc_type = 0;


  if (!tcpclientsrc_type) {
    static const GTypeInfo tcpclientsrc_info = {
      sizeof (GstTCPClientSrcClass),
      gst_tcpclientsrc_base_init,
      NULL,
      (GClassInitFunc) gst_tcpclientsrc_class_init,
      NULL,
      NULL,
      sizeof (GstTCPClientSrc),
      0,
      (GInstanceInitFunc) gst_tcpclientsrc_init,
      NULL
    };

    tcpclientsrc_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstTCPClientSrc",
        &tcpclientsrc_info, 0);
  }
  return tcpclientsrc_type;
}

static void
gst_tcpclientsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_tcpclientsrc_details);
}

static void
gst_tcpclientsrc_class_init (GstTCPClientSrc * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_HOST,
      g_param_spec_string ("host", "Host",
          "The host IP address to receive packets from", TCP_DEFAULT_HOST,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PORT,
      g_param_spec_int ("port", "Port", "The port to receive packets from", 0,
          TCP_HIGHEST_PORT, TCP_DEFAULT_PORT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_PROTOCOL,
      g_param_spec_enum ("protocol", "Protocol", "The protocol to wrap data in",
          GST_TYPE_TCP_PROTOCOL_TYPE, GST_TCP_PROTOCOL_TYPE_NONE,
          G_PARAM_READWRITE));

  gobject_class->set_property = gst_tcpclientsrc_set_property;
  gobject_class->get_property = gst_tcpclientsrc_get_property;

  gstelement_class->change_state = gst_tcpclientsrc_change_state;
  gstelement_class->set_clock = gst_tcpclientsrc_set_clock;

  GST_DEBUG_CATEGORY_INIT (tcpclientsrc_debug, "tcpclientsrc", 0,
      "TCP Client Source");
}

static void
gst_tcpclientsrc_set_clock (GstElement * element, GstClock * clock)
{
  GstTCPClientSrc *tcpclientsrc;

  tcpclientsrc = GST_TCPCLIENTSRC (element);

  tcpclientsrc->clock = clock;
}

static void
gst_tcpclientsrc_init (GstTCPClientSrc * this)
{
  /* create the src pad */
  this->srcpad = gst_pad_new ("src", GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (this), this->srcpad);
  gst_pad_set_get_function (this->srcpad, gst_tcpclientsrc_get);
  gst_pad_set_getcaps_function (this->srcpad, gst_tcpclientsrc_getcaps);

  this->port = TCP_DEFAULT_PORT;
  this->host = g_strdup (TCP_DEFAULT_HOST);
  this->clock = NULL;
  this->sock_fd = -1;
  this->protocol = GST_TCP_PROTOCOL_TYPE_NONE;
  this->curoffset = 0;
  this->caps = NULL;

  GST_FLAG_UNSET (this, GST_TCPCLIENTSRC_OPEN);
}

static GstCaps *
gst_tcpclientsrc_getcaps (GstPad * pad)
{
  GstTCPClientSrc *src;
  GstCaps *caps = NULL;

  src = GST_TCPCLIENTSRC (GST_OBJECT_PARENT (pad));

  if (!GST_FLAG_IS_SET (src, GST_TCPCLIENTSRC_OPEN))
    caps = gst_caps_new_any ();
  else if (src->caps)
    caps = gst_caps_copy (src->caps);
  else
    caps = gst_caps_new_any ();
  GST_DEBUG_OBJECT (src, "returning caps %" GST_PTR_FORMAT, caps);
  g_assert (GST_IS_CAPS (caps));
  return caps;
}

/* close the socket and associated resources
 * unset OPEN flag
 * used both to recover from errors and go to NULL state */
static void
gst_tcpclientsrc_close (GstTCPClientSrc * this)
{
  GST_DEBUG_OBJECT (this, "closing socket");
  if (this->sock_fd != -1) {
    close (this->sock_fd);
    this->sock_fd = -1;
  }
  this->caps_received = FALSE;
  if (this->caps) {
    gst_caps_free (this->caps);
    this->caps = NULL;
  }
  GST_FLAG_UNSET (this, GST_TCPCLIENTSRC_OPEN);
}

/* close socket and related items and return an EOS GstData
 * called from _get */
static GstData *
gst_tcpclientsrc_eos (GstTCPClientSrc * src)
{
  GST_DEBUG_OBJECT (src, "going to EOS");
  gst_element_set_eos (GST_ELEMENT (src));
  gst_tcpclientsrc_close (src);
  return GST_DATA (gst_event_new (GST_EVENT_EOS));
}

static GstData *
gst_tcpclientsrc_get (GstPad * pad)
{
  GstTCPClientSrc *src;
  size_t readsize;
  int ret;

  GstData *data = NULL;
  GstBuffer *buf = NULL;

  g_return_val_if_fail (pad != NULL, NULL);
  g_return_val_if_fail (GST_IS_PAD (pad), NULL);
  src = GST_TCPCLIENTSRC (GST_OBJECT_PARENT (pad));
  if (!GST_FLAG_IS_SET (src, GST_TCPCLIENTSRC_OPEN)) {
    GST_DEBUG_OBJECT (src, "connection to server closed, cannot give data");
    return NULL;
  }
  GST_LOG_OBJECT (src, "asked for a buffer");

  /* try to negotiate here */
  if (!gst_pad_is_negotiated (pad)) {
    if (GST_PAD_LINK_FAILED (gst_pad_renegotiate (pad))) {
      GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL), GST_ERROR_SYSTEM);
      gst_buffer_unref (buf);
      return gst_tcpclientsrc_eos (src);
    }
  }

  /* if we have a left over buffer after a discont, return that */
  if (src->buffer_after_discont) {
    buf = src->buffer_after_discont;
    GST_LOG_OBJECT (src,
        "Returning buffer after discont of size %d, ts %"
        GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
        ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
        GST_BUFFER_SIZE (buf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buf)),
        GST_BUFFER_OFFSET (buf), GST_BUFFER_OFFSET_END (buf));
    src->buffer_after_discont = NULL;
    return GST_DATA (buf);
  }

  /* read the buffer header if we're using a protocol */
  switch (src->protocol) {
      fd_set testfds;

    case GST_TCP_PROTOCOL_TYPE_NONE:
      /* do a blocking select on the socket */
      FD_ZERO (&testfds);
      FD_SET (src->sock_fd, &testfds);
      ret = select (src->sock_fd + 1, &testfds, (fd_set *) 0, (fd_set *) 0, 0);
      /* no action (0) is an error too in our case */
      if (ret <= 0) {
        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("select failed: %s", g_strerror (errno)));
        return gst_tcpclientsrc_eos (src);
      }

      /* ask how much is available for reading on the socket */
      ret = ioctl (src->sock_fd, FIONREAD, &readsize);
      if (ret < 0) {
        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("ioctl failed: %s", g_strerror (errno)));
        return gst_tcpclientsrc_eos (src);
      }
      GST_LOG_OBJECT (src, "ioctl says %d bytes available", readsize);
      buf = gst_buffer_new_and_alloc (readsize);
      break;
    case GST_TCP_PROTOCOL_TYPE_GDP:
      if (!(data = gst_tcp_gdp_read_header (GST_ELEMENT (src), src->sock_fd))) {
        return gst_tcpclientsrc_eos (src);
      }
      if (GST_IS_EVENT (data)) {
        /* if we got back an EOS event, then we should go into eos ourselves */
        if (GST_EVENT_TYPE (data) == GST_EVENT_EOS) {
          gst_event_unref (data);
          return gst_tcpclientsrc_eos (src);
        }
        return data;
      }

      buf = GST_BUFFER (data);

      GST_LOG_OBJECT (src, "Going to read data from socket into buffer %p",
          buf);
      /* use this new buffer to read data into */
      readsize = GST_BUFFER_SIZE (buf);
      break;
    default:
      g_warning ("Unhandled protocol type");
      break;
  }

  GST_LOG_OBJECT (src, "Reading %d bytes into buffer", readsize);
  ret = gst_tcp_socket_read (src->sock_fd, GST_BUFFER_DATA (buf), readsize);
  if (ret < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    return gst_tcpclientsrc_eos (src);
  }

  /* if we read 0 bytes, and we're blocking, we hit eos */
  if (ret == 0) {
    GST_DEBUG_OBJECT (src, "blocking read returns 0, EOS");
    gst_buffer_unref (buf);
    return gst_tcpclientsrc_eos (src);
  }

  readsize = ret;
  GST_BUFFER_SIZE (buf) = readsize;
  GST_BUFFER_MAXSIZE (buf) = readsize;

  /* FIXME: we could decide to set OFFSET and OFFSET_END for non-protocol
   * streams to mean the bytes processed */

  /* if this is our first buffer, we need to send a discont with the
   * given timestamp or the current offset, and store the buffer for
   * the next iteration through the get loop */
  if (src->send_discont) {
    GstClockTime timestamp;
    GstEvent *event;

    src->send_discont = FALSE;
    src->buffer_after_discont = buf;
    /* if the timestamp is valid, send a timed discont
     * taking into account the incoming buffer's timestamps */
    timestamp = GST_BUFFER_TIMESTAMP (buf);
    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      GST_DEBUG_OBJECT (src,
          "sending discontinuous with timestamp %" GST_TIME_FORMAT,
          GST_TIME_ARGS (timestamp));
      event =
          gst_event_new_discontinuous (FALSE, GST_FORMAT_TIME, timestamp, NULL);
      return GST_DATA (event);
    }
    /* otherwise, send an offset discont */
    GST_DEBUG_OBJECT (src, "sending discontinuous with offset %d",
        src->curoffset);
    event =
        gst_event_new_discontinuous (FALSE, GST_FORMAT_BYTES, src->curoffset,
        NULL);
    return GST_DATA (event);
  }

  src->curoffset += readsize;
  GST_LOG_OBJECT (src,
      "Returning buffer from _get of size %d, ts %"
      GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
      ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      GST_BUFFER_SIZE (buf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)),
      GST_BUFFER_OFFSET (buf), GST_BUFFER_OFFSET_END (buf));
  return GST_DATA (buf);
}

static void
gst_tcpclientsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTCPClientSrc *tcpclientsrc;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_TCPCLIENTSRC (object));
  tcpclientsrc = GST_TCPCLIENTSRC (object);

  switch (prop_id) {
    case ARG_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (tcpclientsrc->host);
      tcpclientsrc->host = g_strdup (g_value_get_string (value));
      break;
    case ARG_PORT:
      tcpclientsrc->port = g_value_get_int (value);
      break;
    case ARG_PROTOCOL:
      tcpclientsrc->protocol = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tcpclientsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstTCPClientSrc *tcpclientsrc;

  g_return_if_fail (GST_IS_TCPCLIENTSRC (object));
  tcpclientsrc = GST_TCPCLIENTSRC (object);

  switch (prop_id) {
    case ARG_HOST:
      g_value_set_string (value, tcpclientsrc->host);
      break;
    case ARG_PORT:
      g_value_set_int (value, tcpclientsrc->port);
      break;
    case ARG_PROTOCOL:
      g_value_set_enum (value, tcpclientsrc->protocol);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* create a socket for connecting to remote server */
static gboolean
gst_tcpclientsrc_init_receive (GstTCPClientSrc * this)
{
  int ret;
  gchar *ip;

  /* create receiving client socket */
  GST_DEBUG_OBJECT (this, "opening receiving client socket to %s:%d",
      this->host, this->port);
  if ((this->sock_fd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
    GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ, (NULL), GST_ERROR_SYSTEM);
    return FALSE;
  }
  GST_DEBUG_OBJECT (this, "opened receiving client socket with fd %d",
      this->sock_fd);
  GST_FLAG_SET (this, GST_TCPCLIENTSRC_OPEN);

  /* look up name if we need to */
  ip = gst_tcp_host_to_ip (GST_ELEMENT (this), this->host);
  if (!ip) {
    gst_tcpclientsrc_close (this);
    return FALSE;
  }
  GST_DEBUG_OBJECT (this, "IP address for host %s is %s", this->host, ip);

  /* connect to server */
  memset (&this->server_sin, 0, sizeof (this->server_sin));
  this->server_sin.sin_family = AF_INET;        /* network socket */
  this->server_sin.sin_port = htons (this->port);       /* on port */
  this->server_sin.sin_addr.s_addr = inet_addr (ip);    /* on host ip */
  g_free (ip);

  GST_DEBUG_OBJECT (this, "connecting to server");
  ret = connect (this->sock_fd, (struct sockaddr *) &this->server_sin,
      sizeof (this->server_sin));

  if (ret) {
    gst_tcpclientsrc_close (this);
    switch (errno) {
      case ECONNREFUSED:
        GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ,
            (_("Connection to %s:%d refused."), this->host, this->port),
            (NULL));
        return FALSE;
        break;
      default:
        GST_ELEMENT_ERROR (this, RESOURCE, OPEN_READ, (NULL),
            ("connect to %s:%d failed: %s", this->host, this->port,
                g_strerror (errno)));
        return FALSE;
        break;
    }
  }

  this->send_discont = TRUE;
  this->buffer_after_discont = NULL;

  /* get the caps if we're using GDP */
  if (this->protocol == GST_TCP_PROTOCOL_TYPE_GDP) {
    /* if we haven't received caps yet, we should get them first */
    if (!this->caps_received) {
      GstCaps *caps;

      GST_DEBUG_OBJECT (this, "getting caps through GDP");
      if (!(caps = gst_tcp_gdp_read_caps (GST_ELEMENT (this), this->sock_fd))) {
        gst_tcpclientsrc_close (this);
        GST_ELEMENT_ERROR (this, RESOURCE, READ, (NULL),
            ("Could not read caps through GDP"));
        return FALSE;
      }
      if (!GST_IS_CAPS (caps)) {
        gst_tcpclientsrc_close (this);
        GST_ELEMENT_ERROR (this, RESOURCE, READ, (NULL),
            ("Could not read caps through GDP"));
        return FALSE;
      }
      GST_DEBUG_OBJECT (this, "Received caps through GDP: %" GST_PTR_FORMAT,
          caps);
      this->caps_received = TRUE;
      this->caps = caps;
    }
  }
  return TRUE;
}

static GstElementStateReturn
gst_tcpclientsrc_change_state (GstElement * element)
{
  g_return_val_if_fail (GST_IS_TCPCLIENTSRC (element), GST_STATE_FAILURE);

  /* if open and going to NULL, close it */
  if (GST_FLAG_IS_SET (element, GST_TCPCLIENTSRC_OPEN) &&
      GST_STATE_PENDING (element) == GST_STATE_NULL) {
    gst_tcpclientsrc_close (GST_TCPCLIENTSRC (element));
  }
  /* if closed and going to a state higher than NULL, open it */
  if (!GST_FLAG_IS_SET (element, GST_TCPCLIENTSRC_OPEN) &&
      GST_STATE_PENDING (element) > GST_STATE_NULL) {
    if (!gst_tcpclientsrc_init_receive (GST_TCPCLIENTSRC (element)))
      return GST_STATE_FAILURE;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}
