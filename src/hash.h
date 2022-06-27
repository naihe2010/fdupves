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
/* @CFILE ihash.h
 *
 *  Author: Alf <naihe2010@126.com>
 */

#ifndef _FDUPVES_HASH_H_
#define _FDUPVES_HASH_H_

enum hash_type {
    FDUPVES_IMAGE_HASH,
    FDUPVES_IMAGE_PHASH,
    FDUPVES_AUDIO_HASH,
    FDUPVES_HASH_ALGS_CNT,
};
extern const char *hash_phrase[];

typedef unsigned long long hash_t;

hash_t image_file_hash(const char *);

hash_t image_buffer_hash(const char *, int);

hash_t video_time_hash(const char *, float);

hash_t video_time_phash(const char *, float);

hash_t image_file_phash(const char *);

hash_t image_buffer_phash(const char *, float);

hash_t audio_buffer_hash(const short *, int);

hash_t audio_time_hash(const char *, float);

int hash_cmp(hash_t, hash_t);

#endif
