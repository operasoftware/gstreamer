/* GStreamer
 *
 * v4lsrc_calls.c: generic V4L source functions
 *
 * Copyright (C) 2001-2002 Ronald Bultje <rbultje@ronald.bitfreak.net>
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
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "v4lsrc_calls.h"
#include <sys/time.h>

/* number of buffers to be queued *at least* before syncing */
#define MIN_BUFFERS_QUEUED 2

/* On some systems MAP_FAILED seems to be missing */
#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif

GST_DEBUG_CATEGORY_EXTERN (v4l_debug);

#define GST_CAT_DEFAULT v4l_debug

#ifndef GST_DISABLE_GST_DEBUG
/* palette names */
const char *v4l_palette_name[] = {
  "",                           /* 0 */
  "grayscale",                  /* VIDEO_PALETTE_GREY */
  "Hi-420",                     /* VIDEO_PALETTE_HI420 */
  "16-bit RGB (RGB-565)",       /* VIDEO_PALETTE_RB565 */
  "24-bit RGB",                 /* VIDEO_PALETTE_RGB24 */
  "32-bit RGB",                 /* VIDEO_PALETTE_RGB32 */
  "15-bit RGB (RGB-555)",       /* VIDEO_PALETTE_RGB555 */
  "YUV-4:2:2 (packed)",         /* VIDEO_PALETTE_YUV422 */
  "YUYV",                       /* VIDEO_PALETTE_YUYV */
  "UYVY",                       /* VIDEO_PALETTE_UYVY */
  "YUV-4:2:0 (packed)",         /* VIDEO_PALETTE_YUV420 */
  "YUV-4:1:1 (packed)",         /* VIDEO_PALETTE_YUV411 */
  "Raw",                        /* VIDEO_PALETTE_RAW */
  "YUV-4:2:2 (planar)",         /* VIDEO_PALETTE_YUV422P */
  "YUV-4:1:1 (planar)",         /* VIDEO_PALETTE_YUV411P */
  "YUV-4:2:0 (planar)/I420",    /* VIDEO_PALETTE_YUV420P */
  "YUV-4:1:0 (planar)"          /* VIDEO_PALETTE_YUV410P */
};
#endif

/******************************************************
 * gst_v4lsrc_queue_frame():
 *   queue a frame for capturing
 *   (ie. instruct the hardware to start capture)
 *   Requires queue_state lock to be held!
 * return value: TRUE on success, FALSE on error
 ******************************************************/

static gboolean
gst_v4lsrc_queue_frame (GstV4lSrc * v4lsrc, gint num)
{
  GST_LOG_OBJECT (v4lsrc, "queueing frame %d", num);

  if (v4lsrc->frame_queue_state[num] != QUEUE_STATE_READY_FOR_QUEUE) {
    return FALSE;
  }

  /* instruct the driver to prepare capture using buffer frame num */
  v4lsrc->mmap.frame = num;
  if (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd,
          VIDIOCMCAPTURE, &(v4lsrc->mmap)) < 0) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, WRITE, (NULL),
        ("Error queueing a buffer (%d): %s", num, g_strerror (errno)));
    return FALSE;
  }

  v4lsrc->frame_queue_state[num] = QUEUE_STATE_QUEUED;
  v4lsrc->num_queued++;

  return TRUE;
}

/******************************************************
 * gst_v4lsrc_hard_sync_frame(GstV4lSrc *v4lsrc,gint num)
 *   sync a frame and set the timestamp correctly
 *   Requires queue_state lock to be held
 *****************************************************/

