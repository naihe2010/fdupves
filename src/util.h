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
/* @CFILE util.h
 *
 *  Author: Alf <naihe2010@126.com>
 */
/* @date Created: 2013/01/16 11:20:08 Alf*/

#ifndef _FDUPVES_UTIL_H_
#define _FDUPVES_UTIL_H_

#include <gtk/gtk.h>

#include <glib.h>
#include <libintl.h>

/*
 * %s indicate the prefix
 * */
#define FD_SYS_CONF_FILE "etc/fdupvesrc"
#define FD_SYS_ICON_DIR "share/fdupves/icons"
#define FD_SYS_HELP_FILE "share/fdupves/fdupves.html"
#define FD_SYS_LOCALE_DIR "share/locale"

/*
 * ~ indicate the user home
 * */
#define FD_USR_CONF_FILE "~/.fdupvesrc"
#define FD_USR_CACHE_FILE "~/.fdupves.cache"

#define _(S) gettext (S)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FD_IMAGE 1
#define FD_VIDEO 2
#define FD_AUDIO 3
#define FD_EBOOK 4

gchar *fd_realpath (const gchar *);

gchar *fd_install_path ();

int is_image (const char *);

int is_video (const gchar *);

int is_audio (const gchar *);

int is_ebook (const gchar *);

#endif
