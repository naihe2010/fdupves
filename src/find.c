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
#include "audio.h"
#include "ebook.h"
#include "gui.h"
#include "hash.h"
#include "ini.h"
#include "util.h"
#include "video.h"

#include <glib/gstdio.h>
#include <string.h>

#ifndef FD_COMP_CNT
#define FD_COMP_CNT 2
#endif

#define FDUPVES_VIDEO_SAMPLES 8

struct st_file
{
  const char *path;
  float length;
  hash_t hashes[FDUPVES_VIDEO_SAMPLES];
  hash_array_t *hashArray;
};

struct st_find
{
  GPtrArray *ptr[0x10];
  find_type type;
  find_step *step;
  find_step_cb cb;
  GThreadPool *thread_pool;
  gpointer arg;
};

static void find_video_prepare (const gchar *file, struct st_find *find);

static void find_audio_prepare (const gchar *file, struct st_find *find);

static int video_hash_func (struct st_file *file);

static int audio_hashes_func (struct st_file *file);

static void st_file_free (struct st_file *);

static gboolean
same_file_content (const char *a, const char *b)
{
  gchar *abuf, *bbuf;
  gsize alen, blen;
  gboolean same;

  abuf = NULL;
  bbuf = NULL;
  same = g_file_get_contents (a, &abuf, &alen, NULL)
         && g_file_get_contents (b, &bbuf, &blen, NULL) && alen == blen
         && memcmp (abuf, bbuf, alen) == 0;
  g_free (abuf);
  g_free (bbuf);

  return same;
}

int
find_images (GPtrArray *ptr, find_step_cb cb, gpointer arg)
{
  size_t i, j;
  int count;
  gboolean same;
  hash_t *phashs, *dhashs;
  goffset *sizes;
  GStatBuf st[1];
  const char *file;
  find_step step[1];

  count = 0;

  phashs = g_new0 (hash_t, ptr->len);
  dhashs = g_new0 (hash_t, ptr->len);
  sizes = g_new0 (goffset, ptr->len);

  step->found = FALSE;
  step->total = ptr->len;
  step->doing = _ ("Generate image hash value");
  for (i = 0; i < ptr->len; ++i)
    {
      file = g_ptr_array_index (ptr, i);
      image_file_hashes (file, phashs + i, dhashs + i);
      sizes[i] = g_stat (file, st) == 0 ? st->st_size : -1;
      step->now = i;
      cb (step, arg);
    }

  step->doing = _ ("Compare image hash value");
  step->now = 0;
  for (i = 0; i + 1 < ptr->len; ++i)
    {
      for (j = i + 1; j < ptr->len; ++j)
        {
          same = sizes[i] >= 0 && sizes[i] == sizes[j]
                 && same_file_content (g_ptr_array_index (ptr, i),
                                       g_ptr_array_index (ptr, j));
          if (!same)
            {
              same = hash_cmp (phashs[i], phashs[j])
                         < g_ini->same_image_distance
                     && hash_cmp (dhashs[i], dhashs[j])
                            < g_ini->same_image_distance;
            }
          if (same)
            {
              step->afile = g_ptr_array_index (ptr, i);
              step->bfile = g_ptr_array_index (ptr, j);
              step->found = TRUE;
              step->type = FD_SAME_IMAGE;
              cb (step, arg);
              ++count;
            }
        }

      step->now = i;
      step->found = FALSE;
      cb (step, arg);
    }

  g_free (phashs);
  g_free (dhashs);
  g_free (sizes);

  return count;
}

static gboolean
length_ratio_ok (float a, float b)
{
  float blen, llen;
  static int rates[] = { 0, 1, 2, 10, 20, 100 };

  if (g_ini->filter_time_rate == 0)
    {
      return TRUE;
    }

  blen = a;
  llen = b;
  if (blen < llen)
    {
      llen = a;
      blen = b;
    }

  return llen * (float)(rates[g_ini->filter_time_rate] + 1) >= blen;
}

