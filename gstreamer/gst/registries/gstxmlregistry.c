/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstxml_registry.c: GstXMLRegistry object, support routines
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

/* #define DEBUG_ENABLED */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <gst/gst_private.h>
#include <gst/gstelement.h>
#include <gst/gsttype.h>
#include <gst/gstscheduler.h>
#include <gst/gstautoplug.h>

#include "gstxmlregistry.h"

#define BLOCK_SIZE 1024*10

#define CLASS(registry)  GST_XML_REGISTRY_CLASS (G_OBJECT_GET_CLASS (registry))


static void 		gst_xml_registry_class_init 		(GstXMLRegistryClass *klass);
static void 		gst_xml_registry_init 			(GstXMLRegistry *registry);

static gboolean		gst_xml_registry_load 			(GstRegistry *registry);
static gboolean		gst_xml_registry_save 			(GstRegistry *registry);
static gboolean 	gst_xml_registry_rebuild 		(GstRegistry *registry);

static gboolean 	gst_xml_registry_open_func 		(GstXMLRegistry *registry, GstXMLRegistryMode mode);
static gboolean 	gst_xml_registry_load_func 		(GstXMLRegistry *registry, gchar *data, gssize *size);
static gboolean 	gst_xml_registry_save_func 		(GstXMLRegistry *registry, gchar *format, ...);
static gboolean 	gst_xml_registry_close_func 		(GstXMLRegistry *registry);


static GstRegistryReturn gst_xml_registry_load_plugin		(GstRegistry *registry, GstPlugin *plugin);

static void 		gst_xml_registry_start_element  	(GMarkupParseContext *context,
		                           			 const gchar         *element_name,
					                         const gchar        **attribute_names,
							         const gchar        **attribute_values,
							         gpointer             user_data,
							         GError             **error);
static void 		gst_xml_registry_end_element    	(GMarkupParseContext *context,
		                           			 const gchar         *element_name,
					                         gpointer             user_data,
							         GError             **error);
static void 		gst_xml_registry_text           	(GMarkupParseContext *context,
		                           			 const gchar         *text,
					                         gsize                text_len,  
							         gpointer             user_data,
							         GError             **error);
static void 		gst_xml_registry_passthrough    	(GMarkupParseContext *context,
		                           			 const gchar         *passthrough_text,
					                         gsize                text_len,  
							         gpointer             user_data,
							         GError             **error);
static void 		gst_xml_registry_error          	(GMarkupParseContext *context,
		                           			 GError              *error,
					                         gpointer             user_data);

static GstRegistryClass *parent_class = NULL;
/* static guint gst_xml_registry_signals[LAST_SIGNAL] = { 0 }; */

static const GMarkupParser 
gst_xml_registry_parser = 
{
  gst_xml_registry_start_element,
  gst_xml_registry_end_element,
  gst_xml_registry_text,
  gst_xml_registry_passthrough,
  gst_xml_registry_error,
};


