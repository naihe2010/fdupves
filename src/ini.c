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
/* @CFILE ini.c
 *
 *  Author: Alf <naihe2010@126.com>
 */
/* @date Created: 2013/01/16 12:03:42 Alf*/

#include "ini.h"
#include "util.h"

#include <glib.h>

ini_t *g_ini;

static void ini_free(ini_t *);

ini_t *
ini_new() {
    ini_t *ini;

    const gchar *const isuffix = "bmp,gif,jpeg,jpg,jpe,png,pcx,pnm,tif,tga,xpm,ico,cur,ani";
    const gchar *const vsuffix = "avi,mp4,mpg,rmvb,rm,mov,mkv,m4v,mpg,mpeg,vob,asf,wmv,3gp,flv,mod,swf,mts,m2ts,ts";
    const gchar *const asuffix = "mp3,wma,wav,ogg,amr,m4a,mka,aac";

    ini = g_malloc0(sizeof(ini_t));
    g_return_val_if_fail (ini, NULL);

    if (g_ini == NULL) {
        g_ini = ini;
    }

    ini->keyfile = g_key_file_new();

    ini->verbose = FALSE;

    ini->proc_image = FALSE;
    ini->image_suffix = g_strsplit(isuffix, ",", -1);

    ini->proc_video = TRUE;
    ini->video_suffix = g_strsplit(vsuffix, ",", -1);

    ini->proc_audio = TRUE;
    ini->audio_suffix = g_strsplit(asuffix, ",", -1);

    ini->proc_other = FALSE;

    ini->compare_area = 0;

    ini->filter_time_rate = 0;

    ini->compare_count = 4;

    ini->same_image_distance = 6;
    ini->same_video_distance = 8;
    ini->same_audio_distance = 2;

    ini->thumb_size[0] = 512;
    ini->thumb_size[1] = 384;

    ini->video_timers[0][0] = 10;
    ini->video_timers[0][1] = 120;
    ini->video_timers[0][2] = 4;
    ini->video_timers[1][0] = 60;
    ini->video_timers[1][1] = 600;
    ini->video_timers[1][2] = 25;
    ini->video_timers[2][0] = 300;
    ini->video_timers[2][1] = 3000;
    ini->video_timers[2][2] = 120;
    ini->video_timers[3][0] = 1500;
    ini->video_timers[3][1] = 28800;
    ini->video_timers[3][2] = 600;
    ini->video_timers[4][0] = 0;

    ini->directories = NULL;

    ini->cache_file = g_build_filename(g_get_home_dir(),
                                       ".cache",
                                       "fdupves",
                                       NULL);

    return ini;
}

ini_t *
ini_new_with_file(const gchar *file) {
    ini_t *ini;

    ini = ini_new();
    g_return_val_if_fail (ini, NULL);

    if (ini_load(ini, file) == FALSE) {
        ini_free(ini);
        return NULL;
    }

    return ini;
}