int
find_videos (GPtrArray *ptr, find_step_cb cb, gpointer arg)
{
  gsize i, j;
  int k, same, count;
  struct st_find find[1];
  struct st_file *afile, *bfile;
  find_step step[1];
  gui_t *gui = (gui_t *)arg;

  count = 0;
  find->ptr[0] = g_ptr_array_new_with_free_func ((GFreeFunc)st_file_free);
  step->found = FALSE;
  step->total = ptr->len;
  step->now = 0;
  step->doing = _ ("Generate video screenshot hash value");

  find->thread_pool = g_thread_pool_new ((GFunc)video_hash_func, NULL,
                                         g_ini->threads_count, FALSE, NULL);
  if (find->thread_pool == NULL)
    {
      g_ptr_array_free (find->ptr[0], TRUE);
      return 0;
    }

  find->step = step;
  find->type = FD_VIDEO;
  find->cb = cb;
  find->arg = arg;
  g_ptr_array_foreach (ptr, (GFunc)find_video_prepare, find);

  g_thread_pool_free (find->thread_pool, FALSE, TRUE);

  if (gui->quit)
    {
      g_ptr_array_free (find->ptr[0], TRUE);
      return 0;
    }

  step->doing = _ ("Compare video screenshot hash value");
  for (i = 0; i + 1 < find->ptr[0]->len; ++i)
    {
      afile = g_ptr_array_index (find->ptr[0], i);

      for (j = i + 1; j < find->ptr[0]->len; ++j)
        {
          bfile = g_ptr_array_index (find->ptr[0], j);

          if (!length_ratio_ok (afile->length, bfile->length))
            {
              continue;
            }

          same = 0;
          for (k = 0; k < FDUPVES_VIDEO_SAMPLES; ++k)
            {
              if (hash_cmp (afile->hashes[k], bfile->hashes[k])
                  < g_ini->same_video_distance)
                {
                  ++same;
                }
            }

          if (same * 2 > FDUPVES_VIDEO_SAMPLES)
            {
              step->found = TRUE;
              step->afile = afile->path;
              step->bfile = bfile->path;
              step->type = FD_SAME_VIDEO_HEAD;
              cb (step, arg);
              ++count;
            }
        }

      step->found = FALSE;
      step->total = find->ptr[0]->len;
      step->now = i;
      cb (step, arg);
    }

  g_ptr_array_free (find->ptr[0], TRUE);

  return count;
}

/* convert 0-9 distance to same peak count
 * num1, first peak count
 * num2, second peak count
 */
static int
distance_to_same_peak_count (gulong num1, gulong num2, int distance)
{
  int minnum, count;
  int rate[] = { 100, 90, 80, 50, 20, 10, 5, 2, 1, 0 };

  minnum = num1 < num2 ? (int)num1 : (int)num2;
  if (distance >= (int)G_N_ELEMENTS (rate))
    distance = G_N_ELEMENTS (rate) - 1;
  if (distance < 0)
    distance = 0;
  count = minnum * rate[distance] / 100;

  if (count == 0)
    count = 1;

  return count;
}

int
find_audios (GPtrArray *ptr, find_step_cb cb, gpointer arg)
{
  gsize i, j, dist;
  int count, peak_count;
  struct st_find find[1];
  struct st_file *afile, *bfile;
  find_step step[1];
  gui_t *gui = (gui_t *)arg;

  count = 0;
  find->ptr[0] = g_ptr_array_new_with_free_func ((GFreeFunc)st_file_free);
  step->found = FALSE;
  step->total = ptr->len;
  step->now = 0;
  step->doing = _ ("Generate audio screenshot hash value");

  find->thread_pool = g_thread_pool_new ((GFunc)audio_hashes_func, NULL,
                                         g_ini->threads_count, FALSE, NULL);
  if (find->thread_pool == NULL)
    {
      g_ptr_array_free (find->ptr[0], TRUE);
      return -1;
    }

  find->step = step;
  find->type = FIND_AUDIO;
  find->cb = cb;
  find->arg = arg;
  g_ptr_array_foreach (ptr, (GFunc)find_audio_prepare, find);

  g_thread_pool_free (find->thread_pool, FALSE, TRUE);

  if (gui->quit)
    return 0;

  step->doing = _ ("Compare audio hash value");
  for (i = 0; i + 1 < find->ptr[0]->len; ++i)
    {
      afile = g_ptr_array_index (find->ptr[0], i);

      if (hash_array_size (afile->hashArray) == 0)
        {
          continue;
        }

      for (j = i + 1; j < find->ptr[0]->len; ++j)
        {
          bfile = g_ptr_array_index (find->ptr[0], j);

          if (hash_array_size (bfile->hashArray) == 0)
            {
              continue;
            }

          if (!length_ratio_ok (afile->length, bfile->length))
            {
              g_debug ("%s length %f and %s lenght %f, filtered", afile->path,
                       afile->length, bfile->path, bfile->length);
              continue;
            }

          dist = audio_fingerprint_similarity (afile->hashArray,
                                               bfile->hashArray);
          if (dist == 0)
            continue;

          peak_count = distance_to_same_peak_count (
              hash_array_size (afile->hashArray),
              hash_array_size (bfile->hashArray), g_ini->same_audio_distance);
          g_debug ("distance: %d, peaks %lu and %lu, need %d, dist: %lu",
                   g_ini->same_audio_distance,
                   hash_array_size (afile->hashArray),
                   hash_array_size (bfile->hashArray), peak_count, dist);
          if (dist >= peak_count)
            {
              step->found = TRUE;
              step->afile = afile->path;
              step->bfile = bfile->path;
              step->type = FD_SAME_AUDIO_HEAD;
              cb (step, arg);
              ++count;
              continue;
            }
        }

      step->found = FALSE;
      step->total = find->ptr[0]->len;
      step->now = i;
      cb (step, arg);
    }

  g_ptr_array_free (find->ptr[0], TRUE);

  return count;
}