GType 
gst_xml_registry_get_type (void) 
{
  static GType xml_registry_type = 0;

  if (!xml_registry_type) {
    static const GTypeInfo xml_registry_info = {
      sizeof (GstXMLRegistryClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_xml_registry_class_init,
      NULL,
      NULL,
      sizeof(GstXMLRegistry),
      0,
      (GInstanceInitFunc) gst_xml_registry_init,
      NULL
    };
    xml_registry_type = g_type_register_static (GST_TYPE_REGISTRY, 
		    				"GstXMLRegistry", &xml_registry_info, 0);
  }
  return xml_registry_type;
}

static void
gst_xml_registry_class_init (GstXMLRegistryClass *klass)
{
  GObjectClass *gobject_class;
  GstRegistryClass *gstregistry_class;
  GstXMLRegistryClass *gstxmlregistry_class;

  gobject_class = (GObjectClass*)klass;
  gstregistry_class = (GstRegistryClass*)klass;
  gstxmlregistry_class = (GstXMLRegistryClass*)klass;

  parent_class = g_type_class_ref (GST_TYPE_REGISTRY);

  gstregistry_class->load  	  = GST_DEBUG_FUNCPTR (gst_xml_registry_load);
  gstregistry_class->save  	  = GST_DEBUG_FUNCPTR (gst_xml_registry_save);
  gstregistry_class->rebuild  	  = GST_DEBUG_FUNCPTR (gst_xml_registry_rebuild);
  
  gstregistry_class->load_plugin  = GST_DEBUG_FUNCPTR (gst_xml_registry_load_plugin);

  gstxmlregistry_class->open_func 	= GST_DEBUG_FUNCPTR (gst_xml_registry_open_func);
  gstxmlregistry_class->load_func 	= GST_DEBUG_FUNCPTR (gst_xml_registry_load_func);
  gstxmlregistry_class->save_func 	= GST_DEBUG_FUNCPTR (gst_xml_registry_save_func);
  gstxmlregistry_class->close_func 	= GST_DEBUG_FUNCPTR (gst_xml_registry_close_func);
}

static void
gst_xml_registry_init (GstXMLRegistry *registry)
{
  registry->location = NULL;
  registry->context = NULL;
  registry->state = GST_XML_REGISTRY_NONE;
  registry->current_plugin = NULL;
  registry->current_feature = NULL;
  registry->open_tags = NULL;
}

/**
 * gst_xml_registry_new:
 * @name: the name of the registry
 * @location: the location of the registry file
 *
 * Create a new xml registry with the given name and location.
 *
 * Returns: a new GstXMLRegistry with the given name an location.
 */
GstRegistry*
gst_xml_registry_new (const gchar *name, const gchar *location) 
{
  GstXMLRegistry *xmlregistry;
  
  xmlregistry = GST_XML_REGISTRY (g_object_new (GST_TYPE_XML_REGISTRY, NULL));
  
  xmlregistry->location = g_strdup (location);

  GST_REGISTRY (xmlregistry)->name = g_strdup (name);
  GST_REGISTRY (xmlregistry)->flags = GST_REGISTRY_READABLE | GST_REGISTRY_WRITABLE;

  return GST_REGISTRY (xmlregistry);
}

/* same as 0755 */
#define dirmode \
  (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

static time_t
get_time(const char * path)
{
  struct stat statbuf;
  if (stat(path, &statbuf)) return 0;
  if (statbuf.st_mtime > statbuf.st_ctime) return statbuf.st_mtime;
  return statbuf.st_ctime;
}

static gboolean
plugin_times_older_than_recurse(gchar *path, time_t regtime)
{
  DIR *dir;
  struct dirent *dirent;
  gchar *pluginname;

  time_t pathtime = get_time(path);

  if (pathtime > regtime) {
    GST_INFO (GST_CAT_PLUGIN_LOADING,
                     "time for %s was %ld; more recent than registry time of %ld\n",
                     path, (long)pathtime, (long)regtime);
    return FALSE;
  }

  dir = opendir(path);
  if (dir) {
    while ((dirent = readdir(dir))) {
      /* don't want to recurse in place or backwards */
      if (strcmp(dirent->d_name,".") && strcmp(dirent->d_name,"..")) {
        pluginname = g_strjoin("/",path,dirent->d_name,NULL);
        if (!plugin_times_older_than_recurse(pluginname , regtime)) {
          g_free (pluginname);
          closedir(dir);
          return FALSE;
        }
        g_free (pluginname);
      }
    }
    closedir(dir);
  }
  return TRUE;
}

static gboolean
plugin_times_older_than(GList *paths, time_t regtime)
{
  /* return true iff regtime is more recent than the times of all the files
   * in the plugin dirs.
   */

  while (paths) {
    GST_DEBUG (GST_CAT_PLUGIN_LOADING,
                      "comparing plugin times from %s with %ld\n",
                      (gchar *)paths->data, (long) regtime);
    if(!plugin_times_older_than_recurse(paths->data, regtime))
      return FALSE;
    paths = g_list_next(paths);
  }
  return TRUE;
}

static gboolean
gst_xml_registry_open_func (GstXMLRegistry *registry, GstXMLRegistryMode mode)
{
  GList *paths = GST_REGISTRY (registry)->paths;

  if (mode == GST_XML_REGISTRY_READ) {
    if (!plugin_times_older_than (paths, get_time (registry->location))) {
      GST_INFO (GST_CAT_GST_INIT, "Registry out of date, rebuilding...");
      
      gst_registry_rebuild (GST_REGISTRY (registry));
      gst_registry_save (GST_REGISTRY (registry));

      if (!plugin_times_older_than (paths, get_time (registry->location))) {
        GST_INFO (GST_CAT_GST_INIT, "Registry still out of date, something is wrong...");
        return FALSE;
      }
    }
    
    registry->regfile = fopen (registry->location, "r");
  }
  else if (mode == GST_XML_REGISTRY_WRITE)
  {
    /* check the dir */
    struct stat filestat;
    char *dirname = g_strndup(registry->location,
                              strrchr(registry->location, '/') - registry->location);
    if (stat(dirname, &filestat) == -1 && errno == ENOENT)
      if (mkdir(dirname, dirmode) != 0)
        return FALSE;
    g_free(dirname);

    registry->regfile = fopen (registry->location, "w");
  }

  if (!registry->regfile)
    return FALSE;

  return TRUE;
}

static gboolean
gst_xml_registry_load_func (GstXMLRegistry *registry, gchar *data, gssize *size)
{
  *size = fread (data, 1, *size, registry->regfile);
  
  return TRUE;
}

static gboolean
gst_xml_registry_save_func (GstXMLRegistry *registry, gchar *format, ...)
{
  va_list var_args;

  va_start (var_args, format);

  vfprintf (registry->regfile, format, var_args);

  va_end (var_args);

  return TRUE;
}

static gboolean
gst_xml_registry_close_func (GstXMLRegistry *registry)
{
  fclose (registry->regfile);

  return TRUE;
}

static gboolean
gst_xml_registry_load (GstRegistry *registry)
{
  GstXMLRegistry *xmlregistry = GST_XML_REGISTRY (registry);
  gchar *text;
  gssize size;
  GError *error = NULL;
  GTimer *timer;
  gdouble seconds;

  timer = g_timer_new();
  
  xmlregistry->context = g_markup_parse_context_new (&gst_xml_registry_parser, 0, registry, NULL);

  if (!CLASS (xmlregistry)->open_func (xmlregistry, GST_XML_REGISTRY_READ)) {
    return FALSE;
  }

  text = g_malloc (BLOCK_SIZE);

  size = BLOCK_SIZE;
  CLASS (xmlregistry)->load_func (xmlregistry, text, &size);

  while (size) {
    g_markup_parse_context_parse (xmlregistry->context, text, size, &error);

    size = BLOCK_SIZE;
    CLASS (xmlregistry)->load_func (xmlregistry, text, &size);
  }

  g_free (text);

  if (error) {
    fprintf(stderr, "ERROR: parsing registry: %s\n", error->message);
    return FALSE;
  }

  g_timer_stop (timer);

  seconds = g_timer_elapsed (timer, NULL);

  g_print ("loaded registry %s in %f seconds\n", registry->name, seconds);

  CLASS (xmlregistry)->close_func (xmlregistry);

  
  return TRUE;
}

static GstRegistryReturn 
gst_xml_registry_load_plugin (GstRegistry *registry, GstPlugin *plugin)
{
  if (!gst_plugin_load_plugin (plugin)) {
    return GST_REGISTRY_PLUGIN_LOAD_ERROR;
  }

  return GST_REGISTRY_OK;
}

static gboolean
gst_xml_registry_parse_plugin (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                               gsize text_len, GstXMLRegistry *registry, GError **error)
{
  GstPlugin *plugin = registry->current_plugin;
  
  if (!strcmp (tag, "name")) {
    plugin->name = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "longname")) {
    plugin->longname = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "filename")) {
    plugin->filename = g_strndup (text, text_len);
  }
  
  return TRUE;
}

