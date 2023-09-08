/*
 * This file is part of the fdupves package
 * Copyright (C) <2008> Alf
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
/* @CFILE image.h
 *
 *  Author: Alf <naihe2010@126.com>
 */
#ifndef _FDUPVES_IMAGE_H_
#define _FDUPVES_IMAGE_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf *fdupves_gdkpixbuf_load_file_at_size (const gchar *, int, int,
                                                GError **);

#endif