gboolean
ini_load(ini_t *ini, const gchar *file) {
    gchar *path, *tmpstr;
    gint level;
    GError *err;

    path = fd_realpath(file);
    g_return_val_if_fail (path, FALSE);

    err = NULL;
    g_key_file_load_from_file(ini->keyfile, path, 0, &err);
    g_free(path);
    if (err) {
        g_warning ("load configuration file: %s failed: %s", file, err->message);
        g_error_free(err);
        return FALSE;
    }

    if (g_key_file_has_key(ini->keyfile, "_", "proc_image", NULL)) {
        ini->proc_image = g_key_file_get_boolean(ini->keyfile,
                                                 "_",
                                                 "proc_image",
                                                 NULL);
    }
    tmpstr = g_key_file_get_string(ini->keyfile, "_", "image_ext", NULL);
    if (tmpstr != NULL) {
        g_strfreev(ini->image_suffix);
        ini->image_suffix = g_strsplit(tmpstr, ",", 0);
    }

    if (g_key_file_has_key(ini->keyfile, "_", "proc_video", NULL)) {
        ini->proc_video = g_key_file_get_boolean(ini->keyfile,
                                                 "_",
                                                 "proc_video",
                                                 NULL);
    }
    tmpstr = g_key_file_get_string(ini->keyfile, "_", "video_ext", NULL);
    if (tmpstr != NULL) {
        g_strfreev(ini->video_suffix);
        ini->video_suffix = g_strsplit(tmpstr, ",", 0);
    }

    if (g_key_file_has_key(ini->keyfile, "_", "proc_audio", NULL)) {
        ini->proc_audio = g_key_file_get_boolean(ini->keyfile,
                                                 "_",
                                                 "proc_audio",
                                                 NULL);
    }
    tmpstr = g_key_file_get_string(ini->keyfile, "_", "audio_ext", NULL);
    if (tmpstr != NULL) {
        g_strfreev(ini->audio_suffix);
        ini->audio_suffix = g_strsplit(tmpstr, ",", 0);
    }

    level = g_key_file_get_integer(ini->keyfile, "_", "same_image_rate", &err);
    if (err) {
        g_warning ("configuration file: %s value error: %s, set as default.", file, err->message);
        g_error_free(err);
    } else {
        ini->same_image_distance = SAME_RATE_MAX - level;
    }

    level = g_key_file_get_integer(ini->keyfile, "_", "same_video_rate", &err);
    if (err) {
        g_warning ("configuration file: %s value error: %s, set as default.", file, err->message);
        g_error_free(err);
    } else {
        ini->same_video_distance = SAME_RATE_MAX - level;
    }
    level = g_key_file_get_integer(ini->keyfile, "_", "same_audio_rate", &err);
    if (err) {
        g_warning ("configuration file: %s value error: %s, set as default.", file, err->message);
        g_error_free(err);
    } else {
        ini->same_audio_distance = SAME_RATE_MAX - level;
    }

    if (g_key_file_has_key(ini->keyfile, "_", "compare_area", NULL)) {
        ini->compare_area = g_key_file_get_integer(ini->keyfile,
                                                   "_",
                                                   "compare_area",
                                                   NULL);
    }

    if (g_key_file_has_key(ini->keyfile, "_", "filter_time_rate", NULL)) {
        ini->filter_time_rate = g_key_file_get_integer(ini->keyfile,
                                                       "_",
                                                       "filter_time_rate",
                                                       NULL);
    }

    if (g_key_file_has_key(ini->keyfile, "_", "compare_count", NULL)) {
        ini->compare_count = g_key_file_get_integer(ini->keyfile,
                                                    "_",
                                                    "compare_count",
                                                    NULL);
    }

    if (g_key_file_has_key(ini->keyfile, "_", "directories", NULL)) {
        ini->directories = g_key_file_get_string_list(ini->keyfile,
                                                      "_",
                                                      "directories",
                                                      &ini->directory_count,
                                                      NULL);
    }

    return TRUE;
}

gboolean
ini_save(ini_t *ini, const gchar *file) {
    gchar *data, *path, *tmpstr;
    gsize len;

    path = fd_realpath(file);
    g_return_val_if_fail (path, FALSE);

    g_key_file_set_boolean(ini->keyfile, "_", "proc_image", ini->proc_image);
    tmpstr = g_strjoinv(",", ini->image_suffix);
    if (tmpstr) {
        g_key_file_set_string(ini->keyfile, "_", "image_ext", tmpstr);
        g_free(tmpstr);
    }

    g_key_file_set_boolean(ini->keyfile, "_", "proc_video", ini->proc_video);
    tmpstr = g_strjoinv(",", ini->video_suffix);
    if (tmpstr) {
        g_key_file_set_string(ini->keyfile, "_", "video_ext", tmpstr);
        g_free(tmpstr);
    }

    g_key_file_set_boolean(ini->keyfile, "_", "proc_audio", ini->proc_audio);
    tmpstr = g_strjoinv(",", ini->audio_suffix);
    if (tmpstr) {
        g_key_file_set_string(ini->keyfile, "_", "audio_ext", tmpstr);
        g_free(tmpstr);
    }

    g_key_file_set_integer(ini->keyfile, "_", "same_image_rate",
                           SAME_RATE_MAX - ini->same_image_distance);
    g_key_file_set_integer(ini->keyfile, "_", "same_video_rate",
                           SAME_RATE_MAX - ini->same_video_distance);
    g_key_file_set_integer(ini->keyfile, "_", "same_audio_rate",
                           SAME_RATE_MAX - ini->same_audio_distance);

    g_key_file_set_integer(ini->keyfile, "_", "compare_area", ini->compare_area);
    g_key_file_set_integer(ini->keyfile, "_", "filter_time_rate", ini->filter_time_rate);
    g_key_file_set_integer(ini->keyfile, "_", "compare_count", ini->compare_count);

    g_key_file_set_string_list(ini->keyfile, "_", "directories", (const gchar *const *) ini->directories,
                               ini->directory_count);

    data = g_key_file_to_data(ini->keyfile, &len, NULL);
    g_file_set_contents(path, data, len, NULL);
    g_free(path);
    g_free(data);

    return TRUE;
}

static
void ini_free(ini_t *ini) {
    if (g_ini == ini) {
        g_ini = NULL;
    }

    g_key_file_free(ini->keyfile);
    g_free(ini);
}