static gboolean
gst_xml_registry_parse_element_factory (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                        gsize text_len, GstXMLRegistry *registry, GError **error)
{
  GstElementFactory *factory = GST_ELEMENT_FACTORY (registry->current_feature);

  if (!strcmp (tag, "name")) {
    registry->current_feature->name = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "longname")) {
    factory->details->longname = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "class")) {
    factory->details->klass = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "description")) {
    factory->details->description = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "version")) {
    factory->details->version = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "author")) {
    factory->details->author = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "copyright")) {
    factory->details->copyright = g_strndup (text, text_len);
  }

  return TRUE;
}

static GstCaps*
gst_type_type_find_dummy (GstBuffer *buffer, gpointer priv)
{
  GstTypeFactory *factory = (GstTypeFactory *)priv;

  GST_DEBUG (GST_CAT_TYPES,"gsttype: need to load typefind function for %s", factory->mime);

  if (gst_plugin_feature_ensure_loaded (GST_PLUGIN_FEATURE (factory))) {
    if (factory->typefindfunc) {
      GstCaps *res = factory->typefindfunc (buffer, factory);
      if (res)
        return res;
    }
  }
  return NULL;
}


static gboolean
gst_xml_registry_parse_type_factory (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                     gsize text_len, GstXMLRegistry *registry, GError **error)
{
  GstTypeFactory *factory = GST_TYPE_FACTORY (registry->current_feature);

  if (!strcmp (tag, "name")) {
    registry->current_feature->name = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "mime")) {
    factory->mime = g_strndup (text, text_len);
  }
  else if (!strcmp(tag, "extensions")) {
    factory->exts = g_strndup (text, text_len);
  }

  return TRUE;
}

static gboolean
gst_xml_registry_parse_scheduler_factory (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                          gsize text_len, GstXMLRegistry *registry, GError **error)
{
  GstSchedulerFactory *factory = GST_SCHEDULER_FACTORY (registry->current_feature);

  if (!strcmp (tag, "name")) {
    registry->current_feature->name = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "longdesc")) {
    factory->longdesc = g_strndup (text, text_len);
  }
  return TRUE;
}

static gboolean
gst_xml_registry_parse_autoplug_factory (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                         gsize text_len, GstXMLRegistry *registry, GError **error)
{
  //GstAutoplugFactory *factory = GST_AUTOPLUG_FACTORY (registry->current_feature);

  if (!strcmp (tag, "name")) {
    registry->current_feature->name = g_strndup (text, text_len);
  }
  return TRUE;
}

