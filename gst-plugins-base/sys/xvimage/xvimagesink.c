/* GStreamer
 * Copyright (C) <2003> Julien Moutte <julien@moutte.net>
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

/* Our interfaces */
#include <gst/navigation/navigation.h>
#include <gst/xoverlay/xoverlay.h>
#include <gst/colorbalance/colorbalance.h>

/* Object header */
#include "xvimagesink.h"

/* ElementFactory information */
static GstElementDetails gst_xvimagesink_details = GST_ELEMENT_DETAILS (
  "Video sink",
  "Sink/Video",
  "A Xv based videosink",
  "Julien Moutte <julien@moutte.net>"
);

/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_xvimagesink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "video/x-raw-rgb, "
      "framerate = (double) [ 0, MAX ], "
      "width = (int) [ 0, MAX ], "
      "height = (int) [ 0, MAX ]; "
    "video/x-raw-yuv, "
      "framerate = (double) [ 0, MAX ], "
      "width = (int) [ 0, MAX ], "
      "height = (int) [ 0, MAX ]"
  )
);

static GstVideoSinkClass *parent_class = NULL;

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

/* X11 stuff */

/* This function handles GstXvImage creation depending on XShm availability */
static GstXvImage *
gst_xvimagesink_xvimage_new (GstXvImageSink *xvimagesink,
                             gint width, gint height)
{
  GstXvImage *xvimage = NULL;
  
  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);
  
  xvimage = g_new0 (GstXvImage, 1);
  
  xvimage->width = width;
  xvimage->height = height;
  xvimage->data = NULL;
  xvimage->xvimagesink = xvimagesink;
  
  g_mutex_lock (xvimagesink->x_lock);

  xvimage->size =  (xvimagesink->xcontext->bpp / 8) * xvimage->width * xvimage->height;
  
#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm)
    {
      xvimage->xvimage = XvShmCreateImage (xvimagesink->xcontext->disp,
                                           xvimagesink->xcontext->xv_port_id,
                                           xvimagesink->xcontext->im_format,
                                           NULL, xvimage->width,
                                           xvimage->height, &xvimage->SHMInfo);
      
      xvimage->SHMInfo.shmid = shmget (IPC_PRIVATE, xvimage->size,
                                       IPC_CREAT | 0777);
  
      xvimage->SHMInfo.shmaddr = shmat (xvimage->SHMInfo.shmid, 0, 0);
      xvimage->xvimage->data = xvimage->SHMInfo.shmaddr;
  
      xvimage->SHMInfo.readOnly = FALSE;
  
      XShmAttach (xvimagesink->xcontext->disp, &xvimage->SHMInfo);
    }
  else
#endif /* HAVE_XSHM */
    {
      xvimage->xvimage = XvCreateImage (xvimagesink->xcontext->disp,
                                        xvimagesink->xcontext->xv_port_id,
                                        xvimagesink->xcontext->im_format,
                                        xvimage->data,
                                        xvimage->width, xvimage->height);
      
      xvimage->data = g_malloc (xvimage->xvimage->data_size);
    }

  if (xvimage->xvimage)
    {
      XSync(xvimagesink->xcontext->disp, 0);
    }
  else
    {
      if (xvimage->data)
        g_free (xvimage->data);
      
      g_free (xvimage);
      
      xvimage = NULL;
    }
    
  g_mutex_unlock (xvimagesink->x_lock);
  
  return xvimage;
}

/* This function destroys a GstXvImage handling XShm availability */ 
static void
gst_xvimagesink_xvimage_destroy (GstXvImageSink *xvimagesink,
                                 GstXvImage *xvimage)
{
  g_return_if_fail (xvimage != NULL);
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  g_mutex_lock (xvimagesink->x_lock);
  
#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm)
    {
      if (xvimage->SHMInfo.shmaddr)
        XShmDetach (xvimagesink->xcontext->disp, &xvimage->SHMInfo);
  
      if (xvimage->xvimage)
        XFree (xvimage->xvimage);
  
      if (xvimage->SHMInfo.shmaddr)
        shmdt (xvimage->SHMInfo.shmaddr);
  
      if (xvimage->SHMInfo.shmid > 0)
        shmctl (xvimage->SHMInfo.shmid, IPC_RMID, 0);
    }
  else
#endif /* HAVE_XSHM */ 
    {
      if (xvimage->xvimage)
        XFree (xvimage->xvimage);
    }

  g_mutex_unlock (xvimagesink->x_lock);
  
  g_free (xvimage);
}

/* This function puts a GstXvImage on a GstXvImageSink's window */
static void
gst_xvimagesink_xvimage_put (GstXvImageSink *xvimagesink, GstXvImage *xvimage)
{
  g_return_if_fail (xvimage != NULL);
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  g_mutex_lock (xvimagesink->x_lock);
  
  /* We scale to the window's geometry */
#ifdef HAVE_XSHM
  if (xvimagesink->xcontext->use_xshm)
    {  
      XvShmPutImage (xvimagesink->xcontext->disp,
                     xvimagesink->xcontext->xv_port_id,
                     xvimagesink->xwindow->win, 
                     xvimagesink->xwindow->gc, xvimage->xvimage,
                     0, 0, xvimage->width, xvimage->height,
                     0, 0, xvimagesink->xwindow->width,
                     xvimagesink->xwindow->height, FALSE);
    }
  else
#endif /* HAVE_XSHM */
    {
      XvPutImage (xvimagesink->xcontext->disp,
                  xvimagesink->xcontext->xv_port_id,
                  xvimagesink->xwindow->win, 
                  xvimagesink->xwindow->gc, xvimage->xvimage,
                  0, 0, xvimage->width, xvimage->height,
                  0, 0, xvimagesink->xwindow->width,
                  xvimagesink->xwindow->height);
    }
  
  XSync(xvimagesink->xcontext->disp, FALSE);
  
  g_mutex_unlock (xvimagesink->x_lock);
}

