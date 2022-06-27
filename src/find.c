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

#include <string.h>

#ifndef FD_COMP_CNT
#define FD_COMP_CNT 2
#endif

struct st_hash {
    int seek;
    hash_t hash;
};

struct st_file {
    const char *file;
    int length;
    struct st_hash head[1];
    struct st_hash tail[1];
};

struct st_find {
    GPtrArray *ptr[0x10];
    find_type type;
    find_step *step;
    find_step_cb cb;
    gpointer arg;
};

static void find_prepare(const gchar *, struct st_find *);

static int find_time_hash(struct st_file *, int, int, int);

static gboolean find_is_same(struct st_file *, struct st_file *, gboolean, int);

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
    g_ptr_array_foreach(ptr, (GFunc) find_prepare, find);

    step->doing = _ ("Compare video screenshot hash value");
    for (g = 0; g < group_cnt; ++g) {
        if (find->ptr[g]->len <= 0) {
            g_ptr_array_free(find->ptr[g], TRUE);
            continue;
        }

        for (i = 0; i < find->ptr[g]->len - 1; ++i) {
            for (j = i + 1; j < find->ptr[g]->len; ++j) {
                afile = g_ptr_array_index (find->ptr[g], i);
                bfile = g_ptr_array_index (find->ptr[g], j);

                if (afile->head->hash == 0) {
                    find_time_hash(afile,
                                   g_ini->video_timers[g][2],
                                   0,
                                   FIND_VIDEO);
                }
                if (bfile->head->hash == 0) {
                    find_time_hash(bfile,
                                   g_ini->video_timers[g][2],
                                   0,
                                   FIND_VIDEO);
                }
                dist = hash_cmp(afile->head->hash,
                                bfile->head->hash);
                if (dist < g_ini->same_video_distance) {
                    if (find_is_same(afile, bfile, FALSE, FIND_VIDEO)) {
                        step->found = TRUE;
                        step->afile = afile->file;
                        step->bfile = bfile->file;
                        step->type = FD_SAME_VIDEO_HEAD;
                        cb(step, arg);
                        ++count;
                        continue;
                    }
                }

                if (afile->tail->hash == 0) {
                    find_time_hash(afile,
                                   afile->length - g_ini->video_timers[g][2],
                                   1,
                                   FIND_VIDEO);
                }
                if (bfile->tail->hash == 0) {
                    find_time_hash(bfile,
                                   bfile->length - g_ini->video_timers[g][2],
                                   1,
                                   FIND_VIDEO);
                }
                dist = hash_cmp(afile->tail->hash,
                                bfile->tail->hash);
                if (dist < g_ini->same_video_distance) {
                    if (find_is_same(afile, bfile, TRUE, FIND_VIDEO)) {
                        step->found = TRUE;
                        step->afile = afile->file;
                        step->bfile = bfile->file;
                        step->type = FD_SAME_VIDEO_TAIL;
                        cb(step, arg);
                        ++count;
                    }
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

int
find_audios(GPtrArray *ptr, find_step_cb cb, gpointer arg) {
    gsize i, j, g, group_cnt;
    int dist, count;
    struct st_find find[1];
    struct st_file *afile, *bfile;
    find_step step[1];

    count = 0;

    for (i = 0; g_ini->video_timers[i][0]; ++i) {
        find->ptr[i] = g_ptr_array_new_with_free_func((GFreeFunc) st_file_free);
    }
    group_cnt = i;

    step->found = FALSE;
    step->total = ptr->len;
    step->now = 0;
    step->doing = _ ("Generate audio screenshot hash value");

    find->step = step;
    find->type = FIND_AUDIO;
    find->cb = cb;
    find->arg = arg;
    g_ptr_array_foreach(ptr, (GFunc) find_prepare, find);

    step->doing = _ ("Compare audio screenshot hash value");
    for (g = 0; g < group_cnt; ++g) {
        if (find->ptr[g]->len <= 0) {
            g_ptr_array_free(find->ptr[g], TRUE);
            continue;
        }

        for (i = 0; i < find->ptr[g]->len - 1; ++i) {
            for (j = i + 1; j < find->ptr[g]->len; ++j) {
                afile = g_ptr_array_index (find->ptr[g], i);
                bfile = g_ptr_array_index (find->ptr[g], j);

                if (afile->head->hash == 0) {
                    find_time_hash(afile,
                                   g_ini->video_timers[g][2],
                                   0,
                                   FIND_AUDIO);
                }
                if (bfile->head->hash == 0) {
                    find_time_hash(bfile,
                                   g_ini->video_timers[g][2],
                                   0,
                                   FIND_AUDIO);
                }
                dist = hash_cmp(afile->head->hash,
                                bfile->head->hash);
                if (dist < g_ini->same_audio_distance) {
                    if (find_is_same(afile, bfile, FALSE, FIND_AUDIO)) {
                        step->found = TRUE;
                        step->afile = afile->file;
                        step->bfile = bfile->file;
                        step->type = FD_SAME_AUDIO_HEAD;
                        cb(step, arg);
                        ++count;
                        continue;
                    }
                }

                if (afile->tail->hash == 0) {
                    find_time_hash(afile,
                                   afile->length - g_ini->video_timers[g][2],
                                   1,
                                   FIND_AUDIO);
                }
                if (bfile->tail->hash == 0) {
                    find_time_hash(bfile,
                                   bfile->length - g_ini->video_timers[g][2],
                                   1,
                                   FIND_AUDIO);
                }
                dist = hash_cmp(afile->tail->hash,
                                bfile->tail->hash);
                if (dist < g_ini->same_video_distance) {
                    if (find_is_same(afile, bfile, TRUE, FIND_AUDIO)) {
                        step->found = TRUE;
                        step->afile = afile->file;
                        step->bfile = bfile->file;
                        step->type = FD_SAME_AUDIO_TAIL;
                        cb(step, arg);
                        ++count;
                    }
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

static void
st_file_free(struct st_file *file) {
    g_free(file);
}

static void
find_prepare(const gchar *file, struct st_find *find) {
    int i, length;
    struct st_file *stv;

    if (find->type == FIND_VIDEO) {
        length = video_get_length(file);
    } else if (find->type == FIND_AUDIO) {
        length = audio_get_length(file);
    } else {
        length = -1;
    }

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

        stv->file = file;
        stv->length = length;

        g_ptr_array_add(find->ptr[i], stv);
    }

    ++find->step->now;
    find->cb(find->step, find->arg);
}

static gboolean
find_is_same(struct st_file *afile, struct st_file *bfile, gboolean tail, int type) {
    int seeka[FD_COMP_CNT], seekb[FD_COMP_CNT];
    int i, rate, length;
    hash_t hasha, hashb;
    hash_t (*cb)(const char *, float);

    cb = video_time_hash;
    if (type == FIND_AUDIO) {
        cb = audio_time_hash;
    }

    if (tail) {
        length = afile->tail->seek < bfile->tail->seek ?
                 afile->tail->seek :
                 bfile->tail->seek;
        rate = length / (FD_COMP_CNT + 1);
        for (i = 0; i < FD_COMP_CNT; ++i) {
            seeka[i] = afile->tail->seek - (i + 1) * rate;
            seekb[i] = bfile->tail->seek - (i + 1) * rate;
        }
    } else {
        length = afile->length < bfile->length ? afile->length : bfile->length;
        rate = (length - afile->head->seek) / (FD_COMP_CNT + 1);
        for (i = 0; i < FD_COMP_CNT; ++i) {
            seeka[i] = afile->head->seek + (i + 1) * rate;
            seekb[i] = bfile->head->seek + (i + 1) * rate;
        }
    }

    for (i = 0; i < FD_COMP_CNT; ++i) {
        hasha = cb(afile->file, seeka[i]);
        hashb = cb(bfile->file, seekb[i]);
        if (hash_cmp(hasha, hashb) >= g_ini->same_video_distance) {
            return FALSE;
        }
    }

    return TRUE;
}

static int
find_time_hash(struct st_file *file, int seek, int tail, int type) {
    hash_t (*cb)(const char *, float);

    cb = video_time_hash;
    if (type == FIND_AUDIO) {
        cb = audio_time_hash;
    }

    if (tail) {
        file->tail->seek = seek;
        file->tail->hash = cb(file->file, seek);
    } else {
        file->head->seek = seek;
        file->head->hash = cb(file->file, seek);
    }

    return 0;
}