static gboolean
gst_xml_registry_parse_padtemplate (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                    gsize text_len, GstXMLRegistry *registry, GError **error)
{
  if (!strcmp (tag, "nametemplate")) {
    registry->name_template = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "direction")) {
    if (!strncmp(text, "sink", text_len)) {
      registry->direction = GST_PAD_SINK;
    }
    else if (!strncmp(text, "src", text_len)) {
      registry->direction = GST_PAD_SRC;
    }
  }
  else if (!strcmp (tag, "presence")) {
    if (!strncmp(text, "always", text_len)) {
      registry->presence = GST_PAD_ALWAYS;
    }
    else if (!strncmp(text, "sometimes", text_len)) {
      registry->presence = GST_PAD_SOMETIMES;
    }
    else if (!strncmp(text, "request", text_len)) {
      registry->presence = GST_PAD_REQUEST;
    }
  }
  return TRUE;
}

static gboolean
gst_xml_registry_parse_capscomp (GMarkupParseContext *context, const gchar *tag, const gchar *text,
                                 gsize text_len, GstXMLRegistry *registry, GError **error)
{
  if (!strcmp (tag, "name")) {
    registry->caps_name = g_strndup (text, text_len);
  }
  else if (!strcmp (tag, "type")) {
    registry->caps_mime = g_strndup (text, text_len);
  }
  return TRUE;
}

static gint
find_index_for (const gchar *name, const gchar **attribute_names)
{
  gint i=0;
  
  while (attribute_names[i]) {
    if (!strcmp (attribute_names[i], name))
      return i;
    i++;
  }
  return -1;
}

static void
gst_xml_registry_start_element (GMarkupParseContext *context, const gchar *element_name,
	                       const gchar **attribute_names, const gchar **attribute_values,
			       gpointer user_data, GError **error)
{
  GstXMLRegistry *xmlregistry = GST_XML_REGISTRY (user_data);

  xmlregistry->open_tags = g_list_prepend (xmlregistry->open_tags, g_strdup (element_name));

  switch (xmlregistry->state) {
    case GST_XML_REGISTRY_NONE:
      if (!strcmp (element_name, "GST-PluginRegistry")) {
	xmlregistry->state = GST_XML_REGISTRY_TOP;
      }
      break;
    case GST_XML_REGISTRY_TOP:
      if (!strncmp (element_name, "plugin", 6)) {
	xmlregistry->state = GST_XML_REGISTRY_PLUGIN;
	xmlregistry->parser = gst_xml_registry_parse_plugin;
	xmlregistry->current_plugin = (GstPlugin *)g_new0 (GstPlugin, 1);
      }
      break;
    case GST_XML_REGISTRY_PLUGIN:
      if (!strncmp (element_name, "feature", 7)) {
	gint i = 0;
	GstPluginFeature *feature = NULL;
	
	xmlregistry->state = GST_XML_REGISTRY_FEATURE;

	while (attribute_names[i]) {
          if (!strncmp (attribute_names[i], "typename", 8)) {
	    feature = GST_PLUGIN_FEATURE (g_object_new (g_type_from_name (attribute_values[i]), NULL));
	    break;
	  }
          i++;
	}
	if (feature) {
	  xmlregistry->current_feature = feature;
	  if (GST_IS_ELEMENT_FACTORY (feature)) {
	    GstElementFactory *factory = GST_ELEMENT_FACTORY (feature);

	    factory->details_dynamic = TRUE;
	    factory->details = g_new0(GstElementDetails, 1);
	    factory->padtemplates = NULL;
	    xmlregistry->parser = gst_xml_registry_parse_element_factory;
	  }
	  else if (GST_IS_TYPE_FACTORY (feature))
	    xmlregistry->parser = gst_xml_registry_parse_type_factory;
	  else if (GST_IS_SCHEDULER_FACTORY (feature)) {
	    xmlregistry->parser = gst_xml_registry_parse_scheduler_factory;
            GST_SCHEDULER_FACTORY (feature)->type = 0;
	  }
	  else if (GST_IS_AUTOPLUG_FACTORY (feature))
	    xmlregistry->parser = gst_xml_registry_parse_autoplug_factory;
	}
      }
      break;
    case GST_XML_REGISTRY_FEATURE:
      if (!strncmp (element_name, "padtemplate", 11)) {
	xmlregistry->state = GST_XML_REGISTRY_PADTEMPLATE;
	xmlregistry->parser = gst_xml_registry_parse_padtemplate;
	xmlregistry->name_template = NULL;
	xmlregistry->direction = 0;
	xmlregistry->presence = 0;
	xmlregistry->caps = NULL;
      }
      break;
    case GST_XML_REGISTRY_PADTEMPLATE:
      if (!strncmp (element_name, "caps", 4)) {
	xmlregistry->state = GST_XML_REGISTRY_CAPS;
	xmlregistry->parser = NULL;
      }
      break;
    case GST_XML_REGISTRY_CAPS:
      if (!strncmp (element_name, "capscomp", 8)) {
	xmlregistry->state = GST_XML_REGISTRY_CAPSCOMP;
	xmlregistry->parser = gst_xml_registry_parse_capscomp;
      }
      break;
    case GST_XML_REGISTRY_CAPSCOMP:
      if (!strncmp (element_name, "properties", 10)) {
	xmlregistry->state = GST_XML_REGISTRY_PROPERTIES;
	xmlregistry->parser = NULL;
	xmlregistry->props = gst_props_empty_new ();
      }
      break;
    case GST_XML_REGISTRY_PROPERTIES:
    {
      gint name_index;
      
      name_index = find_index_for ("name", attribute_names);
      if (name_index < 0)
	break;

      if (!strncmp (element_name, "int", 3)) {
	gint value;
        gint index;

	if ((index = find_index_for ("value", attribute_names)) < 0) 
          break;
	sscanf (attribute_values[index], "%d", &value);
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_INT (value)));
      }
      else if (!strncmp (element_name, "range", 5)) {
	gint min, max;
        gint min_idx, max_idx;

	if ((min_idx = find_index_for ("min", attribute_names)) < 0) 
          break;
	if ((max_idx = find_index_for ("max", attribute_names)) < 0) 
          break;
	sscanf (attribute_values[min_idx], "%d", &min);
	sscanf (attribute_values[max_idx], "%d", &max);
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_INT_RANGE (min, max)));
      }
      else if (!strncmp (element_name, "float", 5)) {
	gfloat value;
        gint index;

	if ((index = find_index_for ("value", attribute_names)) < 0) 
          break;
	sscanf (attribute_values[index], "%f", &value);
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_FLOAT (value)));
      }
      else if (!strncmp (element_name, "floatrange", 10)) {
	gfloat min, max;
        gint min_idx, max_idx;

	if ((min_idx = find_index_for ("min", attribute_names)) < 0) 
          break;
	if ((max_idx = find_index_for ("max", attribute_names)) < 0) 
          break;
	sscanf (attribute_values[min_idx], "%f", &min);
	sscanf (attribute_values[max_idx], "%f", &max);
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_FLOAT_RANGE (min, max)));
      }
      else if (!strncmp (element_name, "boolean", 7)) {
	gboolean value = TRUE;
        gint index;

	if ((index = find_index_for ("value", attribute_names)) < 0) 
          break;
	if (!strcmp (attribute_values[index], "false")) 
          value = FALSE;
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_BOOLEAN (value)));
      }
      else if (!strncmp (element_name, "fourcc", 6)) {
	guint32 value;
        gint index;

	if ((index = find_index_for ("hexvalue", attribute_names)) < 0) 
          break;
	sscanf (attribute_values[index], "%08x", &value);
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], GST_PROPS_FOURCC (value)));
      }
      else if (!strncmp (element_name, "string", 6)) {
        gint index;

	if ((index = find_index_for ("value", attribute_names)) < 0) 
          break;
	gst_props_add_entry (xmlregistry->props, 
			gst_props_entry_new (attribute_values[name_index], 
					      GST_PROPS_STRING (attribute_values[index])));
      }
      break;
    }
    default:
      break;
  }
}

