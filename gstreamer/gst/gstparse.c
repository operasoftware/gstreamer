/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * :
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

#define DEBUG(format,args...)
#define DEBUG_NOPREFIX(format,args...)
#define VERBOSE(format,args...)

#include <string.h>

#include "gst_private.h"
#include "gstparse.h"
#include "gstpipeline.h"
#include "gstthread.h"
#include "gstutils.h"

typedef struct _gst_parse_priv gst_parse_priv;
struct _gst_parse_priv {
  guint bincount;
  guint threadcount;
  gint binlevel;
  GHashTable *elementcounts;
  gboolean verbose;
  gboolean debug;
};

typedef struct _gst_parse_delayed_pad gst_parse_delayed_pad;
struct _gst_parse_delayed_pad {
  gchar *name;
  GstPad *peer;
};

/* FIXME need to either revive this, or have pad->padtemplate connections in core
static void
gst_parse_newpad(GstElement *element,GstPad *pad,launch_delayed_pad *peer)
{
  gst_info("have NEW_PAD signal\n");
  // if it matches, connect it
  if (!strcmp(GST_PAD_NAME(pad),peer->name)) {
    gst_pad_connect(pad,peer->peer);
    gst_info("delayed connect of '%s' to '%s'\n",
             GST_PAD_NAME(pad),GST_PAD_NAME(peer->peer));
  }
}
*/

static gchar *
gst_parse_unique_name(gchar *type,gst_parse_priv *priv)
{
  gint count;

  count = GPOINTER_TO_INT(g_hash_table_lookup(priv->elementcounts,type));
  count++;
  g_hash_table_insert(priv->elementcounts,type,GINT_TO_POINTER(count));

  return g_strdup_printf("%s%d",type,count-1);
}



