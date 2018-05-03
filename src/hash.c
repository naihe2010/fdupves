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
#include "image.h"
#include "video.h"
#include "audio.h"
#include "ini.h"
#include "cache.h"
#include "util.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

const char *hash_phrase[] =
  {
    "image_hash",
    "image_phash",
    "audio_hash",
  };

static hash_t pixbuf_hash (GdkPixbuf *);

#define FDUPVES_HASH_LEN 8

hash_t
image_file_hash (const char *file)
{
  GdkPixbuf *buf;
  hash_t h;
  GError *err;

  if (g_cache)
    {
      if (cache_get (g_cache, file, 0, FDUPVES_IMAGE_HASH, &h))
	{
	  return h;
	}
    }

  buf = fdupves_gdkpixbuf_load_file_at_size (file,
					     FDUPVES_HASH_LEN,
					     FDUPVES_HASH_LEN,
					     &err);
  if (err)
    {
      g_warning ("Load file: %s to pixbuf failed: %s", file, err->message);
      g_error_free (err);
      return 0;
    }

  h = pixbuf_hash (buf);
  g_object_unref (buf);

  if (g_cache)
    {
      if (h)
	{
	  cache_set (g_cache, file, 0, FDUPVES_IMAGE_HASH, h);
	}
    }

  return h;
}

hash_t
image_buffer_hash (const char *buffer, int size)
{
  GdkPixbuf *buf;
  GError *err;
  hash_t h;

  err = NULL;
  buf = gdk_pixbuf_new_from_data ((const guchar *) buffer,
				  GDK_COLORSPACE_RGB,
				  FALSE,
				  8,
				  FDUPVES_HASH_LEN,
				  FDUPVES_HASH_LEN,
				  FDUPVES_HASH_LEN * 3,
				  NULL,
				  &err);
  if (err)
    {
      g_warning ("Load inline data to pixbuf failed: %s", err->message);
      g_error_free (err);
      return 0;
    }

  h = pixbuf_hash (buf);
  g_object_unref (buf);

  return h;
}

static hash_t
pixbuf_hash (GdkPixbuf *pixbuf)
{
  int width, height, rowstride, n_channels;
  guchar *pixels, *p;
  int *grays, sum, avg, x, y, off;
  hash_t hash;

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  grays = g_new0 (int, width * height);
  off = 0;
  for (y = 0; y < height; ++ y)
    {
      for (x = 0; x < width; ++ x)
	{
	  p = pixels + y * rowstride + x * n_channels;
	  grays[off] = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;
	  ++ off;
	}
    }

  sum = 0;
  for (x = 0; x < off; ++ x)
    {
      sum += grays[x];
    }
  avg = sum / off;

  hash = 0;
  for (x = 0; x < off; ++ x)
    {
      if (grays[x] >= avg)
	{
	  hash |= ((hash_t) 1 << x);
	}
    }

  g_free (grays);

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
  switch (g_ini->compare_area)
    {
    case 1:
      c = c & 0xFFFFFF00ULL;
      break;

    case 2:
      c = c & 0x00FFFFFFULL;
      break;

    case 3:
      c = c & 0xFCFCFCFCULL;
      break;

    case 4:
      c = c & 0x3F3F3F3FULL;
      break;

    default:
      break;
    }
  for (cmp = 0; c; c = c >> 1)
    {
      if (c & 1)
	{
	  ++ cmp;
	}
    }

  return cmp;
}

hash_t
video_time_hash (const char *file, float offset)
{
  hash_t h;
  gchar *buffer;
  gsize len;

  if (g_cache)
    {
      if (cache_get (g_cache, file, offset, FDUPVES_IMAGE_HASH, &h))
	{
	  return h;
	}
    }

  len = FDUPVES_HASH_LEN * FDUPVES_HASH_LEN * 3;
  buffer = g_malloc (len);
  g_return_val_if_fail (buffer, 0);

  video_time_screenshot (file, offset,
			 FDUPVES_HASH_LEN, FDUPVES_HASH_LEN,
			 buffer, len);

  h = image_buffer_hash (buffer, len);
  g_free (buffer);

  if (g_cache)
    {
      if (h)
	{
	  cache_set (g_cache, file, offset, FDUPVES_IMAGE_HASH, h);
	}
    }

  return h;
}

hash_t
audio_time_hash (const char *file, float offset)
{
  hash_t h;
  short buffer[AUDIO_HASH_COUNT];
#ifdef _DEBUG
  gchar *basename, outfile[PATH_MAX];
#endif

  if (g_cache)
    {
      if (cache_get (g_cache, file, offset, FDUPVES_AUDIO_HASH, &h))
	{
	  return h;
	}
    }

  if (audio_time_screenshot (file, offset,
			     AUDIO_HASH_COUNT,
                             buffer, sizeof (buffer)) <= 0)
    {
      g_warning (_ ("get audio screenshot failed: %s"), file);
      return 0;
    }

#ifdef _DEBUG
  basename = g_path_get_basename (file);
  g_snprintf (outfile, sizeof outfile, "%s/%s-%f.raw",
	      g_get_tmp_dir (),
	      basename, offset);
  g_free (basename);
  audio_time_screenshot_file (file, offset,
                              AUDIO_HASH_COUNT,
			      outfile);
#endif

  h = audio_buffer_hash (buffer, sizeof buffer);

  if (g_cache)
    {
      if (h)
	{
	  cache_set (g_cache, file, offset, FDUPVES_AUDIO_HASH, h);
	}
    }

  return h;
}

hash_t
audio_buffer_hash (const short *buffer, int length)
{
  unsigned char img_buf[AUDIO_HASH_LENGTH];
  double sum, avg;
  unsigned int uvalue;
  hash_t hash;
  size_t i, j;

  g_assert ((size_t) length <= AUDIO_HASH_COUNT * sizeof (short));

  for (i = 0; i < AUDIO_HASH_LENGTH; ++ i)
    {
      for (sum = 0.0, j = 0; j < AUDIO_HASH_FRAME; ++ j)
        {
          uvalue = buffer[i * AUDIO_HASH_OVERLAP + j] + USHRT_MAX;
          sum += uvalue;
        }
      avg = sum / AUDIO_HASH_FRAME;
      avg = avg * UCHAR_MAX / SHRT_MAX;
      img_buf[i] = avg;
    }

  for (sum = 0, i = 0; i < AUDIO_HASH_LENGTH; ++ i)
    {
      sum += img_buf[i];
    }
  avg = sum / AUDIO_HASH_LENGTH;

  for (hash = 0, i = 0; i < AUDIO_HASH_LENGTH; ++ i)
    {
      if (img_buf[i] >= avg)
        {
          hash |= ((hash_t) 1 << i);
        }
    }

  return hash;
}