static void
gst_xml_registry_end_element (GMarkupParseContext *context, const gchar *element_name,
			      gpointer user_data, GError **error)
{
  GstXMLRegistry *xmlregistry = GST_XML_REGISTRY (user_data);
  gchar *open_tag = (gchar *)xmlregistry->open_tags->data;

  xmlregistry->open_tags = g_list_remove (xmlregistry->open_tags, open_tag);
  g_free (open_tag);

  switch (xmlregistry->state) {
    case GST_XML_REGISTRY_TOP:
      if (!strcmp (element_name, "GST-PluginRegistry")) {
	xmlregistry->state = GST_XML_REGISTRY_NONE;
      }
      break;
    case GST_XML_REGISTRY_PLUGIN:
      if (!strcmp (element_name, "plugin")) {
	xmlregistry->state = GST_XML_REGISTRY_TOP;
	xmlregistry->parser = NULL;
	gst_registry_add_plugin (GST_REGISTRY (xmlregistry), xmlregistry->current_plugin);
      }
      break;
    case GST_XML_REGISTRY_FEATURE:
      if (!strcmp (element_name, "feature")) {
	if (GST_IS_TYPE_FACTORY (xmlregistry->current_feature)) {
          GstTypeFactory *factory = GST_TYPE_FACTORY (xmlregistry->current_feature);
	  gst_type_register (factory);
	}
	xmlregistry->state = GST_XML_REGISTRY_PLUGIN;
	xmlregistry->parser = gst_xml_registry_parse_plugin;
	gst_plugin_add_feature (xmlregistry->current_plugin, xmlregistry->current_feature);
	xmlregistry->current_feature = NULL;
      }
      else if (!strcmp (element_name, "typefind")) {
        GstTypeFactory *factory = GST_TYPE_FACTORY (xmlregistry->current_feature);

        factory->typefindfunc = gst_type_type_find_dummy;
      }
      break;
    case GST_XML_REGISTRY_PADTEMPLATE:
      if (!strcmp (element_name, "padtemplate")) {
	GstPadTemplate *template;

        template = gst_pad_template_new (xmlregistry->name_template,
					 xmlregistry->direction,
					 xmlregistry->presence,
					 xmlregistry->caps, NULL);

	g_free (xmlregistry->name_template);
	xmlregistry->name_template = NULL;
	xmlregistry->caps = NULL;

	gst_element_factory_add_pad_template (GST_ELEMENT_FACTORY (xmlregistry->current_feature),
					      template);
	xmlregistry->state = GST_XML_REGISTRY_FEATURE;
	xmlregistry->parser = gst_xml_registry_parse_element_factory;
      }
      break;
    case GST_XML_REGISTRY_CAPS:
      if (!strcmp (element_name, "caps")) {
	xmlregistry->state = GST_XML_REGISTRY_PADTEMPLATE;
	xmlregistry->parser = gst_xml_registry_parse_padtemplate;
      }
      break;
    case GST_XML_REGISTRY_CAPSCOMP:
      if (!strcmp (element_name, "capscomp")) {
	GstCaps *caps;
	
	xmlregistry->state = GST_XML_REGISTRY_CAPS;
	xmlregistry->parser = gst_xml_registry_parse_padtemplate;

	caps = gst_caps_new (xmlregistry->caps_name, xmlregistry->caps_mime, xmlregistry->props);

	xmlregistry->caps = gst_caps_append (xmlregistry->caps, caps);
	xmlregistry->props = NULL;
      }
      break;
    case GST_XML_REGISTRY_PROPERTIES:
      if (!strcmp (element_name, "properties")) {
	xmlregistry->state = GST_XML_REGISTRY_CAPSCOMP;
	xmlregistry->parser = NULL;
      }
      break;
    default:
      break;
  }
}

