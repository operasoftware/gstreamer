/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"../config.h"

#include        <glib-object.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/stat.h>
#include	<fcntl.h>

static gchar *indent_inc = NULL;
static guint spacing = 1;
static FILE *f_out = NULL;
static GType root = 0;
static gboolean recursion = TRUE;

#if 0
#  define	O_SPACE	"\\as"
#  define	O_ESPACE " "
#  define	O_BRANCH "\\aE"
#  define	O_VLINE "\\al"
#  define	O_LLEAF	"\\aL"
#  define	O_KEY_FILL "_"
#else
#  define	O_SPACE	" "
#  define	O_ESPACE ""
#  define	O_BRANCH "+"
#  define	O_VLINE "|"
#  define	O_LLEAF	"`"
#  define	O_KEY_FILL "_"
#endif

static void
show_nodes (GType        type,
	    GType        sibling,
	    const gchar *indent)
{
  GType   *children;
  guint i;
  
  if (!type)
    return;
  
  children = g_type_children (type, NULL);
  
  if (type != root)
    for (i = 0; i < spacing; i++)
      fprintf (f_out, "%s%s\n", indent, O_VLINE);
  
  fprintf (f_out, "%s%s%s%s",
	   indent,
	   sibling ? O_BRANCH : (type != root ? O_LLEAF : O_SPACE),
	   O_ESPACE,
	   g_type_name (type));
  
  for (i = strlen (g_type_name (type)); i <= strlen (indent_inc); i++)
    fputs (O_KEY_FILL, f_out);
  
  fputc ('\n', f_out);
  
  if (children && recursion)
    {
      gchar *new_indent;
      GType   *child;
      
      if (sibling)
	new_indent = g_strconcat (indent, O_VLINE, indent_inc, NULL);
      else
	new_indent = g_strconcat (indent, O_SPACE, indent_inc, NULL);
      
      for (child = children; *child; child++)
	show_nodes (child[0], child[1], new_indent);
      
      g_free (new_indent);
    }
  
  g_free (children);
}

static gint
help (gchar *arg)
{
  fprintf (stderr, "usage: query <qualifier> [-r <type>] [-{i|b} \"\"] [-s #] [-{h|x|y}]\n");
  fprintf (stderr, "       -r       specifiy root type\n");
  fprintf (stderr, "       -n       don't descend type tree\n");
  fprintf (stderr, "       -h       guess what ;)\n");
  fprintf (stderr, "       -b       specifiy indent string\n");
  fprintf (stderr, "       -i       specifiy incremental indent string\n");
  fprintf (stderr, "       -s       specifiy line spacing\n");
  fprintf (stderr, "qualifiers:\n");
  fprintf (stderr, "       froots   iterate over fundamental roots\n");
  fprintf (stderr, "       tree     print BSE type tree\n");
  
  return arg != NULL;
}

int
main (gint   argc,
      gchar *argv[])
{
  gboolean gen_froots = 0;
  gboolean gen_tree = 0;
  guint i;
  gchar *iindent = "";
  
  f_out = stdout;
  
  g_type_init (0);
  
  root = G_TYPE_OBJECT;
  
  for (i = 1; i < argc; i++)
    {
      if (strcmp ("-s", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    spacing = atoi (argv[i]);
	}
      else if (strcmp ("-i", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    {
	      char *p;
	      guint n;
	      
	      p = argv[i];
	      while (*p)
		p++;
	      n = p - argv[i];
	      indent_inc = g_new (gchar, n * strlen (O_SPACE) + 1);
	      *indent_inc = 0;
	      while (n)
		{
		  n--;
		  strcpy (indent_inc, O_SPACE);
		}
	    }
	}
      else if (strcmp ("-b", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    iindent = argv[i];
	}
      else if (strcmp ("-r", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    root = g_type_from_name (argv[i]);
	}
      else if (strcmp ("-n", argv[i]) == 0)
	{
	  recursion = FALSE;
	}
      else if (strcmp ("froots", argv[i]) == 0)
	{
	  gen_froots = 1;
	}
      else if (strcmp ("tree", argv[i]) == 0)
	{
	  gen_tree = 1;
	}
      else if (strcmp ("-h", argv[i]) == 0)
	{
	  return help (NULL);
	}
      else if (strcmp ("--help", argv[i]) == 0)
	{
	  return help (NULL);
	}
      else
	return help (argv[i]);
    }
  if (!gen_froots && !gen_tree)
    return help (argv[i-1]);
  
  if (!indent_inc)
    {
      indent_inc = g_new (gchar, strlen (O_SPACE) + 1);
      *indent_inc = 0;
      strcpy (indent_inc, O_SPACE);
      strcpy (indent_inc, O_SPACE);
      strcpy (indent_inc, O_SPACE);
    }
  
  if (gen_tree)
    show_nodes (root, 0, iindent);
  if (gen_froots)
    {
      root = ~0;
      for (i = 0; i < 256; i++)
	{
	  const gchar *name = g_type_name (i);
	  
	  if (name)
	    show_nodes (i, 0, iindent);
	}
    }
  
  return 0;
}
