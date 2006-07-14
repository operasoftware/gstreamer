/* GStreamer
 *
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gst/check/gstcheck.h>

gboolean have_eos = FALSE;

GstPad *mysinkpad;

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

gboolean
event_func (GstPad * pad, GstEvent * event)
{
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    have_eos = TRUE;
    gst_event_unref (event);
    return TRUE;
  }

  gst_event_unref (event);
  return FALSE;
}

GstElement *
setup_filesrc ()
{
  GstElement *filesrc;

  GST_DEBUG ("setup_filesrc");
  filesrc = gst_check_setup_element ("filesrc");
  mysinkpad = gst_check_setup_sink_pad (filesrc, &sinktemplate, NULL);
  gst_pad_set_event_function (mysinkpad, event_func);
  gst_pad_set_active (mysinkpad, TRUE);
  return filesrc;
}

void
cleanup_filesrc (GstElement * filesrc)
{
  gst_check_teardown_sink_pad (filesrc);
  gst_check_teardown_element (filesrc);
}

GST_START_TEST (test_seeking)
{
  GstElement *src;
  GstQuery *seeking_query;
  gboolean seekable;

#ifndef TESTFILE
#error TESTFILE not defined
#endif
  src = setup_filesrc ();

  g_object_set (G_OBJECT (src), "location", TESTFILE, NULL);
  fail_unless (gst_element_set_state (src,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_SUCCESS,
      "could not set to paused");

  /* Test that filesrc is seekable with a file fd */
  fail_unless ((seeking_query = gst_query_new_seeking (GST_FORMAT_BYTES))
      != NULL);
  fail_unless (gst_element_query (src, seeking_query) == TRUE);
  gst_query_parse_seeking (seeking_query, NULL, &seekable, NULL, NULL);
  fail_unless (seekable == TRUE);
  gst_query_unref (seeking_query);

  fail_unless (gst_element_set_state (src,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* cleanup */
  cleanup_filesrc (src);
}

GST_END_TEST;

GST_START_TEST (test_pull)
{
  GstElement *src;
  GstQuery *seeking_query;
  gboolean res, seekable;
  gint64 start, stop;
  GstPad *pad;
  GstFlowReturn ret;
  GstBuffer *buffer1, *buffer2;

  src = setup_filesrc ();

  g_object_set (G_OBJECT (src), "location", TESTFILE, NULL);
  fail_unless (gst_element_set_state (src,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");

  /* get the source pad */
  pad = gst_element_get_pad (src, "src");
  fail_unless (pad != NULL);

  /* activate the pad in pull mode */
  res = gst_pad_activate_pull (pad, TRUE);
  fail_unless (res == TRUE);

  /* not start playing */
  fail_unless (gst_element_set_state (src,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to paused");

  /* Test that filesrc is seekable with a file fd */
  fail_unless ((seeking_query = gst_query_new_seeking (GST_FORMAT_BYTES))
      != NULL);
  fail_unless (gst_element_query (src, seeking_query) == TRUE);

  /* get the seeking capabilities */
  gst_query_parse_seeking (seeking_query, NULL, &seekable, &start, &stop);
  fail_unless (seekable == TRUE);
  fail_unless (start == 0);
  fail_unless (start != -1);
  gst_query_unref (seeking_query);

  /* do some pulls */
  ret = gst_pad_get_range (pad, 0, 100, &buffer1);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer1 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer1) == 100);

  ret = gst_pad_get_range (pad, 0, 50, &buffer2);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer2 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer2) == 50);

  /* this should be the same */
  fail_unless (memcmp (GST_BUFFER_DATA (buffer1), GST_BUFFER_DATA (buffer2),
          50) == 0);

  gst_buffer_unref (buffer2);

  /* read next 50 bytes */
  ret = gst_pad_get_range (pad, 50, 50, &buffer2);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer2 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer2) == 50);

  /* compare with previously read data */
  fail_unless (memcmp (GST_BUFFER_DATA (buffer1) + 50,
          GST_BUFFER_DATA (buffer2), 50) == 0);

  gst_buffer_unref (buffer1);
  gst_buffer_unref (buffer2);

  /* read 10 bytes at end-10 should give exactly 10 bytes */
  ret = gst_pad_get_range (pad, stop - 10, 10, &buffer1);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer1 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer1) == 10);
  gst_buffer_unref (buffer1);

  /* read 20 bytes at end-10 should give exactly 10 bytes */
  ret = gst_pad_get_range (pad, stop - 10, 20, &buffer1);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer1 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer1) == 10);
  gst_buffer_unref (buffer1);

  /* read 0 bytes at end-1 should return 0 bytes */
  ret = gst_pad_get_range (pad, stop - 1, 0, &buffer1);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer1 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer1) == 0);
  gst_buffer_unref (buffer1);

  /* read 10 bytes at end-1 should return 1 byte */
  ret = gst_pad_get_range (pad, stop - 1, 10, &buffer1);
  fail_unless (ret == GST_FLOW_OK);
  fail_unless (buffer1 != NULL);
  fail_unless (GST_BUFFER_SIZE (buffer1) == 1);
  gst_buffer_unref (buffer1);

  /* read 0 bytes at end should EOS */
  ret = gst_pad_get_range (pad, stop, 0, &buffer1);
  fail_unless (ret == GST_FLOW_UNEXPECTED);

  /* read 10 bytes before end should EOS */
  ret = gst_pad_get_range (pad, stop, 10, &buffer1);
  fail_unless (ret == GST_FLOW_UNEXPECTED);

  /* read 0 bytes after end should EOS */
  ret = gst_pad_get_range (pad, stop + 10, 0, &buffer1);
  fail_unless (ret == GST_FLOW_UNEXPECTED);

  /* read 10 bytes after end should EOS too */
  ret = gst_pad_get_range (pad, stop + 10, 10, &buffer1);
  fail_unless (ret == GST_FLOW_UNEXPECTED);

  fail_unless (gst_element_set_state (src,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  /* cleanup */
  gst_object_unref (pad);
  cleanup_filesrc (src);
}

GST_END_TEST;

GST_START_TEST (test_coverage)
{
  GstElement *src;
  gchar *location;
  GstBus *bus;
  GstMessage *message;

  src = setup_filesrc ();
  bus = gst_bus_new ();

  gst_element_set_bus (src, bus);

  g_object_set (G_OBJECT (src), "location", "/i/do/not/exist", NULL);
  g_object_get (G_OBJECT (src), "location", &location, NULL);
  fail_unless_equals_string (location, "/i/do/not/exist");
  g_free (location);
  fail_unless (gst_element_set_state (src,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE,
      "could set to playing with wrong location");

  /* a state change and an error */
  fail_if ((message = gst_bus_pop (bus)) == NULL);
  gst_message_unref (message);
  fail_if ((message = gst_bus_pop (bus)) == NULL);
  fail_unless_message_error (message, RESOURCE, NOT_FOUND);
  gst_message_unref (message);

  g_object_set (G_OBJECT (src), "location", NULL, NULL);
  g_object_get (G_OBJECT (src), "location", &location, NULL);
  fail_if (location);

  /* cleanup */
  gst_element_set_bus (src, NULL);
  gst_object_unref (GST_OBJECT (bus));
  cleanup_filesrc (src);
}

GST_END_TEST;

Suite *
filesrc_suite (void)
{
  Suite *s = suite_create ("filesrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_seeking);
  tcase_add_test (tc_chain, test_pull);
  tcase_add_test (tc_chain, test_coverage);

  return s;
}

GST_CHECK_MAIN (filesrc);