static void
gst_xml_registry_text (GMarkupParseContext *context, const gchar *text,
                       gsize text_len, gpointer user_data, GError **error)
{
  GstXMLRegistry *xmlregistry = GST_XML_REGISTRY (user_data);
  gchar *open_tag;
  
  if (xmlregistry->open_tags) {
    open_tag = (gchar *)xmlregistry->open_tags->data;

    if (!strcmp (open_tag, "plugin-path")) {
      //gst_plugin_add_path (g_strndup (text, text_len));
    }
    else if (xmlregistry->parser) {
      xmlregistry->parser (context, open_tag, text, text_len, xmlregistry, error);
    }
  }
}

static void
gst_xml_registry_passthrough (GMarkupParseContext *context, const gchar *passthrough_text,
                              gsize text_len, gpointer user_data, GError **error)
{
}

static void
gst_xml_registry_error (GMarkupParseContext *context, GError *error,
                        gpointer user_data)
{
  g_print ("error %s\n", error->message);
}
/*
 * Save
 */
#define PUT_ESCAPED(tag,value) 					\
G_STMT_START{ 							\
  const gchar *toconv = value;					\
  if (value) {							\
    gchar *v = g_markup_escape_text (toconv, strlen (toconv));	\
    CLASS (xmlregistry)->save_func (xmlregistry, "<%s>%s</%s>\n", tag, v, tag);			\
    g_free (v);							\
  }								\
}G_STMT_END

static gboolean
gst_xml_registry_save_props_func (GstPropsEntry *entry, GstXMLRegistry *xmlregistry)
{
  const gchar *name;

  name = gst_props_entry_get_name (entry);

  switch (gst_props_entry_get_type (entry)) {
    case GST_PROPS_INT_TYPE:
    {
      gint value;
      gst_props_entry_get_int (entry, &value);
      CLASS (xmlregistry)->save_func (xmlregistry, "<int name=\"%s\" value=\"%d\"/>\n", name, value);
      break;
    }
    case GST_PROPS_INT_RANGE_TYPE:
    {
      gint min, max;
      gst_props_entry_get_int_range (entry, &min, &max);
      CLASS (xmlregistry)->save_func (xmlregistry, "<range name=\"%s\" min=\"%d\" max=\"%d\"/>\n", name, min, max);
      break;
    }
    case GST_PROPS_FLOAT_TYPE:
    {
      gfloat value;
      gst_props_entry_get_float (entry, &value);
      CLASS (xmlregistry)->save_func (xmlregistry, "<float name=\"%s\" value=\"%f\"/>\n", name, value);
      break;
    }
    case GST_PROPS_FLOAT_RANGE_TYPE:
    {
      gfloat min, max;
      gst_props_entry_get_float_range (entry, &min, &max);
      CLASS (xmlregistry)->save_func (xmlregistry, "<floatrange name=\"%s\" min=\"%f\" max=\"%f\"/>\n", name, min, max);
      break;
    }
    case GST_PROPS_FOURCC_TYPE:
    {
      guint32 fourcc;
      gst_props_entry_get_fourcc_int (entry, &fourcc);
      CLASS (xmlregistry)->save_func (xmlregistry, "<!--%4.4s-->\n", (gchar *)&fourcc);
      CLASS (xmlregistry)->save_func (xmlregistry, "<fourcc name=\"%s\" hexvalue=\"%08x\"/>\n", name, fourcc);
      break;
    }
    case GST_PROPS_BOOL_TYPE:
    {
      gboolean value;
      gst_props_entry_get_boolean (entry, &value);
      CLASS (xmlregistry)->save_func (xmlregistry, "<boolean name=\"%s\" value=\"%s\"/>\n", name, (value ? "true" : "false"));
      break;
    }
    case GST_PROPS_STRING_TYPE:
    {
      const gchar *value;
      gst_props_entry_get_string (entry, &value);
      CLASS (xmlregistry)->save_func (xmlregistry, "<string name=\"%s\" value=\"%s\"/>\n", name, value);
      break;
    }
    default:
      g_warning ("trying to save unknown property type %d", gst_props_entry_get_type (entry));
      return FALSE;
  }
  return TRUE;
}

