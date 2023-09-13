/*
 * This file is part of the fdupves package
 * Copyright (C) <2010>  <Alf>
 *
 * Contact: Alf <naihe2010@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
/* @CFILE util.c
 *
 *  Author: Alf <naihe2010@126.com>
 */
/* @date Created: 2013/01/16 11:21:05 Alf*/

#include "util.h"
#include "ini.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

gchar *
fd_realpath (const gchar *path)
{
  gchar *abpath;
  gchar opath[PATH_MAX];

  if (g_path_is_absolute (path))
    {
      return g_strdup (path);
    }

  if (path[0] == '~')
    {
      const gchar *home = getenv ("HOME");
      if (home == NULL)
        {
          home = g_get_home_dir ();
        }
      if (home == NULL)
        {
          return NULL;
        }
      abpath = g_build_filename (home, path + 1, NULL);
    }
  else
    {
#ifdef WIN32
      getcwd (sizeof (opath), opath);
      strcat (opath, "\\");
      strcat (opath, path);
#else
      if (realpath (path, opath) == NULL)
        {
          return NULL;
        }
#endif
      abpath = g_strdup (opath);
    }

  return abpath;
}

gchar *
fd_install_path ()
{
  gchar *prgdir;

#ifdef WIN32
  prgdir = g_win32_get_package_installation_directory_of_module (NULL);
#else
  prgdir = g_strdup (CMAKE_INSTALL_PREFIX);
#endif

  return prgdir;
}

static int
is_type (const char *path, gchar **suffix)
{
  int i;
  const char *p, *s;

  p = strrchr (path, '.');
  if (p == NULL)
    {
      return 0;
    }

  p += 1;
  for (i = 0; suffix[i]; ++i)
    {
      s = suffix[i];
      if (g_ascii_strcasecmp (p, s) == 0)
        {
          return 1;
        }
    }

  return 0;
}

int
is_image (const gchar *path)
{
  return is_type (path, g_ini->image_suffix);
}

int
is_video (const gchar *path)
{
  return is_type (path, g_ini->video_suffix);
}

int
is_audio (const gchar *path)
{
  return is_type (path, g_ini->audio_suffix);
}

int
is_ebook (const gchar *path)
{
  return is_type (path, g_ini->ebook_suffix);
}

xmlNodeSetPtr
xmldoc_get_nodeset (xmlDocPtr doc, const char *xpath, const char *ns,
                    const char *url)
{
  xmlXPathContextPtr xpathctx;
  xmlXPathObjectPtr xpathobj;
  xmlNodeSetPtr nodes;

  xpathctx = xmlXPathNewContext (doc);
  if (xpathctx == NULL)
    {
      g_warning ("unable to create new XPath context\n");
      return NULL;
    }

  xmlXPathRegisterNs (xpathctx, BAD_CAST ns, BAD_CAST url);

  xpathobj = xmlXPathEvalExpression (BAD_CAST xpath, xpathctx);
  xmlXPathFreeContext (xpathctx);
  if (xpathobj == NULL)
    {
      g_warning ("unable to evaluate xpath expression \"%s\"\n", xpath);
      return NULL;
    }

  if (xmlXPathNodeSetIsEmpty (xpathobj->nodesetval))
    {
      g_debug ("unable to get \"%s\"", xpath);
      xmlXPathFreeObject (xpathobj);
      return NULL;
    }

  nodes = xpathobj->nodesetval;

  xmlXPathFreeNodeSetList (xpathobj);

  return nodes;
}

xmlNodePtr
xmldoc_get_node (xmlDocPtr doc, const char *xpath, const char *ns,
                 const char *url)
{
  xmlNodePtr node = NULL;
  xmlNodeSetPtr nodes = xmldoc_get_nodeset (doc, xpath, ns, url);
  if (nodes != NULL)
    {
      node = nodes->nodeTab[0];
      xmlXPathFreeNodeSet (nodes);
    }

  return node;
}

const gchar *
xml_get_value (const gchar *content, const gchar *xpath, gchar *out,
               size_t outlen)
{
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlReadMemory (content, strlen (content), NULL, NULL,
                       XML_PARSE_NOBLANKS | XML_PARSE_NSCLEAN);
  if (doc == NULL)
    {
      return NULL;
    }

  node = xmldoc_get_node (doc, xpath, "pdfx", "http://ns.adobe.com/pdfx/1.3/");
  if (node == NULL)
    {
      xmlFreeDoc (doc);
      return NULL;
    }

  g_snprintf (out, outlen, "%s", node->children->content);
  xmlFreeDoc (doc);

  return out;
}

char *
xmlnode_attr_get (xmlNodePtr node, const char *attr)
{
  xmlAttrPtr prop;

  for (prop = node->properties; prop != NULL; prop = prop->next)
    {
      if (prop->type == XML_ATTRIBUTE_NODE
          && strcmp ((char *)(prop->name), attr) == 0)
        {
          return g_strdup ((char *)prop->children->content);
        }
    }

  return NULL;
}