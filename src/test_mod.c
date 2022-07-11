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
/* @CFILE test_audio.c
 *
 *  Author: Alf <naihe2010@126.com>
 */
#include <assert.h>
#include "audio.h"
#include "../fingerprint/fingerprint.h"

int
main(int argc, char *argv[]) {
    hash_array_t *array;
    audio_peak_hash *hash;
    char buf[1000];
    int i, len;

    audio_extract_to_wav(argv[1], atoi(argv[2]), atoi(argv[3]),
                         16000, argv[4]);

    test_fingerprint(argv[1]);

    array = audio_fingerprint(argv[1]);
    if (array) {
        FILE *fp = fopen("/tmp/test1-fingerprint.dat", "w");
        assert(fp);
        fwrite("[", 1, 1, fp);
        for (i = 0; i < hash_array_size (array); ++ i) {
            hash = (audio_peak_hash *) hash_array_index(array, i);
            len = g_snprintf (buf, sizeof buf, "{\"hash\":\"%s\",\"offset\":\"%d\"},\n", hash->hash, hash->offset);
            fwrite(buf, 1, len, fp);
        }
        fwrite("]", 1, 1, fp);
        fclose(fp);
    }
}