static gboolean
gst_xml_registry_save_props (GstXMLRegistry *xmlregistry, GstProps *props)
{
  GList *proplist;

  proplist = props->properties;

  while (proplist) {
    GstPropsEntry *entry = (GstPropsEntry *) proplist->data;

    switch (gst_props_entry_get_type (entry)) {
      case GST_PROPS_LIST_TYPE: 
      {
	const GList *list;

	gst_props_entry_get_list (entry, &list);
	
        CLASS (xmlregistry)->save_func (xmlregistry, "<list name=\"%s\">\n", gst_props_entry_get_name (entry));
        g_list_foreach ((GList *)list, (GFunc) gst_xml_registry_save_props_func, xmlregistry);
        CLASS (xmlregistry)->save_func (xmlregistry, "</list>\n");
       	break;
      }
      default:
   	gst_xml_registry_save_props_func (entry, xmlregistry);
       	break;
    }
    proplist = g_list_next (proplist);
  }
  return TRUE;
}

static gboolean
gst_xml_registry_save_caps (GstXMLRegistry *xmlregistry, GstCaps *caps)
{
  while (caps) {
    CLASS (xmlregistry)->save_func (xmlregistry, "<capscomp>\n");
    PUT_ESCAPED ("name", caps->name);
    PUT_ESCAPED ("type", gst_type_find_by_id (caps->id)->mime);

    if (caps->properties) {
      CLASS (xmlregistry)->save_func (xmlregistry, "<properties>\n");
      gst_xml_registry_save_props (xmlregistry, caps->properties);
      CLASS (xmlregistry)->save_func (xmlregistry, "</properties>\n");
    }
    CLASS (xmlregistry)->save_func (xmlregistry, "</capscomp>\n");
    caps = caps->next;
  }
  return TRUE;
}

static gboolean
gst_xml_registry_save_pad_template (GstXMLRegistry *xmlregistry, GstPadTemplate *template)
{
  gchar *presence;
  
  PUT_ESCAPED ("nametemplate", template->name_template);
  CLASS (xmlregistry)->save_func (xmlregistry, "<direction>%s</direction>\n", (template->direction == GST_PAD_SINK? "sink":"src"));

  switch (template->presence) {
    case GST_PAD_ALWAYS:
      presence = "always";
      break;
    case GST_PAD_SOMETIMES:
      presence = "sometimes";
      break;
    case GST_PAD_REQUEST:
      presence = "request";
      break;
    default:
      presence = "unknown";
      break;
  }
  CLASS (xmlregistry)->save_func (xmlregistry, "<presence>%s</presence>\n", presence);

  if (GST_PAD_TEMPLATE_CAPS (template)) {
    CLASS (xmlregistry)->save_func (xmlregistry, "<caps>\n");
    gst_xml_registry_save_caps (xmlregistry, GST_PAD_TEMPLATE_CAPS (template));
    CLASS (xmlregistry)->save_func (xmlregistry, "</caps>\n");
  }
  return TRUE;
}