static gboolean
gst_v4lsrc_sync_frame (GstV4lSrc * v4lsrc, gint num)
{
  GST_LOG_OBJECT (v4lsrc, "VIOIOCSYNC on frame %d", num);

  if (v4lsrc->frame_queue_state[num] != QUEUE_STATE_QUEUED) {
    return FALSE;
  }

  while (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd, VIDIOCSYNC, &num) < 0) {
    /* if the sync() got interrupted, we can retry */
    if (errno != EINTR) {
      v4lsrc->frame_queue_state[num] = QUEUE_STATE_ERROR;
      GST_ELEMENT_ERROR (v4lsrc, RESOURCE, SYNC, (NULL), GST_ERROR_SYSTEM);
      return FALSE;
    }
    GST_DEBUG_OBJECT (v4lsrc, "Sync got interrupted");
  }
  GST_LOG_OBJECT (v4lsrc, "VIOIOCSYNC on frame %d done", num);

  if (v4lsrc->clock) {
    v4lsrc->timestamp_sync = gst_clock_get_time (v4lsrc->clock);
  } else {
    GTimeVal time;

    g_get_current_time (&time);
    v4lsrc->timestamp_sync = GST_TIMEVAL_TO_TIME (time);
  }

  v4lsrc->frame_queue_state[num] = QUEUE_STATE_SYNCED;
  v4lsrc->num_queued--;

  return TRUE;
}