/* This function handles a GstXWindow creation */
static GstXWindow *
gst_xvimagesink_xwindow_new (GstXvImageSink *xvimagesink,
                             gint width, gint height)
{
  GstXWindow *xwindow = NULL;
  XGCValues values;
  
  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);
  
  xwindow = g_new0 (GstXWindow, 1);
  
  xwindow->width = width;
  xwindow->height = height;
  xwindow->internal = TRUE;
  
  g_mutex_lock (xvimagesink->x_lock);
  
  xwindow->win = XCreateSimpleWindow (xvimagesink->xcontext->disp,
                                      xvimagesink->xcontext->root, 
                                      0, 0, xwindow->width, xwindow->height, 
                                      0, 0, xvimagesink->xcontext->black);
  
  XSelectInput (xvimagesink->xcontext->disp, xwindow->win,  ExposureMask |
                StructureNotifyMask | PointerMotionMask | KeyPressMask |
                KeyReleaseMask | ButtonPressMask | ButtonReleaseMask);
  
  xwindow->gc = XCreateGC (xvimagesink->xcontext->disp,
                           xwindow->win, 0, &values);
  
  XMapRaised (xvimagesink->xcontext->disp, xwindow->win);
  
  g_mutex_unlock (xvimagesink->x_lock);
  
  return xwindow;
}

/* This function destroys a GstXWindow */
static void
gst_xvimagesink_xwindow_destroy (GstXvImageSink *xvimagesink, GstXWindow *xwindow)
{
  g_return_if_fail (xwindow != NULL);
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  g_mutex_lock (xvimagesink->x_lock);
  
  /* If we did not create that window we just free the GC and let it live */
  if (xwindow->internal)
    XDestroyWindow (xvimagesink->xcontext->disp, xwindow->win);
  else
    XSelectInput (xvimagesink->xcontext->disp, xwindow->win, 0);
  
  XFreeGC (xvimagesink->xcontext->disp, xwindow->gc);
  
  g_mutex_unlock (xvimagesink->x_lock);
  
  g_free (xwindow);
}

/* This function resizes a GstXWindow */
static void
gst_xvimagesink_xwindow_resize (GstXvImageSink *xvimagesink,
                                GstXWindow *xwindow, guint width, guint height)
{
  g_return_if_fail (xwindow != NULL);
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  g_mutex_lock (xvimagesink->x_lock);
  
  xwindow->width = width;
  xwindow->height = height;
  
  XResizeWindow (xvimagesink->xcontext->disp, xwindow->win,
                 xwindow->width, xwindow->height);

  g_mutex_unlock (xvimagesink->x_lock);
}

/* This function handles XEvents that might be in the queue. It generates
   GstEvent that will be sent upstream in the pipeline to handle interactivity
   and navigation. It will also listen for configure events on the window to
   trigger caps renegotiation so on the fly software scaling can work. */
static void
gst_xvimagesink_handle_xevents (GstXvImageSink *xvimagesink, GstPad *pad)
{
  XEvent e;
  
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  /* We get all events on our window to throw them upstream */
  g_mutex_lock (xvimagesink->x_lock);
  while (XCheckWindowEvent (xvimagesink->xcontext->disp,
                            xvimagesink->xwindow->win,
                            ExposureMask | StructureNotifyMask |
                            PointerMotionMask | KeyPressMask |
                            KeyReleaseMask | ButtonPressMask |
                            ButtonReleaseMask, &e))
    {
      KeySym keysym;
      
      /* We lock only for the X function call */
      g_mutex_unlock (xvimagesink->x_lock);
      
      switch (e.type)
        {
          case ConfigureNotify:
            /* Window got resized or moved. We update our data. */
            GST_DEBUG ("xvimagesink window is at %d, %d with geometry : %d,%d",
                       e.xconfigure.x, e.xconfigure.y,
                       e.xconfigure.width, e.xconfigure.height);
            xvimagesink->xwindow->width = e.xconfigure.width;
            xvimagesink->xwindow->height = e.xconfigure.height;
            break;
          case MotionNotify:
            /* Mouse pointer moved over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("xvimagesink pointer moved over window at %d,%d",
                       e.xmotion.x, e.xmotion.y);
            gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
                                             "mouse-move", 0, 
                                             e.xmotion.x, e.xmotion.y);
            break;
          case ButtonPress:
            /* Mouse button pressed over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("xvimagesink button %d pressed over window at %d,%d",
                       e.xbutton.button, e.xbutton.x, e.xbutton.y);
            gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
                                             "mouse-button-press",
                                             e.xbutton.button,
                                             e.xbutton.x, e.xbutton.y);
            break;
          case ButtonRelease:
            /* Mouse button released over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("xvimagesink button %d released over window at %d,%d",
                       e.xbutton.button, e.xbutton.x, e.xbutton.y);
            gst_navigation_send_mouse_event (GST_NAVIGATION (xvimagesink),
                                             "mouse-button-release",
                                             e.xbutton.button,
                                             e.xbutton.x, e.xbutton.y);
            break;
          case KeyPress:
          case KeyRelease:
            /* Key pressed/released over our window. We send upstream
               events for interactivity/navigation */
            GST_DEBUG ("xvimagesink key %d pressed over window at %d,%d",
                       e.xkey.keycode, e.xkey.x, e.xkey.y);
            keysym = XKeycodeToKeysym (xvimagesink->xcontext->disp,
                                       e.xkey.keycode, 0);
            if (keysym != NoSymbol) {
              gst_navigation_send_key_event (GST_NAVIGATION (xvimagesink),
                                             e.type == KeyPress ?
                                               "key-press" : "key-release",
                                             XKeysymToString (keysym));
            }
            else {
              gst_navigation_send_key_event (GST_NAVIGATION (xvimagesink),
                                             e.type == KeyPress ?
                                               "key-press" : "key-release",
                                             "unknown");
            }
            break;
          default:
            GST_DEBUG ("xvimagesink unhandled X event (%d)", e.type);
        }
        
      g_mutex_lock (xvimagesink->x_lock);
    }
  g_mutex_unlock (xvimagesink->x_lock);
}

/* This function generates a caps with all supported format by the first
   Xv grabable port we find. We store each one of the supported formats in a
   format list and append the format to a newly created caps that we return */
