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
/* @CFILE ifind.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "find.h"
#include "hash.h"
#include "audio.h"
#include "video.h"
#include "ini.h"
#include "util.h"
#include "gui.h"

#include <string.h>

#ifndef FD_COMP_CNT
#define FD_COMP_CNT 2
#endif

struct st_hash {
    int seek;
    hash_t hash;
};

struct st_file {
    const char *path;
    float length;
    float offset;
    struct st_hash head[1];
    struct st_hash tail[1];
    hash_array_t *hashArray;
};

struct st_find {
    GPtrArray *ptr[0x10];
    find_type type;
    find_step *step;
    find_step_cb cb;
    GThreadPool *thread_pool;
    gpointer arg;
};

static void find_video_prepare(const gchar *file, struct st_find *find);

static void find_audio_prepare(const gchar *file, struct st_find *find);

static int image_hash_func(struct st_file *file);

static int video_hash_func(struct st_file *file);

static int audio_hashes_func(struct st_file *file);

static void st_file_free(struct st_file *);

int
find_images(GPtrArray *ptr, find_step_cb cb, gpointer arg) {
    size_t i, j;
    int dist, count;
    hash_t *hashs;
    find_step step[1];

    count = 0;

    hashs = g_new0 (hash_t, ptr->len);
    g_return_val_if_fail (hashs, 0);

    step->found = FALSE;
    step->total = ptr->len;
    step->doing = _ ("Generate image hash value");
    for (i = 0; i < ptr->len; ++i) {
        hashs[i] = image_file_hash((gchar *) g_ptr_array_index (ptr, i));
        step->now = i;
        cb(step, arg);
    }

    step->doing = _ ("Compare image hash value");
    step->now = 0;
    for (i = 0; i < ptr->len - 1; ++i) {
        for (j = i + 1; j < ptr->len; ++j) {
            dist = hash_cmp(hashs[i], hashs[j]);
            if (dist < g_ini->same_image_distance) {
                step->afile = g_ptr_array_index (ptr, i);
                step->bfile = g_ptr_array_index (ptr, j);
                step->found = TRUE;
                step->type = FD_SAME_IMAGE;
                cb(step, arg);
                ++count;
            }
        }

        step->now = i;
        step->found = FALSE;
        cb(step, arg);
    }

    g_free(hashs);

    return count;
}

int
find_videos(GPtrArray *ptr, find_step_cb cb, gpointer arg) {
    gsize i, j, g, group_cnt;
    int dist, count;
    struct st_find find[1];
    struct st_file *afile, *bfile;
    find_step step[1];
    gui_t *gui = (gui_t *) arg;

    count = 0;

    for (i = 0; g_ini->video_timers[i][0]; ++i) {
        find->ptr[i] = g_ptr_array_new_with_free_func((GFreeFunc) st_file_free);
    }
    group_cnt = i;

    step->found = FALSE;
    step->total = ptr->len;
    step->now = 0;
    step->doing = _ ("Generate video screenshot hash value");

    find->step = step;
    find->type = FD_VIDEO;
    find->cb = cb;
    find->arg = arg;
    g_ptr_array_foreach(ptr, (GFunc) find_video_prepare, find);

    step->doing = _ ("Compare video screenshot hash value");
    for (g = 0; g < group_cnt; ++g) {
        if (find->ptr[g]->len <= 0) {
            g_ptr_array_free(find->ptr[g], TRUE);
            continue;
        }

        find->thread_pool = g_thread_pool_new((GFunc) video_hash_func, find, g_ini->threads_count, FALSE, NULL);
        if (find->thread_pool == NULL) {
            g_ptr_array_free(find->ptr[g], TRUE);
            continue;
        }

        for (i = 0; i < find->ptr[g]->len; ++i) {
            afile = g_ptr_array_index (find->ptr[g], i);
            afile->offset = g_ini->video_timers[g][2];
            g_thread_pool_push(find->thread_pool, afile, NULL);
        }

        g_thread_pool_free(find->thread_pool, FALSE, TRUE);

        if (gui->quit)
            return 0;

        for (i = 0; i < find->ptr[g]->len - 1; ++i) {
            for (j = i + 1; j < find->ptr[g]->len; ++j) {
                afile = g_ptr_array_index (find->ptr[g], i);
                bfile = g_ptr_array_index (find->ptr[g], j);

                dist = hash_cmp(afile->head->hash,
                                bfile->head->hash);
                if (dist < g_ini->same_video_distance) {
                    step->found = TRUE;
                    step->afile = afile->path;
                    step->bfile = bfile->path;
                    step->type = FD_SAME_VIDEO_HEAD;
                    cb(step, arg);
                    ++count;
                    continue;
                }

                dist = hash_cmp(afile->tail->hash,
                                bfile->tail->hash);
                if (dist < g_ini->same_video_distance) {
                    step->found = TRUE;
                    step->afile = afile->path;
                    step->bfile = bfile->path;
                    step->type = FD_SAME_VIDEO_TAIL;
                    cb(step, arg);
                    ++count;
                }
            }

            step->found = FALSE;
            step->total = find->ptr[g]->len;
            step->now = i;
            cb(step, arg);
        }

        g_ptr_array_free(find->ptr[g], TRUE);
    }

    return count;
}

/* convert 0-9 distance to same peak count
 * num1, first peak count
 * num2, second peak count
 */