/******************************************************
 * gst_v4lsrc_set_capture():
 *   set capture parameters, palette = VIDEO_PALETTE_*
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_set_capture (GstV4lSrc * v4lsrc,
    gint width, gint height, gint palette)
{
  GST_DEBUG_OBJECT (v4lsrc,
      "capture properties set to %dx%d, palette %d", width, height, palette);

  v4lsrc->mmap.width = width;
  v4lsrc->mmap.height = height;
  v4lsrc->mmap.format = palette;

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_capture_init():
 *   initialize the capture system
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_capture_init (GstV4lSrc * v4lsrc)
{
  GST_DEBUG_OBJECT (v4lsrc, "initting capture subsystem");
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_NOT_ACTIVE (GST_V4LELEMENT (v4lsrc));

  /* request the mmap buffer info:
   * total size of mmap buffer, number of frames, offsets of frames */
  if (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd, VIDIOCGMBUF,
          &(v4lsrc->mbuf)) < 0) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, READ, (NULL),
        ("Error getting buffer information: %s", g_strerror (errno)));
    return FALSE;
  }

  if (v4lsrc->mbuf.frames < MIN_BUFFERS_QUEUED) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, READ, (NULL),
        ("Not enough buffers. We got %d, we want at least %d",
            v4lsrc->mbuf.frames, MIN_BUFFERS_QUEUED));
    return FALSE;
  }

  GST_INFO_OBJECT (v4lsrc, "Got %d buffers (\'%s\') with total size %d KB",
      v4lsrc->mbuf.frames, v4l_palette_name[v4lsrc->mmap.format],
      v4lsrc->mbuf.size / (v4lsrc->mbuf.frames * 1024));

  /* keep track of queued buffers */
  v4lsrc->frame_queue_state = (gint8 *)
      g_malloc (sizeof (gint8) * v4lsrc->mbuf.frames);

  /* track how often to use each frame */
  v4lsrc->use_num_times = (gint *)
      g_malloc (sizeof (gint) * v4lsrc->mbuf.frames);

  /* lock for the frame_state */
  v4lsrc->mutex_queue_state = g_mutex_new ();
  v4lsrc->cond_queue_state = g_cond_new ();

  /* Map the buffers */
  GST_V4LELEMENT (v4lsrc)->buffer = mmap (0, v4lsrc->mbuf.size,
      PROT_READ | PROT_WRITE, MAP_SHARED, GST_V4LELEMENT (v4lsrc)->video_fd, 0);
  if (GST_V4LELEMENT (v4lsrc)->buffer == MAP_FAILED) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Error mapping video buffers: %s", g_strerror (errno)));
    GST_V4LELEMENT (v4lsrc)->buffer = NULL;
    return FALSE;
  }

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_capture_start():
 *   start streaming capture
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_capture_start (GstV4lSrc * v4lsrc)
{
  int n;

  GST_DEBUG_OBJECT (v4lsrc, "starting capture");
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_ACTIVE (GST_V4LELEMENT (v4lsrc));

  g_mutex_lock (v4lsrc->mutex_queue_state);

  v4lsrc->quit = FALSE;
  v4lsrc->num_queued = 0;
  v4lsrc->sync_frame = 0;
  v4lsrc->queue_frame = 0;

  /* set all buffers ready to queue, and queue captures to the device.
   * This starts streaming capture */
  for (n = 0; n < v4lsrc->mbuf.frames; n++) {
    v4lsrc->frame_queue_state[n] = QUEUE_STATE_READY_FOR_QUEUE;
    if (!gst_v4lsrc_queue_frame (v4lsrc, n)) {
      g_mutex_unlock (v4lsrc->mutex_queue_state);
      gst_v4lsrc_capture_stop (v4lsrc);
      return FALSE;
    }
  }

  v4lsrc->is_capturing = TRUE;
  g_mutex_unlock (v4lsrc->mutex_queue_state);

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_grab_frame():
 *   capture one frame during streaming capture
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_grab_frame (GstV4lSrc * v4lsrc, gint * num)
{
  GST_LOG_OBJECT (v4lsrc, "grabbing frame %d", *num);
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_ACTIVE (GST_V4LELEMENT (v4lsrc));

  g_mutex_lock (v4lsrc->mutex_queue_state);

  /* do we have enough frames? */
  while (v4lsrc->num_queued < MIN_BUFFERS_QUEUED ||
      v4lsrc->frame_queue_state[v4lsrc->queue_frame] ==
      QUEUE_STATE_READY_FOR_QUEUE) {
    while (v4lsrc->frame_queue_state[v4lsrc->queue_frame] !=
        QUEUE_STATE_READY_FOR_QUEUE && !v4lsrc->quit) {
      GST_DEBUG_OBJECT (v4lsrc,
          "Waiting for frames to become available (queued %d < minimum %d)",
          v4lsrc->num_queued, MIN_BUFFERS_QUEUED);
      g_cond_wait (v4lsrc->cond_queue_state, v4lsrc->mutex_queue_state);
    }
    if (v4lsrc->quit) {
      g_mutex_unlock (v4lsrc->mutex_queue_state);
      return FALSE;
    }
    if (!gst_v4lsrc_queue_frame (v4lsrc, v4lsrc->queue_frame)) {
      g_mutex_unlock (v4lsrc->mutex_queue_state);
      return FALSE;
    }
    v4lsrc->queue_frame = (v4lsrc->queue_frame + 1) % v4lsrc->mbuf.frames;
  }

  /* syncing on the buffer grabs it */
  *num = v4lsrc->sync_frame;
  if (!gst_v4lsrc_sync_frame (v4lsrc, *num)) {
    g_mutex_unlock (v4lsrc->mutex_queue_state);
    return FALSE;
  }
  v4lsrc->sync_frame = (v4lsrc->sync_frame + 1) % v4lsrc->mbuf.frames;

  GST_LOG_OBJECT (v4lsrc, "(%" GST_TIME_FORMAT ") grabbed frame %d",
      GST_TIME_ARGS (gst_clock_get_time (v4lsrc->clock)), *num);

  g_mutex_unlock (v4lsrc->mutex_queue_state);

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_get_buffer():
 *   get the address of the given frame number in the mmap'd buffer
 * return value: the buffer's address or NULL
 ******************************************************/

guint8 *
gst_v4lsrc_get_buffer (GstV4lSrc * v4lsrc, gint num)
{
  if (!GST_V4L_IS_ACTIVE (GST_V4LELEMENT (v4lsrc)) ||
      !GST_V4L_IS_OPEN (GST_V4LELEMENT (v4lsrc)))
    return NULL;

  if (num < 0 || num >= v4lsrc->mbuf.frames)
    return NULL;

  return GST_V4LELEMENT (v4lsrc)->buffer + v4lsrc->mbuf.offsets[num];
}


/******************************************************
 * gst_v4lsrc_requeue_frame():
 *   re-queue a frame after we're done with the buffer
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_requeue_frame (GstV4lSrc * v4lsrc, gint num)
{
  GST_LOG_OBJECT (v4lsrc, "requeueing frame %d", num);
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_ACTIVE (GST_V4LELEMENT (v4lsrc));

  /* mark frame as 'ready to requeue' */
  g_mutex_lock (v4lsrc->mutex_queue_state);

  if (v4lsrc->frame_queue_state[num] != QUEUE_STATE_SYNCED) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, TOO_LAZY, (NULL),
        ("Invalid state %d (expected %d), can't requeue",
            v4lsrc->frame_queue_state[num], QUEUE_STATE_SYNCED));
    return FALSE;
  }

  v4lsrc->frame_queue_state[num] = QUEUE_STATE_READY_FOR_QUEUE;

  /* let an optional wait know */
  g_cond_broadcast (v4lsrc->cond_queue_state);

  g_mutex_unlock (v4lsrc->mutex_queue_state);

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_capture_stop():
 *   stop streaming capture
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_capture_stop (GstV4lSrc * v4lsrc)
{
  GST_DEBUG_OBJECT (v4lsrc, "stopping capture");
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_ACTIVE (GST_V4LELEMENT (v4lsrc));

  g_mutex_lock (v4lsrc->mutex_queue_state);
  v4lsrc->is_capturing = FALSE;

  /* make an optional pending wait stop */
  v4lsrc->quit = TRUE;
  g_cond_broadcast (v4lsrc->cond_queue_state);

  /* sync on remaining frames */
  while (1) {
    if (v4lsrc->frame_queue_state[v4lsrc->sync_frame] == QUEUE_STATE_QUEUED) {
      gst_v4lsrc_sync_frame (v4lsrc, v4lsrc->sync_frame);
      v4lsrc->sync_frame = (v4lsrc->sync_frame + 1) % v4lsrc->mbuf.frames;
    } else {
      break;
    }
  }

  g_mutex_unlock (v4lsrc->mutex_queue_state);

  return TRUE;
}