static GstCaps *
gst_xvimagesink_get_xv_support (GstXContext *xcontext)
{
  gint i, nb_adaptors;
  XvAdaptorInfo *adaptors;
  
  g_return_val_if_fail (xcontext != NULL, NULL);
  
  /* First let's check that XVideo extension is available */
  if (!XQueryExtension (xcontext->disp, "XVideo", &i, &i, &i))
    {
      GST_DEBUG ("XVideo extension is not available");
      return NULL;
    }
  
  /* Then we get adaptors list */
  if (Success != XvQueryAdaptors (xcontext->disp, xcontext->root,
                                  &nb_adaptors, &adaptors))
    {
      GST_DEBUG ("Failed getting XV adaptors list");
      return NULL;
    }
  
  xcontext->xv_port_id = 0;
  
  GST_DEBUG ("Found %d XV adaptor(s)", nb_adaptors);
    
  /* Now search for an adaptor that supports XvImageMask */
  for (i = 0; i < nb_adaptors && !xcontext->xv_port_id; i++)
    {
      if (adaptors[i].type & XvImageMask)
        {
          gint j;
          
          /* We found such an adaptor, looking for an available port */
          for (j = 0; j < adaptors[i].num_ports && !xcontext->xv_port_id; j++)
            {
              /* We try to grab the port */
              if (Success == XvGrabPort (xcontext->disp,
                                         adaptors[i].base_id + j, 0))
                {
                  xcontext->xv_port_id = adaptors[i].base_id + j;
                }
            }
        }
        
      GST_DEBUG ("XV Adaptor %s with %ld ports", adaptors[i].name,
                 adaptors[i].num_ports);
        
    }
  XvFreeAdaptorInfo (adaptors);
    
  if (xcontext->xv_port_id)
    {
      gint nb_formats;
      XvImageFormatValues *formats = NULL;
      GstCaps *caps = NULL;
      
      /* We get all image formats supported by our port */
      formats = XvListImageFormats (xcontext->disp,
                                    xcontext->xv_port_id, &nb_formats);
      caps = gst_caps_new_empty ();
      for (i = 0; i < nb_formats; i++)
        {
          GstCaps *format_caps = NULL;
          
          switch (formats[i].type)
            {
              case XvRGB:
                {
                  format_caps = gst_caps_new_simple ("video/x-raw-rgb",
                      "endianness", G_TYPE_INT, xcontext->endianness,
                      "depth", G_TYPE_INT, xcontext->depth,
                      "bpp", G_TYPE_INT, xcontext->bpp,
                      "blue_mask", G_TYPE_INT, formats[i].red_mask,
                      "green_mask", G_TYPE_INT, formats[i].green_mask,
                      "red_mask", G_TYPE_INT, formats[i].blue_mask,
                      "width", GST_TYPE_INT_RANGE, 0, G_MAXINT,
                      "height", GST_TYPE_INT_RANGE, 0, G_MAXINT,
                      "framerate", GST_TYPE_DOUBLE_RANGE, 0.0, G_MAXDOUBLE,
                      NULL);
                  
                  /* For RGB caps we store them and the image 
                     format so that we can get back the format
                     when sinkconnect will give us a caps without
                     format property */
                  if (format_caps)
                    {
                      GstXvImageFormat *format = NULL;
                      format = g_new0 (GstXvImageFormat, 1);
                      if (format)
                        {
                          format->format = formats[i].id;
                          format->caps = gst_caps_copy (format_caps);
                          xcontext->formats_list = g_list_append (
                                                xcontext->formats_list, format);
                        }
                    }
                  break;
                }
              case XvYUV:
                format_caps = gst_caps_new_simple ("video/x-raw-yuv",
                    "format", GST_TYPE_FOURCC,formats[i].id,
                    "width", GST_TYPE_INT_RANGE, 0, G_MAXINT,
                    "height", GST_TYPE_INT_RANGE, 0, G_MAXINT,
                    "framerate", GST_TYPE_DOUBLE_RANGE, 0.0, G_MAXDOUBLE,
                    NULL);
                break;
              default:
                g_assert_not_reached();
                break;
            }
          
          gst_caps_append (caps, format_caps);
        }
        
      if (formats)
        XFree (formats);
      
      GST_DEBUG_CAPS ("Generated the following caps", caps);

      return caps;
    }
    
  return NULL;
}

/* This function get the X Display and global infos about it. Everything is
   stored in our object and will be cleaned when the object is disposed. Note
   here that caps for supported format are generated without any window or 
   image creation */
static GstXContext *
gst_xvimagesink_xcontext_get (GstXvImageSink *xvimagesink)
{
  GstXContext *xcontext = NULL;
  XPixmapFormatValues *px_formats = NULL;
  gint nb_formats = 0, i, j, N_attr;
  XvAttribute *xv_attr;
  char *channels[4] = { "XV_HUE", "XV_SATURATION",
                        "XV_BRIGHTNESS", "XV_CONTRAST" };
  
  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);
  
  xcontext = g_new0 (GstXContext, 1);
  
  g_mutex_lock (xvimagesink->x_lock);
  
  xcontext->disp = XOpenDisplay (NULL);
  
  if (!xcontext->disp)
    {
      g_mutex_unlock (xvimagesink->x_lock);
      g_free (xcontext);
      return NULL;
    }
  
  xcontext->screen = DefaultScreenOfDisplay (xcontext->disp);
  xcontext->screen_num = DefaultScreen (xcontext->disp);
  xcontext->visual = DefaultVisual(xcontext->disp, xcontext->screen_num);
  xcontext->root = DefaultRootWindow (xcontext->disp);
  xcontext->white = XWhitePixel (xcontext->disp, xcontext->screen_num);
  xcontext->black = XBlackPixel (xcontext->disp, xcontext->screen_num);
  xcontext->depth = DefaultDepthOfScreen (xcontext->screen);
  
  /* We get supported pixmap formats at supported depth */
  px_formats = XListPixmapFormats (xcontext->disp, &nb_formats);
  
  if (!px_formats)
    {
      XCloseDisplay (xcontext->disp);
      g_mutex_unlock (xvimagesink->x_lock);
      g_free (xcontext);
      return NULL;
    }
  
  /* We get bpp value corresponding to our running depth */
  for (i=0; i<nb_formats; i++)
    {
      if (px_formats[i].depth == xcontext->depth)
        xcontext->bpp = px_formats[i].bits_per_pixel;
    }
    
  XFree (px_formats);
    
  xcontext->endianness = (ImageByteOrder (xcontext->disp) == LSBFirst) ? G_LITTLE_ENDIAN:G_BIG_ENDIAN;
  
