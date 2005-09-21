/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2005 Philippe Khalaf <burger@speedy.org>
 *
 * gstfdsrc.c: 
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
#include "gst/gst_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include "gstfdsrc.h"

/* the select call is also performed on the control sockets, that way
 * we can send special commands to unblock the select call */
#define CONTROL_STOP            'S'     /* stop the select call */
#define CONTROL_SOCKETS(src)   src->control_sock
#define WRITE_SOCKET(src)      src->control_sock[1]
#define READ_SOCKET(src)       src->control_sock[0]

#define SEND_COMMAND(src, command)          \
G_STMT_START {                              \
  unsigned char c; c = command;             \
  write (WRITE_SOCKET(src), &c, 1);         \
} G_STMT_END

#define READ_COMMAND(src, command, res)        \
G_STMT_START {                                 \
  res = read(READ_SOCKET(src), &command, 1);   \
} G_STMT_END

#define DEFAULT_BLOCKSIZE	4096

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_fdsrc_debug);
#define GST_CAT_DEFAULT gst_fdsrc_debug

GstElementDetails gst_fdsrc_details = GST_ELEMENT_DETAILS ("Disk Source",
    "Source/File",
    "Synchronous read from a file",
    "Erik Walthinsen <omega@cse.ogi.edu>");


/* FdSrc signals and args */
enum
{
  SIGNAL_TIMEOUT,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_FD,
  ARG_BLOCKSIZE,
  ARG_TIMEOUT
};

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_fdsrc_debug, "fdsrc", 0, "fdsrc element");

GST_BOILERPLATE_FULL (GstFdSrc, gst_fdsrc, GstElement, GST_TYPE_PUSH_SRC,
    _do_init);

static void gst_fdsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_fdsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_fdsrc_start (GstBaseSrc * bsrc);
static gboolean gst_fdsrc_stop (GstBaseSrc * bsrc);
static gboolean gst_fdsrc_unlock (GstBaseSrc * bsrc);

static GstFlowReturn gst_fdsrc_create (GstPushSrc * psrc, GstBuffer ** outbuf);

static void
gst_fdsrc_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_set_details (gstelement_class, &gst_fdsrc_details);
}
static void
gst_fdsrc_class_init (GstFdSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstElementClass *gstelement_class;
  GstPushSrcClass *gstpush_src_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpush_src_class = (GstPushSrcClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_PUSH_SRC);

  gobject_class->set_property = gst_fdsrc_set_property;
  gobject_class->get_property = gst_fdsrc_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_FD,
      g_param_spec_int ("fd", "fd", "An open file descriptor to read from",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  gstbasesrc_class->start = gst_fdsrc_start;
  gstbasesrc_class->stop = gst_fdsrc_stop;
  gstbasesrc_class->unlock = gst_fdsrc_unlock;

  gstpush_src_class->create = gst_fdsrc_create;
}

static void
gst_fdsrc_init (GstFdSrc * fdsrc, GstFdSrcClass * klass)
{
  /* TODO set live only if it's actually a live source (check
   * for seekable fd) */
  gst_base_src_set_live (GST_BASE_SRC (fdsrc), TRUE);

  fdsrc->fd = 0;
  fdsrc->curoffset = 0;
}


static gboolean
gst_fdsrc_start (GstBaseSrc * bsrc)
{
  GstFdSrc *src = GST_FDSRC (bsrc);
  gint control_sock[2];

  src->curoffset = 0;

  if (socketpair (PF_UNIX, SOCK_STREAM, 0, control_sock) < 0)
    goto socket_pair;

  READ_SOCKET (src) = control_sock[0];
  WRITE_SOCKET (src) = control_sock[1];

  fcntl (READ_SOCKET (src), F_SETFL, O_NONBLOCK);
  fcntl (WRITE_SOCKET (src), F_SETFL, O_NONBLOCK);

  return TRUE;

  /* ERRORS */
socket_pair:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        GST_ERROR_SYSTEM);
    return FALSE;
  }
}

static gboolean
gst_fdsrc_stop (GstBaseSrc * bsrc)
{
  GstFdSrc *src = GST_FDSRC (bsrc);

  close (READ_SOCKET (src));
  close (WRITE_SOCKET (src));

  return TRUE;
}

static gboolean
gst_fdsrc_unlock (GstBaseSrc * bsrc)
{
  GstFdSrc *src = GST_FDSRC (bsrc);

  SEND_COMMAND (src, CONTROL_STOP);

  return TRUE;
}

static void
gst_fdsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstFdSrc *src;

  g_return_if_fail (GST_IS_FDSRC (object));

  src = GST_FDSRC (object);

  switch (prop_id) {
    case ARG_FD:
      src->fd = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_fdsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFdSrc *src;

  g_return_if_fail (GST_IS_FDSRC (object));

  src = GST_FDSRC (object);

  switch (prop_id) {
    case ARG_FD:
      g_value_set_int (value, src->fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_fdsrc_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstFdSrc *src;
  GstBuffer *buf;
  glong readbytes;
  guint blocksize;

#ifndef HAVE_WIN32
  fd_set readfds;
  gint retval;
#endif

  src = GST_FDSRC (psrc);

#ifndef HAVE_WIN32
  FD_ZERO (&readfds);
  FD_SET (src->fd, &readfds);
  FD_SET (READ_SOCKET (src), &readfds);

  do {
    retval = select (FD_SETSIZE, &readfds, NULL, NULL, NULL);
  } while ((retval == -1 && errno == EINTR));

  if (retval == -1)
    goto select_error;

  if (FD_ISSET (READ_SOCKET (src), &readfds)) {
    /* read all stop commands */
    while (TRUE) {
      gchar command;
      int res;

      READ_COMMAND (src, command, res);
      if (res < 0) {
        GST_LOG_OBJECT (src, "no more commands");
        /* no more commands */
        break;
      }
    }
    goto stopped;
  }
#endif

  blocksize = GST_BASE_SRC (src)->blocksize;

  /* create the buffer */
  buf = gst_buffer_new_and_alloc (blocksize);

  do {
    readbytes = read (src->fd, GST_BUFFER_DATA (buf), blocksize);
  } while (readbytes == -1 && errno == EINTR);  /* retry if interrupted */

  if (readbytes < 0)
    goto read_error;

  if (readbytes == 0)
    goto eos;

  GST_BUFFER_OFFSET (buf) = src->curoffset;
  GST_BUFFER_SIZE (buf) = readbytes;
  GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
  src->curoffset += readbytes;

  GST_DEBUG_OBJECT (psrc, "Read buffer of size %u.", readbytes);

  /* we're done, return the buffer */
  *outbuf = buf;

  return GST_FLOW_OK;

  /* ERRORS */
#ifndef HAVE_WIN32
select_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("select on file descriptor: %s.", g_strerror (errno)));
    GST_DEBUG_OBJECT (psrc, "Error during select");
    return GST_FLOW_ERROR;
  }
stopped:
  {
    GST_DEBUG_OBJECT (psrc, "Select stopped");
    return GST_FLOW_WRONG_STATE;
  }
#endif
eos:
  {
    GST_DEBUG_OBJECT (psrc, "Read 0 bytes. EOS.");
    gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }
read_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("read on file descriptor: %s.", g_strerror (errno)));
    GST_DEBUG_OBJECT (psrc, "Error reading from fd");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
}