/******************************************************
 * gst_v4lsrc_capture_deinit():
 *   deinitialize the capture system
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_capture_deinit (GstV4lSrc * v4lsrc)
{
  GST_DEBUG_OBJECT (v4lsrc, "quitting capture subsystem");
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_ACTIVE (GST_V4LELEMENT (v4lsrc));

  /* free buffer tracker */
  g_mutex_free (v4lsrc->mutex_queue_state);
  v4lsrc->mutex_queue_state = NULL;
  g_cond_free (v4lsrc->cond_queue_state);
  v4lsrc->cond_queue_state = NULL;
  g_free (v4lsrc->frame_queue_state);
  v4lsrc->frame_queue_state = NULL;
  g_free (v4lsrc->use_num_times);
  v4lsrc->use_num_times = NULL;

  /* unmap the buffer */
  if (munmap (GST_V4LELEMENT (v4lsrc)->buffer, v4lsrc->mbuf.size) == -1) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, CLOSE, (NULL),
        ("error munmap'ing capture buffer: %s", g_strerror (errno)));
    return FALSE;
  }
  GST_V4LELEMENT (v4lsrc)->buffer = NULL;

  return TRUE;
}

/******************************************************
 * gst_v4lsrc_try_capture():
 *   try out a capture on the device
 *   This has to be done before initializing the
 *   actual capture system, to make sure we don't
 *   mess up anything. So we need to mini-mmap()
 *   a buffer here, queue and sync on one buffer,
 *   and unmap it.
 *   This is ugly, yes, I know - but it's a major
 *   design flaw of v4l1 that you don't know in
 *   advance which formats will be supported...
 *   This is better than "just assuming that it'll
 *   work"...
 * return value: TRUE on success, FALSE on error
 ******************************************************/