#ifdef HAVE_XSHM
  /* Search for XShm extension support */
  if (XQueryExtension (xcontext->disp, "MIT-SHM", &i, &i, &i))
    {
      xcontext->use_xshm = TRUE;
      GST_DEBUG ("xvimagesink is using XShm extension");
    }
  else
    {
      xcontext->use_xshm = FALSE;
      GST_DEBUG ("xvimagesink is not using XShm extension");
    }
#endif /* HAVE_XSHM */
  
  /* our caps system handles 24/32bpp RGB as big-endian. */
  if ((xcontext->bpp == 24 || xcontext->bpp == 32) &&
      xcontext->endianness == G_LITTLE_ENDIAN) {
    xcontext->endianness = G_BIG_ENDIAN;
    xcontext->visual->red_mask = GUINT32_TO_BE (xcontext->visual->red_mask);
    xcontext->visual->green_mask = GUINT32_TO_BE (xcontext->visual->green_mask);
    xcontext->visual->blue_mask = GUINT32_TO_BE (xcontext->visual->blue_mask);
    if (xcontext->bpp == 24) {
      xcontext->visual->red_mask >>= 8;
      xcontext->visual->green_mask >>= 8;
      xcontext->visual->blue_mask >>= 8;
    }
  }
  
  xcontext->caps = gst_xvimagesink_get_xv_support (xcontext);
  
  if (!xcontext->caps)
    {
      XCloseDisplay (xcontext->disp);
      g_mutex_unlock (xvimagesink->x_lock);
      g_free (xcontext);
      return NULL;
    }

  xv_attr = XvQueryPortAttributes (xcontext->disp,
                                   xcontext->xv_port_id,
                                   &N_attr);
  
 
  /* Generate the channels list */
  for (i = 0; i < (sizeof (channels) / sizeof (char *)); i++)
    {
      GstColorBalanceChannel *channel;
      XvAttribute *matching_attr = NULL;

      if (xv_attr != NULL) 
        {
          for (j = 0; j < N_attr && matching_attr == NULL; ++j)
            if (! g_ascii_strcasecmp (channels[i], xv_attr[j].name))
              matching_attr = xv_attr + j;
        }
      
      channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
      channel->label = g_strdup (channels[i]);
      channel->min_value = matching_attr ? matching_attr->min_value : -1000;
      channel->max_value = matching_attr ? matching_attr->max_value : 1000;
      
      xcontext->channels_list = g_list_append (xcontext->channels_list,
                                               channel);
    }

  if (xv_attr)
    XFree (xv_attr);

  g_mutex_unlock (xvimagesink->x_lock);
    
  return xcontext;
}

/* This function cleans the X context. Closing the Display, releasing the XV
   port and unrefing the caps for supported formats. */
static void
gst_xvimagesink_xcontext_clear (GstXvImageSink *xvimagesink)
{
  GList *formats_list, *channels_list;
  
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  formats_list = xvimagesink->xcontext->formats_list;
  
  while (formats_list)
    {
      GstXvImageFormat *format = formats_list->data;
      gst_caps_free (format->caps);
      g_free (format);
      formats_list = g_list_next (formats_list);
    }
    
  if (xvimagesink->xcontext->formats_list)
    g_list_free (xvimagesink->xcontext->formats_list);
  
  channels_list = xvimagesink->xcontext->channels_list;
  
  while (channels_list)
    {
      GstColorBalanceChannel *channel = channels_list->data;
      g_object_unref (channel);
      channels_list = g_list_next (channels_list);
    }
    
  if (xvimagesink->xcontext->channels_list)
    g_list_free (xvimagesink->xcontext->channels_list);
  
  gst_caps_free (xvimagesink->xcontext->caps);
  
  g_mutex_lock (xvimagesink->x_lock);
  
  XvUngrabPort (xvimagesink->xcontext->disp,
                xvimagesink->xcontext->xv_port_id, 0);
  
  XCloseDisplay (xvimagesink->xcontext->disp);
  
  g_mutex_unlock (xvimagesink->x_lock);
  
  xvimagesink->xcontext = NULL;
}

/* Element stuff */

static GstCaps *
gst_xvimagesink_fixate (GstPad *pad, const GstCaps *caps)
{
  GstStructure *structure;
  GstCaps *newcaps;

  if (gst_caps_get_size (caps) > 1) return NULL;

  newcaps = gst_caps_copy (caps);
  structure = gst_caps_get_structure (newcaps, 0);

  if (gst_caps_structure_fixate_field_nearest_int (structure, "width", 320)) {
    return newcaps;
  }
  if (gst_caps_structure_fixate_field_nearest_int (structure, "height", 240)) {
    return newcaps;
  }
  if (gst_caps_structure_fixate_field_nearest_double (structure, "framerate",
                                                      30.0)) {
    return newcaps;
  }

  gst_caps_free (newcaps);
  return NULL;
}

/* This function tries to get a format matching with a given caps in the
   supported list of formats we generated in gst_xvimagesink_get_xv_support */
static gint
gst_xvimagesink_get_fourcc_from_caps (GstXvImageSink *xvimagesink,
                                      GstCaps *caps)
{
  GList *list = NULL;
  
  g_return_val_if_fail (xvimagesink != NULL, 0);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), 0);
  
  list = xvimagesink->xcontext->formats_list;
  
  while (list)
    {
      GstXvImageFormat *format = list->data;
      
      if (format)
        {
          GstCaps *icaps = NULL;
          icaps = gst_caps_intersect (caps, format->caps);
          if (!gst_caps_is_empty(icaps))
            return format->format;
        }
      list = g_list_next (list);
    }
  
  return 0;
}

static GstCaps *
gst_xvimagesink_getcaps (GstPad *pad)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (gst_pad_get_parent (pad));
  
  if (xvimagesink->xcontext)
    return gst_caps_copy (xvimagesink->xcontext->caps);

  return gst_caps_from_string(
    "video/x-raw-rgb, "
      "framerate = (double) [ 0, MAX ], "
      "width = (int) [ 0, MAX ], "
      "height = (int) [ 0, MAX ]; "
    "video/x-raw-yuv, "
      "framerate = (double) [ 0, MAX ], "
      "width = (int) [ 0, MAX ], "
      "height = (int) [ 0, MAX ]");
}