int
find_ebooks (GPtrArray *ptr, find_step_cb cb, gpointer arg)
{
  guint i, j;
  int dist, count;
  ebook_hash_t *hashs;
  find_step step[1];

  count = 0;

  hashs = g_new0 (ebook_hash_t, ptr->len);
  g_return_val_if_fail (hashs, 0);

  step->found = FALSE;
  step->total = ptr->len;
  step->doing = _ ("Generate ebook hash value");
  for (i = 0; i < ptr->len; ++i)
    {
      ebook_file_hash ((gchar *)g_ptr_array_index (ptr, i), hashs + i);
      step->now = i;
      cb (step, arg);
    }

  step->doing = _ ("Compare ebook hash value");
  step->now = 0;
  for (i = 0; i + 1 < ptr->len; ++i)
    {
      for (j = i + 1; j < ptr->len; ++j)
        {
          dist = ebook_hash_cmp (hashs + i, hashs + j);
          if (dist < g_ini->same_image_distance)
            {
              step->afile = g_ptr_array_index (ptr, i);
              step->bfile = g_ptr_array_index (ptr, j);
              step->found = TRUE;
              step->type = FD_SAME_EBOOK;
              cb (step, arg);
              ++count;
            }
        }

      step->now = i;
      step->found = FALSE;
      cb (step, arg);
    }

  g_free (hashs);

  return count;
}

static void
st_file_free (struct st_file *file)
{
  if (file->hashArray)
    hash_array_free (file->hashArray);
  g_free (file);
}

static void
find_video_prepare (const gchar *file, struct st_find *find)
{
  int length;
  struct st_file *stv;

  length = video_get_length (file);
  if (length <= 0)
    {
      g_warning ("Can't get duration of %s", file);
      return;
    }

  stv = g_malloc0 (sizeof (struct st_file));

  stv->path = file;
  stv->length = length;

  g_thread_pool_push (find->thread_pool, stv, NULL);

  g_ptr_array_add (find->ptr[0], stv);

  ++find->step->now;
  find->cb (find->step, find->arg);
}

static void
find_audio_prepare (const gchar *file, struct st_find *find)
{
  float length;
  struct st_file *stv;

  length = audio_get_length (file);
  if (length <= 0.1f)
    {
      g_warning ("Can't get duration of %s", file);
      return;
    }

  stv = g_malloc0 (sizeof (struct st_file));

  stv->path = file;
  stv->length = length;
  stv->hashArray = NULL;

  g_thread_pool_push (find->thread_pool, stv, NULL);

  g_ptr_array_add (find->ptr[0], stv);

  ++find->step->now;
  find->cb (find->step, find->arg);
}

static int
video_hash_func (struct st_file *file)
{
  int k;

  for (k = 0; k < FDUPVES_VIDEO_SAMPLES; ++k)
    {
      file->hashes[k] = video_time_phash (
          file->path, file->length * (k + 0.5f) / FDUPVES_VIDEO_SAMPLES);
    }
  return 0;
}

static int
audio_hashes_func (struct st_file *file)
{
  file->hashArray = audio_hashes (file->path);
  return 0;
}