static gint
gst_parse_launch_cmdline(int argc,char *argv[],GstBin *parent,gst_parse_priv *priv)
{
  gint i = 0;
  gchar *arg;
  GstElement *element = NULL, *previous = NULL, *prevelement = NULL;
  gchar closingchar = '\0';
  gint len;
  gchar *ptr;
  gchar *sinkpadname = NULL, *srcpadname = NULL;
  GstPad *sinkpad = NULL, *srcpad = NULL;
  GList *pads;
  gint elementcount = 0;
  gint retval = 0;

  priv->binlevel++;

  if (GST_IS_PIPELINE(parent)) { closingchar = '\0';DEBUG("in pipeline "); }
  else if (GST_IS_THREAD(parent)) { closingchar = '}';DEBUG("in thread "); }
  else { closingchar = ')';DEBUG("in bin "); }
  DEBUG_NOPREFIX("%s\n",GST_ELEMENT_NAME (GST_ELEMENT (parent)));

  while (i < argc) {
    arg = argv[i];
    // FIXME this is a lame solution for problems with the first parser
    if (arg == NULL) { i++;continue; }
    len = strlen(arg);
    element = NULL;
    DEBUG("** ARGUMENT is '%s'\n",arg);

    // a null that slipped through the reconstruction
    if (len == 0) {
      DEBUG("random arg, FIXME\n");
      i++;
      continue;

    // end of the container
    } else if (arg[0] == closingchar) {
      // time to finish off this bin
      DEBUG("exiting container %s\n",GST_ELEMENT_NAME (GST_ELEMENT (parent)));
      retval = i+1;
      break;

    // a pad connection
    } else if ((ptr = strchr(arg,'!'))) {
      DEBUG("attempting to connect pads together....\n");

      // if it starts with the !
      if (arg[0] == '!') {
        srcpadname = NULL;
        // if there's a sinkpad...
        if (len > 1)
          sinkpadname = &arg[1];
        else
          sinkpadname = NULL;
      } else {
        srcpadname = g_strndup(arg,(ptr-arg));
        // if there's a sinkpad
        if (len > (ptr-arg)+1)
          sinkpadname = &ptr[1];
        else
          sinkpadname = NULL;
      }

      GST_DEBUG(0,"have srcpad %s, sinkpad %s\n",srcpadname,sinkpadname);

      srcpad = NULL;

      // if the srcpadname doesn't have any commas in it, find an actual pad
      if (!srcpadname || !strchr(srcpadname,',')) {
        if (srcpadname != NULL) {
          srcpad = gst_element_get_pad(previous,srcpadname);
          if (!srcpad)
            GST_DEBUG(0,"NO SUCH pad %s in element %s\n",srcpadname,GST_ELEMENT_NAME(previous));
        }

        if (srcpad == NULL) {
          // check through the list to find the first sink pad
          GST_DEBUG(0,"CHECKING through element %s for pad named %s\n",GST_ELEMENT_NAME(previous),srcpadname);
          pads = gst_element_get_pad_list(previous);
          while (pads) {
            srcpad = GST_PAD(pads->data);
GST_DEBUG(0,"have pad %s:%s\n",GST_DEBUG_PAD_NAME(srcpad));
if (GST_IS_GHOST_PAD(srcpad)) GST_DEBUG(0,"it's a ghost pad\n");
            pads = g_list_next (pads);
            if (gst_pad_get_direction (srcpad) == GST_PAD_SRC) break;
            srcpad = NULL;
          }
        }

        if (!srcpad) GST_DEBUG(0,"error, can't find a src pad!!!\n");
        else GST_DEBUG(0,"have src pad %s:%s\n",GST_DEBUG_PAD_NAME(srcpad));
      }

    // argument with = in it
    } else if (strstr(arg, "=")) {
      gchar * argname;
      gchar * argval;
      gchar * pos = strstr(arg, "=");
      // we have an argument
      argname = arg;
      pos[0] = '\0';
      argval = pos+1;
      DEBUG("attempting to set argument '%s' to '%s' on element '%s'\n",
            argname,argval,GST_ELEMENT_NAME(previous));
      //gtk_object_set(GTK_OBJECT(previous),argname,argval,NULL);
      gst_util_set_object_arg (GTK_OBJECT(previous), argname, argval);
      g_free(argname);

    // element or argument, or beginning of bin or thread
    } else {
      DEBUG("have element or bin/thread\n");
      // if we have a bin or thread starting
      if (strchr("({",arg[0])) {
        if (arg[0] == '(') {
          // create a bin and add it to the current parent
          element = gst_bin_new(g_strdup_printf("bin%d",priv->bincount++));
          if (!element) {
            fprintf(stderr,"Couldn't create a bin!\n");
//            exit(-1);
          }
          GST_DEBUG(0,"CREATED bin %s\n",GST_ELEMENT_NAME(element));
        } else if (arg[0] == '{') {
          // create a thread and add it to the current parent
          element = gst_thread_new(g_strdup_printf("thread%d",priv->threadcount++));
          if (!element) {
            fprintf(stderr,"Couldn't create a thread!\n");
//            exit(-1);
          }
          GST_DEBUG(0,"CREATED thread %s\n",GST_ELEMENT_NAME(element));
        }

        i += gst_parse_launch_cmdline(argc - i, argv + i + 1, GST_BIN (element), priv);

      } else {
        // we have an element
        DEBUG("attempting to create element '%s'\n",arg);
        ptr = gst_parse_unique_name(arg,priv);
        element = gst_elementfactory_make(arg,ptr);
        g_free(ptr);
        if (!element) {
          fprintf(stderr,"Couldn't create a '%s', no such element or need to run gstraemer-register?\n",arg);
//          exit(-1);
        }
        GST_DEBUG(0,"CREATED element %s\n",GST_ELEMENT_NAME(element));
      }

      gst_bin_add (GST_BIN (parent), element);
      elementcount++;

      if (srcpad != NULL) {
        DEBUG("need to connect to sinkpad %s:%s\n",GST_DEBUG_PAD_NAME(srcpad));

        sinkpad = NULL;

        if (sinkpadname != NULL)
          sinkpad = gst_element_get_pad(previous,sinkpadname);

        if (!sinkpad) {
          // check through the list to find the first sink pad
          pads = gst_element_get_pad_list(element);
          while (pads) {
            sinkpad = GST_PAD(pads->data);
            pads = g_list_next (pads);
            if (gst_pad_get_direction (sinkpad) == GST_PAD_SINK) break;
            sinkpad = NULL;
          }
        }

        if (!sinkpad) DEBUG("error, can't find a sink pad!!!\n");
        else DEBUG("have sink pad %s:%s\n",GST_DEBUG_PAD_NAME(sinkpad));

        GST_DEBUG(0,"CONNECTING %s:%s and %s:%s\n",GST_DEBUG_PAD_NAME(srcpad),GST_DEBUG_PAD_NAME(sinkpad));
        gst_pad_connect(srcpad,sinkpad);

        sinkpad = NULL;
        srcpad = NULL;
      }

      // if we're the first element, ghost all the sinkpads
      if (elementcount == 1) {
        DEBUG("first element, ghosting all of %s's sink pads to parent %s\n",
              GST_ELEMENT_NAME(element),GST_ELEMENT_NAME(GST_ELEMENT(parent)));
        pads = gst_element_get_pad_list (element);
        while (pads) {
          sinkpad = GST_PAD (pads->data);
          pads = g_list_next (pads);
          if (!sinkpad) DEBUG("much oddness, pad doesn't seem to exist\n");
          else if (gst_pad_get_direction (sinkpad) == GST_PAD_SINK) {
            gst_element_add_ghost_pad (GST_ELEMENT (parent), sinkpad,
g_strdup_printf("%s-ghost",GST_PAD_NAME(sinkpad)));
            GST_DEBUG(0,"GHOSTED %s:%s to %s as %s-ghost\n",
                      GST_DEBUG_PAD_NAME(sinkpad),GST_ELEMENT_NAME(GST_ELEMENT(parent)),GST_PAD_NAME(sinkpad));
          }
        }
      }

      previous = element;
      if (!GST_IS_BIN(element)) prevelement = element;
    }

    i++;
  }

  // ghost all the src pads of the bin
  if (prevelement != NULL) {
    DEBUG("last element, ghosting all of %s's src pads to parent %s\n",
          GST_ELEMENT_NAME(prevelement),GST_ELEMENT_NAME(GST_ELEMENT(parent)));
    pads = gst_element_get_pad_list (prevelement);
    while (pads) {
      srcpad = GST_PAD (pads->data);
      pads = g_list_next (pads);
      if (!srcpad) DEBUG("much oddness, pad doesn't seem to exist\n");
      else if (gst_pad_get_direction (srcpad) == GST_PAD_SRC) {
        gst_element_add_ghost_pad (GST_ELEMENT (parent), srcpad,
g_strdup_printf("%s-ghost",GST_PAD_NAME(srcpad)));
        GST_DEBUG(0,"GHOSTED %s:%s to %s as %s-ghost\n",
GST_DEBUG_PAD_NAME(srcpad),GST_ELEMENT_NAME (parent),GST_PAD_NAME(srcpad));
      }
    }
  }

  priv->binlevel--;

  if (retval) return retval;

  if (closingchar != '\0')
    DEBUG("returning IN THE WRONG PLACE\n");
  else DEBUG("ending pipeline\n");
  return i+1;
}