static int
distance_to_same_peak_count(gulong num1, gulong num2, int distance) {
    int minnum, count;
    int rate[] = {100, 90, 80, 50, 20,
                  10, 5, 2, 1, 0};

    minnum = num1 < num2 ? (int) num1 : (int) num2;
    count = minnum * rate[distance] / 100;

    if (count == 0)
        count = 1;

    return count;
}

int
find_audios(GPtrArray *ptr, find_step_cb cb, gpointer arg) {
    gsize i, j, dist;
    int count, peak_count;
    float blen, llen;
    struct st_find find[1];
    struct st_file *afile, *bfile;
    find_step step[1];
    gui_t *gui = (gui_t *) arg;
    static int rates[] = {0, 1, 2, 10, 20, 100};

    count = 0;
    find->ptr[0] = g_ptr_array_new_with_free_func((GFreeFunc) st_file_free);
    step->found = FALSE;
    step->total = ptr->len;
    step->now = 0;
    step->doing = _ ("Generate audio screenshot hash value");

    find->thread_pool = g_thread_pool_new((GFunc) audio_hashes_func, NULL, g_ini->threads_count, FALSE, NULL);
    if (find->thread_pool == NULL) {
        g_ptr_array_free(find->ptr[0], TRUE);
        return -1;
    }

    find->step = step;
    find->type = FIND_AUDIO;
    find->cb = cb;
    find->arg = arg;
    g_ptr_array_foreach(ptr, (GFunc) find_audio_prepare, find);

    g_thread_pool_free(find->thread_pool, FALSE, TRUE);

    if (gui->quit)
        return 0;

    step->doing = _ ("Compare audio hash value");
    for (i = 0; i < find->ptr[0]->len - 1; ++i) {
        afile = g_ptr_array_index (find->ptr[0], i);

        if (hash_array_size(afile->hashArray) == 0) {
            continue;
        }

        for (j = i + 1; j < find->ptr[0]->len; ++j) {
            bfile = g_ptr_array_index (find->ptr[0], j);

            if (hash_array_size(bfile->hashArray) == 0) {
                continue;
            }

            if (g_ini->filter_time_rate != 0) {
                blen = afile->length;
                llen = bfile->length;
                if (blen < llen) {
                    llen = afile->length;
                    blen = bfile->length;
                }
                if (llen * (float) (rates[g_ini->filter_time_rate] + 1) < blen) {
                    g_debug("%s length %f and %s lenght %f, filtered",
                            afile->path, afile->length, bfile->path, bfile->length);
                    continue;
                }
            }

            dist = audio_fingerprint_similarity(afile->hashArray, bfile->hashArray);
            if (dist == 0)
                continue;

            peak_count = distance_to_same_peak_count(hash_array_size(afile->hashArray),
                                                     hash_array_size(bfile->hashArray),
                                                     g_ini->same_audio_distance);
            g_debug("distance: %d, peaks %lu and %lu, need %d, dist: %lu", g_ini->same_audio_distance,
                    hash_array_size(afile->hashArray),
                    hash_array_size(bfile->hashArray),
                    peak_count, dist);
            if (dist >= peak_count) {
                step->found = TRUE;
                step->afile = afile->path;
                step->bfile = bfile->path;
                step->type = FD_SAME_AUDIO_HEAD;
                cb(step, arg);
                ++count;
                continue;
            }
        }

        step->found = FALSE;
        step->total = find->ptr[0]->len;
        step->now = i;
        cb(step, arg);
    }

    g_ptr_array_free(find->ptr[0], TRUE);

    return count;
}

static void
st_file_free(struct st_file *file) {
    if (file->hashArray)
        hash_array_free(file->hashArray);
    g_free(file);
}

static void
find_video_prepare(const gchar *file, struct st_find *find) {
    int i, length;
    struct st_file *stv;

    length = video_get_length(file);
    if (length <= 0) {
        g_warning ("Can't get duration of %s", file);
        return;
    }

    for (i = 0; g_ini->video_timers[i][0]; ++i) {
        if (length < g_ini->video_timers[i][0]
            || length > g_ini->video_timers[i][1]) {
            continue;
        }

        stv = g_malloc0(sizeof(struct st_file));

        stv->path = file;
        stv->length = length;

        g_ptr_array_add(find->ptr[i], stv);
    }

    ++find->step->now;
    find->cb(find->step, find->arg);
}

static void
find_audio_prepare(const gchar *file, struct st_find *find) {
    float length;
    struct st_file *stv;

    length = audio_get_length(file);
    if (length <= 0.1f) {
        g_warning ("Can't get duration of %s", file);
        return;
    }

    stv = g_malloc0(sizeof(struct st_file));

    stv->path = file;
    stv->length = length;
    stv->hashArray = NULL;

    g_thread_pool_push(find->thread_pool, stv, NULL);

    g_ptr_array_add(find->ptr[0], stv);

    ++find->step->now;
    find->cb(find->step, find->arg);
}

static int
image_hash_func(struct st_file *file) {
    return 0;
}

static int
video_hash_func(struct st_file *file) {
    file->head->hash = video_time_hash(file->path, file->offset);
    file->tail->hash = video_time_hash(file->path, file->length - file->offset);
    return 0;
}

static int
audio_hashes_func(struct st_file *file) {
    file->hashArray = audio_hashes(file->path);
    return 0;
}