static GstPadLinkReturn
gst_xvimagesink_sinkconnect (GstPad *pad, const GstCaps *caps)
{
  GstXvImageSink *xvimagesink;
  char *caps_str1, *caps_str2;
  GstStructure *structure;
  gboolean ret;

  xvimagesink = GST_XVIMAGESINK (gst_pad_get_parent (pad));

  caps_str1 = gst_caps_to_string (xvimagesink->xcontext->caps);
  caps_str2 = gst_caps_to_string (caps);

  GST_DEBUG ("sinkconnect %s with %s", caps_str1, caps_str2);

  g_free (caps_str1);
  g_free (caps_str2);
  
  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "width",
                               &(GST_VIDEOSINK_WIDTH (xvimagesink)));
  ret &= gst_structure_get_int (structure, "height",
                                &(GST_VIDEOSINK_HEIGHT (xvimagesink)));
  ret &= gst_structure_get_double (structure, "framerate",
                                   &xvimagesink->framerate);
  if (!ret) return GST_PAD_LINK_REFUSED;
  
  xvimagesink->xcontext->im_format = 0;
  if (!gst_structure_get_fourcc (structure, "format",
                                 &xvimagesink->xcontext->im_format)) {
    xvimagesink->xcontext->im_format = gst_xvimagesink_get_fourcc_from_caps (
                                         xvimagesink, gst_caps_copy(caps));
  }
  if (xvimagesink->xcontext->im_format == 0) {
    return GST_PAD_LINK_REFUSED;
  }
  
  xvimagesink->pixel_width = 1;
  gst_structure_get_int (structure, "pixel_width", &xvimagesink->pixel_width);

  xvimagesink->pixel_height = 1;
  gst_structure_get_int  (structure, "pixel_height",
                          &xvimagesink->pixel_height);
  
  /* Creating our window and our image */
  if (!xvimagesink->xwindow)
    xvimagesink->xwindow = gst_xvimagesink_xwindow_new (xvimagesink,
                                            GST_VIDEOSINK_WIDTH (xvimagesink),
                                            GST_VIDEOSINK_HEIGHT (xvimagesink));
  else
    {
      if (xvimagesink->xwindow->internal)
        gst_xvimagesink_xwindow_resize (xvimagesink, xvimagesink->xwindow,
                                        GST_VIDEOSINK_WIDTH (xvimagesink),
                                        GST_VIDEOSINK_HEIGHT (xvimagesink));
    }
  
  if ( (xvimagesink->xvimage) &&
       ( (GST_VIDEOSINK_WIDTH (xvimagesink) != xvimagesink->xvimage->width) ||
         (GST_VIDEOSINK_HEIGHT (xvimagesink) != xvimagesink->xvimage->height) ) )
    { /* We renew our xvimage only if size changed */
      gst_xvimagesink_xvimage_destroy (xvimagesink, xvimagesink->xvimage);
  
      xvimagesink->xvimage = gst_xvimagesink_xvimage_new (xvimagesink,
                                            GST_VIDEOSINK_WIDTH (xvimagesink),
                                            GST_VIDEOSINK_HEIGHT (xvimagesink));
    }
  else if (!xvimagesink->xvimage) /* If no xvimage, creating one */
    xvimagesink->xvimage = gst_xvimagesink_xvimage_new (xvimagesink,
                                            GST_VIDEOSINK_WIDTH (xvimagesink),
                                            GST_VIDEOSINK_HEIGHT (xvimagesink));
  
  gst_x_overlay_got_desired_size (GST_X_OVERLAY (xvimagesink),
                                  GST_VIDEOSINK_WIDTH (xvimagesink),
                                  GST_VIDEOSINK_HEIGHT (xvimagesink));
  
  return GST_PAD_LINK_OK;
}

static GstElementStateReturn
gst_xvimagesink_change_state (GstElement *element)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      /* Initializing the XContext */
      if (!xvimagesink->xcontext)
        xvimagesink->xcontext = gst_xvimagesink_xcontext_get (xvimagesink);
      if (!xvimagesink->xcontext)
        return GST_STATE_FAILURE;
      break;
    case GST_STATE_READY_TO_PAUSED:
      xvimagesink->time = 0;
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      xvimagesink->framerate = 0;
      GST_VIDEOSINK_WIDTH (xvimagesink) = 0;
      GST_VIDEOSINK_HEIGHT (xvimagesink) = 0;
      break;
    case GST_STATE_READY_TO_NULL:
      break;
  }

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

static void
gst_xvimagesink_chain (GstPad *pad, GstData *data)
{
  GstBuffer *buf = NULL;
  GstXvImageSink *xvimagesink;

  g_return_if_fail (pad != NULL);
  g_return_if_fail (GST_IS_PAD (pad));
  g_return_if_fail (data != NULL);

  xvimagesink = GST_XVIMAGESINK (gst_pad_get_parent (pad));
    
  if (GST_IS_EVENT (data))
    {
      GstEvent *event = GST_EVENT (data);
      gint64 offset;

      switch (GST_EVENT_TYPE (event))
        {
          case GST_EVENT_DISCONTINUOUS:
            offset = GST_EVENT_DISCONT_OFFSET (event, 0).value;
            GST_DEBUG ("xvimage discont %" G_GINT64_FORMAT "\n", offset);
            break;
          default:
            gst_pad_event_default (pad, event);
            return;
        }
      gst_event_unref (event);
      return;
    }
  
  buf = GST_BUFFER (data);
  /* update time */
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    xvimagesink->time = GST_BUFFER_TIMESTAMP (buf);
  }
  GST_DEBUG ("videosink: clock wait: %" G_GUINT64_FORMAT, xvimagesink->time);
  
  if (GST_VIDEOSINK_CLOCK (xvimagesink)) {
    GstClockID id;
    id = gst_clock_new_single_shot_id (GST_VIDEOSINK_CLOCK (xvimagesink),
                                       xvimagesink->time);
    gst_element_clock_wait (GST_ELEMENT (xvimagesink), id, NULL);
    gst_clock_id_free (id);
  }
  
  /* If this buffer has been allocated using our buffer management we simply
     put the ximage which is in the PRIVATE pointer */
  if (GST_BUFFER_PRIVATE (buf))
    {
      gst_xvimagesink_xvimage_put (xvimagesink, GST_BUFFER_PRIVATE (buf));
    }
  else /* Else we have to copy the data into our private image, */
    {  /* if we have one... */
      if (xvimagesink->xvimage)
        {
          memcpy (xvimagesink->xvimage->xvimage->data, 
                  GST_BUFFER_DATA (buf), 
                  MIN (GST_BUFFER_SIZE (buf), xvimagesink->xvimage->size));
          gst_xvimagesink_xvimage_put (xvimagesink, xvimagesink->xvimage);
        }
      else /* No image available. Something went wrong during capsnego ! */
        {
          gst_buffer_unref (buf);
          gst_element_error (GST_ELEMENT (xvimagesink), "no image to draw");
          return;
        }
    }

  /* set correct time for next buffer */
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buf) && xvimagesink->framerate > 0) {
    xvimagesink->time += GST_SECOND / xvimagesink->framerate;
  }

  gst_buffer_unref (buf);
    
  gst_xvimagesink_handle_xevents (xvimagesink, pad);
}