/**
 * gst_parse_launch:
 * @cmdline: the command line describing the pipeline
 * @parent: the parent bin for the resulting pipeline
 *
 * Create a new pipeline based on command line syntax.
 *
 * Returns: ?
 */
gint
gst_parse_launch(const gchar *cmdline,GstBin *parent)
{
  gst_parse_priv priv;
  gchar **argvn;
  gint newargc;
  gint len;
  int i,j,k;

  priv.bincount = 0;
  priv.threadcount = 0;
  priv.binlevel = 0;
  priv.elementcounts = NULL;
  priv.verbose = FALSE;
  priv.debug = FALSE;

  // first walk through quickly and see how many more slots we need
  len = strlen(cmdline);
  newargc = 1;
  for (i=0;i<len;i++) {
    // if it's a space, it denotes a new arg
    if (cmdline[i] == ' ') newargc++;
    // if it's a brace and isn't followed by a space, give it an arg
    if (strchr("([{}])",cmdline[i])) {
      // not followed by space, gets one
      if (cmdline[i+1] != ' ') newargc++;
    }
  }

  // now allocate the new argv array
  argvn = g_new0(char *,newargc+1);
  GST_DEBUG(0,"supposed to have %d args\n",newargc);

  // now attempt to construct the new arg list
  j = 0;k = 0;
  for (i=0;i<len+1;i++) {
    // if it's a delimiter
    if (strchr("([{}]) ",cmdline[i]) || (cmdline[i] == '\0')) {
      // extract the previous arg
      if (i-k > 0) {
        if (cmdline[k] == ' ') k++;
        argvn[j] = g_new0(char,(i-k)+1);
        memcpy(argvn[j],&cmdline[k],i-k);

        // catch misparses
        if (strlen(argvn[j]) > 0) j++;
      }
      k = i;

      // if this is a bracket, construct a word
      if ((cmdline[i] != ' ') && (cmdline[i] != '\0')) {
        argvn[j++] = g_strdup_printf("%c",cmdline[i]);
        k++;
      }
    }
  }

  // print them out
  for (i=0;i<newargc;i++) {
    GST_DEBUG(0,"arg %d is: %s\n",i,argvn[i]);
  }

  // set up the elementcounts hash
  priv.elementcounts = g_hash_table_new(g_str_hash,g_str_equal);

  return gst_parse_launch_cmdline(newargc,argvn,parent,&priv);
}