static gboolean
gst_xml_registry_save_feature (GstXMLRegistry *xmlregistry, GstPluginFeature *feature)
{
  PUT_ESCAPED ("name", feature->name);

  if (GST_IS_ELEMENT_FACTORY (feature)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (feature);
    GList *templates;

    PUT_ESCAPED ("longname", factory->details->longname);
    PUT_ESCAPED ("class", factory->details->klass);
    PUT_ESCAPED ("description", factory->details->description);
    PUT_ESCAPED ("version", factory->details->version);
    PUT_ESCAPED ("author", factory->details->author);
    PUT_ESCAPED ("copyright", factory->details->copyright);
    
    templates = factory->padtemplates;

    while (templates) {
      GstPadTemplate *template = GST_PAD_TEMPLATE (templates->data);

      CLASS (xmlregistry)->save_func (xmlregistry, "<padtemplate>\n");
      gst_xml_registry_save_pad_template (xmlregistry, template);
      CLASS (xmlregistry)->save_func (xmlregistry, "</padtemplate>\n");
      
      templates = g_list_next (templates);
    }
  }
  else if (GST_IS_TYPE_FACTORY (feature)) {
    GstTypeFactory *factory = GST_TYPE_FACTORY (feature);

    PUT_ESCAPED ("mime", factory->mime);
    PUT_ESCAPED ("extensions", factory->exts);
    if (factory->typefindfunc) {
      CLASS (xmlregistry)->save_func (xmlregistry, "<typefind/>\n");
    }
  }
  else if (GST_IS_SCHEDULER_FACTORY (feature)) {
    PUT_ESCAPED ("longdesc", GST_SCHEDULER_FACTORY (feature)->longdesc);
  }
  else if (GST_IS_AUTOPLUG_FACTORY (feature)) {
    PUT_ESCAPED ("longdesc", GST_AUTOPLUG_FACTORY (feature)->longdesc);
  }
  return TRUE;
}


static gboolean
gst_xml_registry_save_plugin (GstXMLRegistry *xmlregistry, GstPlugin *plugin)
{
  GList *walk;
  
  PUT_ESCAPED ("name", plugin->name);
  PUT_ESCAPED ("longname", plugin->longname);
  PUT_ESCAPED ("filename", plugin->filename);

  walk = plugin->features;

  while (walk) {
    GstPluginFeature *feature = GST_PLUGIN_FEATURE (walk->data);

    CLASS (xmlregistry)->save_func (xmlregistry, "<feature typename=\"%s\">\n", g_type_name (G_OBJECT_TYPE (feature)));
    gst_xml_registry_save_feature (xmlregistry, feature);
    CLASS (xmlregistry)->save_func (xmlregistry, "</feature>\n");
    
    walk = g_list_next (walk);
  }
  return TRUE;
}


static gboolean
gst_xml_registry_save (GstRegistry *registry)
{
  GList *walk;
  GstXMLRegistry *xmlregistry;
  
  g_return_val_if_fail (GST_IS_REGISTRY (registry), FALSE);
  g_return_val_if_fail (registry->flags & GST_REGISTRY_WRITABLE, FALSE);
  
  xmlregistry = GST_XML_REGISTRY (registry);

  if (!CLASS (xmlregistry)->open_func (xmlregistry, GST_XML_REGISTRY_WRITE)) {
    return FALSE;
  }

  walk = g_list_last (registry->plugins);

  CLASS (xmlregistry)->save_func (xmlregistry, "<?xml version=\"1.0\"?>\n");
  CLASS (xmlregistry)->save_func (xmlregistry, "<GST-PluginRegistry>\n");
  
  while (walk) {
    GstPlugin *plugin = GST_PLUGIN (walk->data);

    CLASS (xmlregistry)->save_func (xmlregistry, "<plugin>\n");
    gst_xml_registry_save_plugin (xmlregistry, plugin);
    CLASS (xmlregistry)->save_func (xmlregistry, "</plugin>\n");
    
    walk = g_list_previous (walk);
  }
  CLASS (xmlregistry)->save_func (xmlregistry, "</GST-PluginRegistry>\n");

  CLASS (xmlregistry)->close_func (xmlregistry);

  return TRUE;
}

static void
gst_xml_registry_rebuild_recurse (GstXMLRegistry *registry, const gchar *directory)
{
  GDir *dir;
  gboolean loaded = FALSE;

  dir = g_dir_open (directory, 0, NULL);

  if (dir) {
    const gchar *dirent;

    while ((dirent = g_dir_read_name (dir))) {
      gchar *dirname;
	
      dirname = g_strjoin ("/", directory, dirent, NULL);
      gst_xml_registry_rebuild_recurse (registry, dirname);
      g_free(dirname);
    }
    g_dir_close (dir);
  } else {
    if (strstr (directory, ".so")) {
      gchar *temp;

      if ((temp = strstr (directory, ".so")) &&
          (!strcmp (temp, ".so"))) {
	GstPlugin *plugin = g_new0 (GstPlugin, 1);
	plugin->filename = g_strdup (directory);

        loaded = gst_plugin_load_plugin (plugin);
	if (!loaded) {
          g_free (plugin->filename);
	  g_free (plugin);
	}
	else {
	  gst_registry_add_plugin (GST_REGISTRY (registry), plugin);
	}
      }
    }
  }
}

static gboolean
gst_xml_registry_rebuild (GstRegistry *registry)
{
  GList *walk = NULL;
  GstXMLRegistry *xmlregistry = GST_XML_REGISTRY (registry);
 
  walk = registry->paths;

  while (walk) {
    gchar *path = (gchar *) walk->data;
    
    gst_xml_registry_rebuild_recurse (xmlregistry, path);

    walk = g_list_next (walk);
  }
  
  return TRUE;
}