gboolean
gst_v4lsrc_try_capture (GstV4lSrc * v4lsrc, gint width, gint height,
    gint palette)
{
  /* so, we need a buffer and some more stuff */
  int frame = 0;
  guint8 *buffer;
  struct video_mbuf vmbuf;
  struct video_mmap vmmap;

  GST_DEBUG_OBJECT (v4lsrc, "try out %dx%d, palette format %d (%s)",
      width, height, palette, v4l_palette_name[palette]);
  GST_V4L_CHECK_OPEN (GST_V4LELEMENT (v4lsrc));
  GST_V4L_CHECK_NOT_ACTIVE (GST_V4LELEMENT (v4lsrc));

  /* let's start by requesting a buffer and mmap()'ing it */
  if (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd, VIDIOCGMBUF, &vmbuf) < 0) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, READ, (NULL),
        ("Error getting buffer information: %s", g_strerror (errno)));
    return FALSE;
  }
  /* Map the buffers */
  buffer = mmap (0, vmbuf.size, PROT_READ | PROT_WRITE,
      MAP_SHARED, GST_V4LELEMENT (v4lsrc)->video_fd, 0);
  if (buffer == MAP_FAILED) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Error mapping our try-out buffer: %s", g_strerror (errno)));
    return FALSE;
  }

  /* now that we have a buffer, let's try out our format */
  vmmap.width = width;
  vmmap.height = height;
  vmmap.format = palette;
  vmmap.frame = frame;
  if (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd, VIDIOCMCAPTURE, &vmmap) < 0) {
    if (errno != EINVAL)        /* our format failed! */
      GST_ERROR_OBJECT (v4lsrc,
          "Error queueing our try-out buffer: %s", g_strerror (errno));
    munmap (buffer, vmbuf.size);
    return FALSE;
  }

  if (ioctl (GST_V4LELEMENT (v4lsrc)->video_fd, VIDIOCSYNC, &frame) < 0) {
    GST_ELEMENT_ERROR (v4lsrc, RESOURCE, SYNC, (NULL), GST_ERROR_SYSTEM);
    munmap (buffer, vmbuf.size);
    return FALSE;
  }

  munmap (buffer, vmbuf.size);

  /* if we got here, it worked! woohoo, the format is supported! */
  return TRUE;
}

const char *
gst_v4lsrc_palette_name (int i)
{
  return v4l_palette_name[i];
}

gfloat
gst_v4lsrc_get_fps (GstV4lSrc * v4lsrc)
{
  gint norm;
  gint fps_index;
  gfloat fps;
  struct video_window *vwin = &GST_V4LELEMENT (v4lsrc)->vwin;

  /* check if we have vwin window properties giving a framerate,
   * as is done for webcams
   * See http://www.smcc.demon.nl/webcam/api.html
   * which is used for the Philips and qce-ga drivers */
  fps_index = (vwin->flags >> 16) & 0x3F;       /* 6 bit index for framerate */

  /* webcams have a non-zero fps_index */
  if (fps_index != 0) {
    gfloat current_fps;

    /* index of 16 corresponds to 15 fps */
    current_fps = fps_index * 15.0 / 16;
    GST_LOG_OBJECT (v4lsrc, "device reports fps of %.4f", current_fps);
    return current_fps;
  }

  if (!(v4lsrc->syncmode == GST_V4LSRC_SYNC_MODE_FIXED_FPS) &&
      v4lsrc->clock != NULL && v4lsrc->handled > 0) {
    /* try to get time from clock master and calculate fps */
    GstClockTime time =
        gst_clock_get_time (v4lsrc->clock) - v4lsrc->substract_time;
    return v4lsrc->handled * GST_SECOND / time;
  }

  /* if that failed ... */

  if (!GST_V4L_IS_OPEN (GST_V4LELEMENT (v4lsrc)))
    return 0.;

  if (!gst_v4l_get_chan_norm (GST_V4LELEMENT (v4lsrc), NULL, &norm))
    return 0.;

  if (norm == VIDEO_MODE_NTSC)
    fps = 30000 / 1001;
  else
    fps = 25.;

  return fps;
}
