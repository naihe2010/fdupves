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
/* @CFILE fdini.h
 *
 *  Author: Alf <naihe2010@126.com>
 */
/* @date Created: 2013/01/16 11:02:08 Alf*/

#ifndef _FDUPVES_INI_H_
#define _FDUPVES_INI_H_

#include <glib.h>

#ifndef SAME_RATE_MIN
#define SAME_RATE_MIN 0
#endif

#ifndef SAME_RATE_MAX
#define SAME_RATE_MAX 10
#endif

typedef struct {
    gboolean verbose;

    gboolean proc_image;
    gchar **image_suffix;

    gboolean proc_video;
    gchar **video_suffix;

    gboolean proc_audio;
    gchar **audio_suffix;

    gint compare_area;

    gboolean proc_other;

    gint compare_count;

    gint same_image_distance;
    gint same_video_distance;
    gint same_audio_distance;

    gint thumb_size[2];

    gint video_timers[0x10][3];

    gchar *cache_file;

    /* Private values */
    GKeyFile *keyfile;

} ini_t;

extern ini_t *g_ini;

ini_t *ini_new();

ini_t *ini_new_with_file(const gchar *);

gboolean ini_load(ini_t *, const gchar *);

gboolean ini_save(ini_t *, const gchar *);

#endif