/* Buffer management */

static void
gst_xvimagesink_buffer_free (GstBuffer *buffer)
{
  GstXvImageSink *xvimagesink;
  GstXvImage *xvimage;
  
  xvimage = GST_BUFFER_PRIVATE (buffer);
  
  g_assert (GST_IS_XVIMAGESINK (xvimage->xvimagesink));
  xvimagesink = xvimage->xvimagesink;
  
  /* If our geometry changed we can't reuse that image. */
  if ( (xvimage->width != GST_VIDEOSINK_WIDTH (xvimagesink)) ||
       (xvimage->height != GST_VIDEOSINK_HEIGHT (xvimagesink)) )
    gst_xvimagesink_xvimage_destroy (xvimagesink, xvimage);
  else /* In that case we can reuse the image and add it to our image pool. */
    {
      g_mutex_lock (xvimagesink->pool_lock);
      xvimagesink->image_pool = g_slist_prepend (xvimagesink->image_pool,
                                                 xvimage);
      g_mutex_unlock (xvimagesink->pool_lock);
    }
}

static GstBuffer *
gst_xvimagesink_buffer_alloc (GstPad *pad, guint64 offset, guint size)
{
  GstXvImageSink *xvimagesink;
  GstBuffer *buffer;
  GstXvImage *xvimage = NULL;
  gboolean not_found = TRUE;
  
  xvimagesink = GST_XVIMAGESINK (gst_pad_get_parent (pad));

  g_mutex_lock (xvimagesink->pool_lock);
  
  /* Walking through the pool cleaning unsuable images and searching for a
     suitable one */
  while (not_found && xvimagesink->image_pool)
    {
      xvimage = xvimagesink->image_pool->data;
      
      if (xvimage)
        {
          /* Removing from the pool */
          xvimagesink->image_pool = g_slist_delete_link (xvimagesink->image_pool,
                                                         xvimagesink->image_pool);
          
          if ( (xvimage->width != GST_VIDEOSINK_WIDTH (xvimagesink)) ||
               (xvimage->height != GST_VIDEOSINK_HEIGHT (xvimagesink)) )
            { /* This image is unusable. Destroying... */
              gst_xvimagesink_xvimage_destroy (xvimagesink, xvimage);
              xvimage = NULL;
            }
          else /* We found a suitable image */
            {
              break;
            }
        }
    }
   
  g_mutex_unlock (xvimagesink->pool_lock);
  
  if (!xvimage) /* We found no suitable image in the pool. Creating... */
    {
      xvimage = gst_xvimagesink_xvimage_new (xvimagesink,
                                             GST_VIDEOSINK_WIDTH (xvimagesink),
                                             GST_VIDEOSINK_HEIGHT (xvimagesink));
    }
  
  if (xvimage)
    {
      buffer = gst_buffer_new ();
      
      /* Storing some pointers in the buffer */
      GST_BUFFER_PRIVATE (buffer) = xvimage;
      
      GST_BUFFER_DATA (buffer) = xvimage->xvimage->data;
      GST_BUFFER_FREE_DATA_FUNC (buffer) = gst_xvimagesink_buffer_free;
      GST_BUFFER_SIZE (buffer) = xvimage->size;
      return buffer;
    }
  else
    return NULL;
}

static void
gst_xvimagesink_imagepool_clear (GstXvImageSink *xvimagesink)
{
  g_mutex_lock(xvimagesink->pool_lock);
  
  while (xvimagesink->image_pool)
    {
      GstXvImage *xvimage = xvimagesink->image_pool->data;
      xvimagesink->image_pool = g_slist_delete_link (xvimagesink->image_pool,
                                                     xvimagesink->image_pool);
      gst_xvimagesink_xvimage_destroy (xvimagesink, xvimage);
    }
  
  g_mutex_unlock(xvimagesink->pool_lock);
}

/* Interfaces stuff */

static gboolean
gst_xvimagesink_interface_supported (GstImplementsInterface *iface, GType type)
{
  g_assert (type == GST_TYPE_NAVIGATION ||
            type == GST_TYPE_X_OVERLAY ||
            type == GST_TYPE_COLOR_BALANCE);
  return TRUE;
}

static void
gst_xvimagesink_interface_init (GstImplementsInterfaceClass *klass)
{
  klass->supported = gst_xvimagesink_interface_supported;
}

static void
gst_xvimagesink_navigation_send_event (GstNavigation *navigation,
                                       GstStructure *structure)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (navigation);
  GstEvent *event;
  double x,y;

  event = gst_event_new (GST_EVENT_NAVIGATION);
  event->event_data.structure.structure = structure;
  
  /* Converting pointer coordinates to the non scaled geometry */
  if (gst_structure_get_double (structure, "pointer_x", &x))
    {
      x *= GST_VIDEOSINK_WIDTH (xvimagesink);
      x /= xvimagesink->xwindow->width;
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE, x, NULL);
    }
  if (gst_structure_get_double (structure, "pointer_y", &y))
    {
      y *= GST_VIDEOSINK_HEIGHT (xvimagesink);
      y /= xvimagesink->xwindow->height;
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE, y, NULL);
    }

  gst_pad_send_event (gst_pad_get_peer (GST_VIDEOSINK_PAD (xvimagesink)),
                      event);
}

