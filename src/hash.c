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
/* @CFILE ihash.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "hash.h"
#include "audio.h"
#include "cache.h"
#include "find.h"
#include "image.h"
#include "ini.h"
#include "util.h"
#include "video.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

static hash_t pixbuf_dhash (GdkPixbuf *);

#define FDUPVES_HASH_LEN 8

int
hash_cache_alg (int alg)
{
  return alg + g_ini->compare_area * 0x100;
}

GdkPixbuf *
pixbuf_compare_area (GdkPixbuf *pixbuf)
{
  int w, h;
  GdkPixbuf *sub, *area;

  if (g_ini->compare_area < FD_COMPARE_TOP
      || g_ini->compare_area > FD_COMPARE_RIGHT)
    {
      return g_object_ref (pixbuf);
    }

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);
  switch (g_ini->compare_area)
    {
    case FD_COMPARE_TOP:
      sub = gdk_pixbuf_new_subpixbuf (pixbuf, 0, 0, w, h / 2);
      break;

    case FD_COMPARE_BOTTOM:
      sub = gdk_pixbuf_new_subpixbuf (pixbuf, 0, h / 2, w, h - h / 2);
      break;

    case FD_COMPARE_LEFT:
      sub = gdk_pixbuf_new_subpixbuf (pixbuf, 0, 0, w / 2, h);
      break;

    default:
      sub = gdk_pixbuf_new_subpixbuf (pixbuf, w / 2, 0, w - w / 2, h);
      break;
    }

  area = gdk_pixbuf_scale_simple (sub, w, h, GDK_INTERP_BILINEAR);
  g_object_unref (sub);

  return area;
}

int
image_file_hashes (const char *file, hash_t *phash, hash_t *dhash)
{
  GdkPixbuf *buf, *orig, *area, *small;
  GError *err;

  if (g_cache
      && cache_get (g_cache, file, 0, hash_cache_alg (FDUPVES_IMAGE_PHASH),
                    phash)
      && cache_get (g_cache, file, 0, hash_cache_alg (FDUPVES_IMAGE_DHASH),
                    dhash))
    {
      return 0;
    }

  *phash = 0;
  *dhash = 0;

  orig = fdupves_gdkpixbuf_load_file_at_size (file, FDUPVES_PHASH_LEN,
                                              FDUPVES_PHASH_LEN, &err);
  if (err)
    {
      g_warning ("Load file: %s to pixbuf failed: %s", file, err->message);
      g_error_free (err);
      return -1;
    }

  buf = gdk_pixbuf_apply_embedded_orientation (orig);
  g_object_unref (orig);
  area = pixbuf_compare_area (buf);
  g_object_unref (buf);

  small = gdk_pixbuf_scale_simple (area, FDUPVES_HASH_LEN + 1,
                                   FDUPVES_HASH_LEN, GDK_INTERP_BILINEAR);
  *phash = pixbuf_phash (area);
  *dhash = pixbuf_dhash (small);
  g_object_unref (small);
  g_object_unref (area);

  if (g_cache)
    {
      if (*phash)
        {
          cache_set (g_cache, file, 0, hash_cache_alg (FDUPVES_IMAGE_PHASH),
                     *phash);
        }
      if (*dhash)
        {
          cache_set (g_cache, file, 0, hash_cache_alg (FDUPVES_IMAGE_DHASH),
                     *dhash);
        }
    }

  return 0;
}

static hash_t
pixbuf_dhash (GdkPixbuf *pixbuf)
{
  int rowstride, n_channels, x, y;
  guchar *pixels, *p;
  int grays[FDUPVES_HASH_LEN + 1];
  hash_t hash;

  g_assert (gdk_pixbuf_get_width (pixbuf) == FDUPVES_HASH_LEN + 1);
  g_assert (gdk_pixbuf_get_height (pixbuf) == FDUPVES_HASH_LEN);

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  hash = 0;
  for (y = 0; y < FDUPVES_HASH_LEN; ++y)
    {
      for (x = 0; x < FDUPVES_HASH_LEN + 1; ++x)
        {
          p = pixels + y * rowstride + x * n_channels;
          grays[x] = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;
        }
      for (x = 0; x < FDUPVES_HASH_LEN; ++x)
        {
          if (grays[x] < grays[x + 1])
            {
              hash |= ((hash_t)1) << (y * FDUPVES_HASH_LEN + x);
            }
        }
    }

  return hash;
}

int
hash_cmp (hash_t a, hash_t b)
{
  hash_t c;
  int cmp;

  if (!a || !b)
    {
      return FDUPVES_HASH_LEN * FDUPVES_HASH_LEN; /* max invalid distance */
    }

  c = a ^ b;
  for (cmp = 0; c; c = c >> 1)
    {
      if (c & 1)
        {
          ++cmp;
        }
    }

  return cmp;
}

hash_array_t *
audio_hashes (const char *path)
{
  hash_array_t *hashArray;

  if (g_cache)
    {
      if (cache_gets (g_cache, path, FDUPVES_AUDIO_HASH, &hashArray))
        {
          g_debug ("got %s cached peak hashes: %lu", path,
                   hash_array_size (hashArray));
          return hashArray;
        }
    }

  g_debug ("get %s peak hashes ...", path);
  hashArray = audio_fingerprint (path);
  g_debug ("get %s peak hashes: %lu", path,
           hashArray ? hash_array_size (hashArray) : 0);

  if (g_cache)
    {
      if (hashArray)
        {
          cache_sets (g_cache, path, FDUPVES_AUDIO_HASH, hashArray);
        }
    }

  return hashArray;
}

hash_array_t *
hash_array_new ()
{
  hash_array_t *hashArray;

  hashArray = g_new0 (hash_array_t, 1);
  g_return_val_if_fail (hashArray, NULL);

  hashArray->array = g_ptr_array_new_with_free_func (g_free);
  if (hashArray->array == NULL)
    {
      g_free (hashArray);
      return NULL;
    }

  return hashArray;
}

void
hash_array_free (hash_array_t *hashArray)
{
  g_ptr_array_free (hashArray->array, TRUE);
  g_free (hashArray);
}

gsize
hash_array_size (hash_array_t *hashArray)
{
  return hashArray->array->len;
}

void *
hash_array_index (hash_array_t *hashArray, int index)
{
  void *hashp = g_ptr_array_index (hashArray->array, index);
  return hashp;
}

void
hash_array_append (hash_array_t *hashArray, void *hash, size_t size)
{
  hash_t *nhash = g_malloc (size);
  g_return_if_fail (nhash);
  memcpy (nhash, hash, size);
  g_ptr_array_add (hashArray->array, nhash);
}
