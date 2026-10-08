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
/* @CFILE phash.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include <glib.h>
#include <libavutil/mathematics.h>
#include <math.h>
#include <stdlib.h>

#include "cache.h"
#include "hash.h"
#include "image.h"
#include "util.h"
#include "video.h"

#define FDUPVES_DCT_LEN 8

#define FDUPVES_VIDEO_SPACING 30.0f

static gdouble coefficient[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN];

static gdouble coefficient_t[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN];

static gboolean buffer_dct (const gdouble *, gdouble *, gsize);

static gpointer coefficient_init (gpointer);

static void matrix_mul (const gdouble *, const gdouble *, gdouble *);

static int dct_cmp (gconstpointer, gconstpointer);

static hash_t
video_offset_phash (video_t *video, const char *file, float offset)
{
  hash_t h;
  gchar buffer[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN * 3];
  GdkPixbuf *buf, *area;
#ifdef _DEBUG
  gchar *basename, outfile[PATH_MAX];
#endif

  if (g_cache
      && cache_get (g_cache, file, offset,
                    hash_cache_alg (FDUPVES_IMAGE_PHASH), &h))
    {
      return h;
    }

  if (video_screenshot (video, offset, FDUPVES_PHASH_LEN, FDUPVES_PHASH_LEN,
                        buffer, sizeof buffer)
      < 0)
    {
      return 0;
    }
#ifdef _DEBUG
  basename = g_path_get_basename (file);
  g_snprintf (outfile, sizeof outfile, "%s/%s-%f.png", g_get_tmp_dir (),
              basename, offset);
  g_free (basename);
  video_time_screenshot_file (file, offset, FDUPVES_PHASH_LEN * 100,
                              FDUPVES_PHASH_LEN * 100, outfile);
#endif

  buf = gdk_pixbuf_new_from_data (
      (const guchar *)buffer, GDK_COLORSPACE_RGB, FALSE, 8, FDUPVES_PHASH_LEN,
      FDUPVES_PHASH_LEN, FDUPVES_PHASH_LEN * 3, NULL, NULL);
  area = pixbuf_compare_area (buf);
  h = pixbuf_phash (area);
  g_object_unref (area);
  g_object_unref (buf);

  if (g_cache && h)
    {
      cache_set (g_cache, file, offset, hash_cache_alg (FDUPVES_IMAGE_PHASH),
                 h);
    }

  return h;
}

float
video_phashes (const char *file, hash_t *head, hash_t *tail, int count)
{
  video_t *video;
  double length;
  float spacing;
  int k;

  video = video_open (file, &length);
  if (video == NULL)
    {
      return 0;
    }

  if (length > 0)
    {
      spacing = MIN (FDUPVES_VIDEO_SPACING, (float)length / (count + 1));
      for (k = 0; k < count; ++k)
        {
          head[k] = video_offset_phash (video, file, spacing * (k + 1));
        }
      for (k = count - 1; k >= 0; --k)
        {
          if (length <= FDUPVES_VIDEO_SPACING * (count + 1))
            {
              tail[k] = head[count - 1 - k];
            }
          else
            {
              tail[k] = video_offset_phash (
                  video, file, (float)length - spacing * (k + 1));
            }
        }
    }

  video_close (video);

  return length;
}

hash_t
pixbuf_phash (GdkPixbuf *pixbuf)
{
  int width, height, rowstride, n_channels;
  guchar *pixels, *p;
  int x, y, off, min, max;
  hash_t hash;
  gdouble median, grays[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN],
      dct[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN],
      dctc[FDUPVES_DCT_LEN * FDUPVES_DCT_LEN - 1];

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  g_return_val_if_fail (
      width == FDUPVES_PHASH_LEN && height == FDUPVES_PHASH_LEN, 0);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  off = 0;
  min = 255;
  max = 0;
  for (y = 0; y < height; ++y)
    {
      for (x = 0; x < width; ++x)
        {
          p = pixels + y * rowstride + x * n_channels;
          grays[off] = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;
          min = MIN (min, (int)grays[off]);
          max = MAX (max, (int)grays[off]);
          ++off;
        }
    }

  if (max - min < 2)
    {
      return 0;
    }

  buffer_dct (grays, dct, G_N_ELEMENTS (dct));

  off = 0;
  for (y = 0; y < FDUPVES_DCT_LEN; ++y)
    {
      for (x = 0; x < FDUPVES_DCT_LEN; ++x)
        {
          if (x || y)
            {
              dctc[off] = dct[y * FDUPVES_PHASH_LEN + x];
              ++off;
            }
        }
    }
  qsort (dctc, off, sizeof dctc[0], dct_cmp);
  median = dctc[off / 2];

  hash = 0;
  for (y = 0; y < FDUPVES_DCT_LEN; ++y)
    {
      for (x = 0; x < FDUPVES_DCT_LEN; ++x)
        {
          if ((x || y) && dct[y * FDUPVES_PHASH_LEN + x] > median)
            {
              hash |= ((hash_t)1) << (y * FDUPVES_DCT_LEN + x);
            }
        }
    }

  return hash;
}

static int
dct_cmp (gconstpointer a, gconstpointer b)
{
  gdouble da, db;

  da = *(const gdouble *)a;
  db = *(const gdouble *)b;

  return (da > db) - (da < db);
}

static gboolean
buffer_dct (const gdouble *pix, gdouble *out_pix, gsize out_len)
{
  static GOnce once = G_ONCE_INIT;
  gdouble temp[FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN];

  g_assert (out_len >= FDUPVES_PHASH_LEN * FDUPVES_PHASH_LEN);

  g_once (&once, coefficient_init, NULL);

  matrix_mul (coefficient, pix, temp);
  matrix_mul (temp, coefficient_t, out_pix);

  return TRUE;
}

static gpointer
coefficient_init (gpointer data)
{
  gsize i, j;
  gdouble s;

  s = 1.0 / sqrt (FDUPVES_PHASH_LEN);
  for (i = 0; i < FDUPVES_PHASH_LEN; i++)
    {
      coefficient[i] = s;
    }
  for (i = 1; i < FDUPVES_PHASH_LEN; i++)
    {
      for (j = 0; j < FDUPVES_PHASH_LEN; j++)
        {
          coefficient[i * FDUPVES_PHASH_LEN + j]
              = sqrt (2.0 / FDUPVES_PHASH_LEN)
                * cos (i * M_PI * (j + 0.5) / (gdouble)FDUPVES_PHASH_LEN);
        }
    }

  for (i = 0; i < FDUPVES_PHASH_LEN; i++)
    {
      for (j = 0; j < FDUPVES_PHASH_LEN; j++)
        {
          coefficient_t[i * FDUPVES_PHASH_LEN + j]
              = coefficient[j * FDUPVES_PHASH_LEN + i];
        }
    }

  return data;
}

static void
matrix_mul (const gdouble *A, const gdouble *B, gdouble *matrix)
{
  gdouble t;
  gsize i, j, k;

  for (i = 0; i < FDUPVES_PHASH_LEN; i++)
    {
      for (j = 0; j < FDUPVES_PHASH_LEN; j++)
        {
          t = 0.0;
          for (k = 0; k < FDUPVES_PHASH_LEN; k++)
            {
              t += A[i * FDUPVES_PHASH_LEN + k] * B[k * FDUPVES_PHASH_LEN + j];
            }
          matrix[i * FDUPVES_PHASH_LEN + j] = t;
        }
    }
}