static void
gst_xvimagesink_navigation_init (GstNavigationInterface *iface)
{
  iface->send_event = gst_xvimagesink_navigation_send_event;
}

static void
gst_xvimagesink_set_xwindow_id (GstXOverlay *overlay, XID xwindow_id)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);
  GstXWindow *xwindow = NULL;
  XWindowAttributes attr;
  
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  
  /* If we already use that window return */
  if (xvimagesink->xwindow && (xwindow_id == xvimagesink->xwindow->win))
    return;
  
  /* If the element has not initialized the X11 context try to do so */
  if (!xvimagesink->xcontext)
    xvimagesink->xcontext = gst_xvimagesink_xcontext_get (xvimagesink);
  
  if (!xvimagesink->xcontext)
    {
      g_warning ("xvimagesink was unable to obtain the X11 context.");
      return;
    }
    
  /* Clear image pool as the images are unusable anyway */
  gst_xvimagesink_imagepool_clear (xvimagesink);
  
  /* Clear the xvimage */
  if (xvimagesink->xvimage)
    {
      gst_xvimagesink_xvimage_destroy (xvimagesink, xvimagesink->xvimage);
      xvimagesink->xvimage = NULL;
    }
  
  /* If a window is there already we destroy it */
  if (xvimagesink->xwindow)
    {
      gst_xvimagesink_xwindow_destroy (xvimagesink, xvimagesink->xwindow);
      xvimagesink->xwindow = NULL;
    }
    
  /* If the xid is 0 we go back to an internal window */
  if (xwindow_id == 0)
    {
      /* If no width/height caps nego did not happen window will be created
         during caps nego then */
      if (GST_VIDEOSINK_WIDTH (xvimagesink) &&
          GST_VIDEOSINK_HEIGHT (xvimagesink))
        {
          xwindow = gst_xvimagesink_xwindow_new (xvimagesink,
                                            GST_VIDEOSINK_WIDTH (xvimagesink),
                                            GST_VIDEOSINK_HEIGHT (xvimagesink));
        }
    }
  else
    {
      xwindow = g_new0 (GstXWindow, 1);

      xwindow->win = xwindow_id;
  
      /* We get window geometry, set the event we want to receive,
         and create a GC */
      g_mutex_lock (xvimagesink->x_lock);
      XGetWindowAttributes (xvimagesink->xcontext->disp, xwindow->win, &attr);
      xwindow->width = attr.width;
      xwindow->height = attr.height;
      xwindow->internal = FALSE;
      XSelectInput (xvimagesink->xcontext->disp, xwindow->win, ExposureMask |
                    StructureNotifyMask | PointerMotionMask | KeyPressMask |
                    KeyReleaseMask);
  
      xwindow->gc = XCreateGC (xvimagesink->xcontext->disp,
                               xwindow->win, 0, NULL);
      g_mutex_unlock (xvimagesink->x_lock);
    }
  
  /* Recreating our xvimage */
  if (!xvimagesink->xvimage &&
      GST_VIDEOSINK_WIDTH (xvimagesink) &&
      GST_VIDEOSINK_HEIGHT (xvimagesink))
    {
      xvimagesink->xvimage = gst_xvimagesink_xvimage_new (
                                            xvimagesink,
                                            GST_VIDEOSINK_WIDTH (xvimagesink),
                                            GST_VIDEOSINK_HEIGHT (xvimagesink));
    }
    
  if (xwindow)
    xvimagesink->xwindow = xwindow;
}

static void
gst_xvimagesink_get_desired_size (GstXOverlay *overlay,
                                  guint *width, guint *height)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (overlay);

  *width = GST_VIDEOSINK_WIDTH (xvimagesink);
  *height = GST_VIDEOSINK_HEIGHT (xvimagesink);
}

static void
gst_xvimagesink_xoverlay_init (GstXOverlayClass *iface)
{
  iface->set_xwindow_id = gst_xvimagesink_set_xwindow_id;
  iface->get_desired_size = gst_xvimagesink_get_desired_size;
}

static const GList *
gst_xvimagesink_colorbalance_list_channels (GstColorBalance *balance)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);
  
  g_return_val_if_fail (xvimagesink != NULL, NULL);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), NULL);
  
  if (xvimagesink->xcontext)
    return xvimagesink->xcontext->channels_list;
  else
    return NULL;
}

static void
gst_xvimagesink_colorbalance_set_value (GstColorBalance        *balance,
                                        GstColorBalanceChannel *channel,
                                        gint                    value)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);
  
  g_return_if_fail (xvimagesink != NULL);
  g_return_if_fail (GST_IS_XVIMAGESINK (xvimagesink));
  g_return_if_fail (channel->label != NULL);
  
  g_mutex_lock (xvimagesink->x_lock);
  
  XvSetPortAttribute (xvimagesink->xcontext->disp,
                      xvimagesink->xcontext->xv_port_id,
                      XInternAtom (xvimagesink->xcontext->disp,
                                   channel->label, 1), value);
  
  g_mutex_unlock (xvimagesink->x_lock);
}

static gint
gst_xvimagesink_colorbalance_get_value (GstColorBalance        *balance,
                                        GstColorBalanceChannel *channel)
{
  GstXvImageSink *xvimagesink = GST_XVIMAGESINK (balance);
  gint value;
  
  g_return_val_if_fail (xvimagesink != NULL, 0);
  g_return_val_if_fail (GST_IS_XVIMAGESINK (xvimagesink), 0);
  g_return_val_if_fail (channel->label != NULL, 0);
  
  g_mutex_lock (xvimagesink->x_lock);
  
  XvGetPortAttribute (xvimagesink->xcontext->disp,
                      xvimagesink->xcontext->xv_port_id,
                      XInternAtom (xvimagesink->xcontext->disp,
                                   channel->label, 1), &value);
  
  g_mutex_unlock (xvimagesink->x_lock);
  
  return value;
}

