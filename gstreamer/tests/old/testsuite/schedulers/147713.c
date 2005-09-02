/* GStreamer
 * Copyright (C) 2004 Wim Taymanse <wim@fluendo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gst/gst.h>

static gint src_handoff = 0;

static void
handoff_src (GstElement * element)
{
  g_print ("src handoff\n");
  src_handoff++;
}

static void
handoff_sink (GstElement * element)
{
  g_print ("sink handoff\n");
  g_assert (src_handoff == 1);
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline, *src, *sink, *id1, *id2;

  gst_init (&argc, &argv);

  g_print ("setting up...\n");
  /* setup pipeline */
  pipeline = gst_element_factory_make ("pipeline", NULL);
  g_assert (pipeline);
  src = gst_element_factory_make ("fakesrc", NULL);
  g_assert (src);
  g_object_set (G_OBJECT (src), "signal-handoffs", TRUE, NULL);
  g_signal_connect (G_OBJECT (src), "handoff", (GCallback) handoff_src, NULL);
  id1 = gst_element_factory_make ("identity", NULL);
  g_assert (id1);

  id2 = gst_element_factory_make ("identity", NULL);
  g_assert (id2);
  g_object_set (G_OBJECT (id2), "loop-based", TRUE, NULL);

  sink = gst_element_factory_make ("fakesink", NULL);
  g_assert (sink);
  g_object_set (G_OBJECT (sink), "signal-handoffs", TRUE, NULL);
  g_signal_connect (G_OBJECT (sink), "handoff", (GCallback) handoff_sink, NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, id1, NULL);
  gst_element_link_pads (src, "src", id1, "sink");

  if (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_SUCCESS)
    g_assert_not_reached ();

  if (gst_element_set_state (id2,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_SUCCESS)
    g_assert_not_reached ();
  if (gst_element_set_state (sink,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_SUCCESS)
    g_assert_not_reached ();

  gst_bin_add_many (GST_BIN (pipeline), sink, NULL);
  gst_element_link_pads (id2, "src", sink, "sink");
  gst_element_link_pads (id1, "src", id2, "sink");
  gst_bin_add_many (GST_BIN (pipeline), id2, NULL);

  gst_bin_iterate (GST_BIN (pipeline));
  gst_bin_iterate (GST_BIN (pipeline));

  g_print ("cleaning up...\n");
  gst_object_unref (pipeline);
  src = id1 = id2 = sink = pipeline = NULL;

  g_print ("done.\n");
  return 0;
}
