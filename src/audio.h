/*
 * This file is part of the fdvupes package
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
/* @CFILE audio.h
 *
 *  Author: Alf <naihe2010@126.com>
 */

#ifndef _FDUPVES_AUDIO_H_
#define _FDUPVES_AUDIO_H_

#include <stdio.h>
#include "hash.h"

typedef struct {
    /* filename */
    char *name;

    /* dirname */
    char *dir;

    /* Format */
    const char *format;

    /* Duration */
    double length;

    /* Size */
    int size[2];
} audio_info;

audio_info *audio_get_info(const char *file);

void audio_info_free(audio_info *info);

int audio_get_length(const char *file);

int audio_extract(const char *file,
                  float offset, float length,
                  int ar,
                  short **pBuffer, int *buf_len);

int audio_extract_to_file(const char *file,
                          float offset, float length,
                          int ar,
                          FILE *fp);

int audio_extract_to_wav(const char *file,
                         float offset, float length,
                         int ar,
                         const char *out_wav);

#endif