static void
gst_xvimagesink_colorbalance_init (GstColorBalanceClass *iface)
{
  iface->balance_type = GST_COLOR_BALANCE_HARDWARE;
  iface->list_channels = gst_xvimagesink_colorbalance_list_channels;
  iface->set_value = gst_xvimagesink_colorbalance_set_value;
  iface->get_value = gst_xvimagesink_colorbalance_get_value;
}

/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_xvimagesink_dispose (GObject *object)
{
  GstXvImageSink *xvimagesink;

  xvimagesink = GST_XVIMAGESINK (object);

  if (xvimagesink->xvimage)
    {
      gst_xvimagesink_xvimage_destroy (xvimagesink, xvimagesink->xvimage);
      xvimagesink->xvimage = NULL;
    }
    
  if (xvimagesink->image_pool)
    gst_xvimagesink_imagepool_clear (xvimagesink);
  
  if (xvimagesink->xwindow)
    {
      gst_xvimagesink_xwindow_destroy (xvimagesink, xvimagesink->xwindow);
      xvimagesink->xwindow = NULL;
    }
  
  if (xvimagesink->xcontext)
    gst_xvimagesink_xcontext_clear (xvimagesink);
    
  g_mutex_free (xvimagesink->x_lock);
  g_mutex_free (xvimagesink->pool_lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_xvimagesink_init (GstXvImageSink *xvimagesink)
{
  GST_VIDEOSINK_PAD (xvimagesink) = gst_pad_new_from_template (
      gst_static_pad_template_get (&gst_xvimagesink_sink_template_factory),
      "sink");
  
  gst_element_add_pad (GST_ELEMENT (xvimagesink),
                       GST_VIDEOSINK_PAD (xvimagesink));

  gst_pad_set_chain_function (GST_VIDEOSINK_PAD (xvimagesink),
                              gst_xvimagesink_chain);
  gst_pad_set_link_function (GST_VIDEOSINK_PAD (xvimagesink),
                             gst_xvimagesink_sinkconnect);
  gst_pad_set_getcaps_function (GST_VIDEOSINK_PAD (xvimagesink),
                                gst_xvimagesink_getcaps);
  gst_pad_set_fixate_function (GST_VIDEOSINK_PAD (xvimagesink),
                                gst_xvimagesink_fixate);
  gst_pad_set_bufferalloc_function (GST_VIDEOSINK_PAD (xvimagesink),
                                    gst_xvimagesink_buffer_alloc);
  
  xvimagesink->xcontext = NULL;
  xvimagesink->xwindow = NULL;
  xvimagesink->xvimage = NULL;
  
  xvimagesink->framerate = 0;
  
  xvimagesink->x_lock = g_mutex_new ();
  
  xvimagesink->pixel_width = xvimagesink->pixel_height = 1;
  
  xvimagesink->image_pool = NULL;
  xvimagesink->pool_lock = g_mutex_new ();

  GST_FLAG_SET(xvimagesink, GST_ELEMENT_THREAD_SUGGESTED);
  GST_FLAG_SET(xvimagesink, GST_ELEMENT_EVENT_AWARE);
}

static void
gst_xvimagesink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details (element_class, &gst_xvimagesink_details);

  gst_element_class_add_pad_template (element_class, 
    gst_static_pad_template_get (&gst_xvimagesink_sink_template_factory));
}

static void
gst_xvimagesink_class_init (GstXvImageSinkClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_VIDEOSINK);

  gobject_class->dispose = gst_xvimagesink_dispose;
  
  gstelement_class->change_state = gst_xvimagesink_change_state;
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*          Object typing & Creation           */
/*                                             */
/* =========================================== */

GType
gst_xvimagesink_get_type (void)
{
  static GType xvimagesink_type = 0;

  if (!xvimagesink_type)
    {
      static const GTypeInfo xvimagesink_info = {
        sizeof(GstXvImageSinkClass),
        gst_xvimagesink_base_init,
        NULL,
        (GClassInitFunc) gst_xvimagesink_class_init,
        NULL,
        NULL,
        sizeof(GstXvImageSink),
        0,
        (GInstanceInitFunc) gst_xvimagesink_init,
      };
      static const GInterfaceInfo iface_info = {
        (GInterfaceInitFunc) gst_xvimagesink_interface_init,
        NULL,
        NULL,
      };
      static const GInterfaceInfo navigation_info = {
        (GInterfaceInitFunc) gst_xvimagesink_navigation_init,
        NULL,
        NULL,
      };
      static const GInterfaceInfo overlay_info = {
        (GInterfaceInitFunc) gst_xvimagesink_xoverlay_init,
        NULL,
        NULL,
      };
      static const GInterfaceInfo colorbalance_info = {
        (GInterfaceInitFunc) gst_xvimagesink_colorbalance_init,
        NULL,
        NULL,
      };
      
      xvimagesink_type = g_type_register_static (GST_TYPE_VIDEOSINK,
                                                 "GstXvImageSink",
                                                 &xvimagesink_info, 0);
      
      g_type_add_interface_static (xvimagesink_type, GST_TYPE_IMPLEMENTS_INTERFACE,
                                   &iface_info);
      g_type_add_interface_static (xvimagesink_type, GST_TYPE_NAVIGATION,
                                   &navigation_info);
      g_type_add_interface_static (xvimagesink_type, GST_TYPE_X_OVERLAY,
                                   &overlay_info);
      g_type_add_interface_static (xvimagesink_type, GST_TYPE_COLOR_BALANCE,
                                   &colorbalance_info);
    }
    
  return xvimagesink_type;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  /* Loading the library containing GstVideoSink, our parent object */
  if (!gst_library_load ("gstvideo"))
    return FALSE;
  
  if (!gst_element_register (plugin, "xvimagesink",
                             GST_RANK_PRIMARY, GST_TYPE_XVIMAGESINK))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "xvimagesink",
  "XFree86 video output plugin using Xv extension",
  plugin_init,
  VERSION,
  GST_LICENSE,
  GST_PACKAGE,
  GST_ORIGIN)
