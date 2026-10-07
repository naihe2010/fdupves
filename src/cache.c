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
/* @CFILE cache.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "cache.h"
#include "audio.h"

#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

cache_t *g_cache;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef WIN32
#define strtouq _strtoui64
#endif

#define CACHE_VERSION 2

struct cache_s
{
  sqlite3 *db;
  gchar *file;
};

static gboolean cache_exec (cache_t *cache, int (*cb) (sqlite3_stmt *, void *),
                            void *arg, const char *sql, const char *fmt, ...);
static void cache_remove_by_id (cache_t *cache, int media_id);
static int get_id_callback (sqlite3_stmt *stmt, void *para);

#define EBOOK_CREATE_TEXT                                                     \
  "create table ebook(id INTEGER PRIMARY KEY AUTOINCREMENT, media_id "        \
  "integer, hash varchar(32), title "                                         \
  "varchar(1024), author varchar(256), "                                      \
  "producer varchar(256), pubdate_year integer, pubdate_mon integer, "        \
  "pubdate_day integer, isbn varchar(128), text_hash varchar(32));"

const char *init_text
    = "create table media(id INTEGER PRIMARY KEY AUTOINCREMENT, path text, "
      "size bigint, mtime bigint);"
      "create table hash(id INTEGER PRIMARY KEY AUTOINCREMENT, media_id "
      "integer, alg int, offset real, hash varchar(32));"
      "create unique index index_path on media (path);" EBOOK_CREATE_TEXT;

static void
cache_init (cache_t *cache)
{
  char *errmsg = NULL;
  if (sqlite3_exec (cache->db, init_text, NULL, NULL, &errmsg) != 0)
    {
      g_warning ("init cache file error: %s", errmsg ? errmsg : "uknown");
      if (errmsg)
        {
          sqlite3_free (errmsg);
        }
    }
}

cache_t *
cache_open (const gchar *file)
{
  cache_t *cache;
  gchar *dirname;
  gboolean needInit;
  int version;

  cache = g_malloc (sizeof (cache_t));
  g_return_val_if_fail (cache, NULL);

  cache->file = g_strdup (file);

  needInit = FALSE;
  if (g_file_test (file, G_FILE_TEST_EXISTS) == FALSE)
    {
      needInit = TRUE;
      dirname = g_path_get_dirname (file);
      g_mkdir_with_parents (dirname, 0755);
      g_free (dirname);
    }

  if (sqlite3_open (file, &cache->db) != 0)
    {
      g_warning ("Open cache file: %s failed:%s.", file, strerror (errno));
      g_free (cache);
      return NULL;
    }

  if (needInit)
    {
      cache_init (cache);
    }

  version = 0;
  cache_exec (cache, get_id_callback, &version, "pragma user_version;", "");
  if (version < CACHE_VERSION)
    {
      if (version < 1)
        cache_exec (cache, NULL, NULL, "delete from hash;", "");
      if (version < 2)
        {
          cache_exec (cache, NULL, NULL, "drop table if exists ebook;", "");
          cache_exec (cache, NULL, NULL, EBOOK_CREATE_TEXT, "");
        }
      cache_exec (cache, NULL, NULL,
                  "pragma user_version = " G_STRINGIFY (CACHE_VERSION) ";",
                  "");
    }

  if (g_cache == NULL)
    {
      g_cache = cache;
    }

  return cache;
}

void
cache_close (cache_t *cache)
{
  sqlite3_close (cache->db);
  g_free (cache->file);
  g_free (cache);
}

static gboolean
cache_exec (cache_t *cache, int (*cb) (sqlite3_stmt *, void *), void *arg,
            const char *sql, const char *fmt, ...)
{
  int rc;
  va_list ap;
  const char *errMsg;
  sqlite3_stmt *stmt = NULL;
  int index;
  int valuei;
  long valuel;
  const char *values;
  double valued;

  if (sqlite3_prepare_v2 (cache->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
      return FALSE;
    }

  for (index = 1, va_start (ap, fmt); *fmt; fmt++)
    {
      if (*fmt == '%' || *fmt == ',' || isspace (*fmt))
        continue;

      if (*fmt == 'd')
        {
          valuei = va_arg (ap, int);
          sqlite3_bind_int (stmt, index++, valuei);
        }
      else if (*fmt == 'l')
        {
          valuel = va_arg (ap, long);
          sqlite3_bind_int64 (stmt, index++, valuel);
        }
      else if (*fmt == 'f')
        {
          valued = va_arg (ap, double);
          sqlite3_bind_double (stmt, index++, valued);
        }
      else if (*fmt == 's')
        {
          values = va_arg (ap, const char *);
          sqlite3_bind_text (stmt, index++, values, strlen(values), NULL);
        }
    }
  va_end (ap);

  rc = sqlite3_step (stmt);
  while (rc == SQLITE_ROW)
    {
      if (cb)
        {
          cb (stmt, arg);
        }
      rc = sqlite3_step (stmt);
    }

  if (rc != SQLITE_DONE)
    {
      errMsg = sqlite3_errstr (rc);
      g_warning ("SQL error: %s in [%s]", errMsg, sql);
      return FALSE;
    }

  sqlite3_reset (stmt);
  sqlite3_finalize (stmt);

  return TRUE;
}

static int
get_id_callback (sqlite3_stmt *stmt, void *para)
{
  *(int *) para = sqlite3_column_int (stmt, 0);
  return 0;
}

static int
get_hash_callback (sqlite3_stmt *stmt, void *para)
{
  hash_t *hp = (hash_t *)para;
  *hp = sqlite3_column_int64 (stmt, 0);
  return 0;
}

static int
get_hash_array_callback (sqlite3_stmt *stmt, void *para)
{
  audio_peak_hash hash;
  const unsigned char *str;
  hash_array_t **pHashArray = (hash_array_t **)para;

  if (*pHashArray == NULL)
    {
      *pHashArray = hash_array_new ();
    }
  if (*pHashArray == NULL)
    {
      g_warning ("hash array new error: %s", strerror (errno));
      return -1;
    }

  hash.offset = sqlite3_column_int (stmt, 0);
  str = sqlite3_column_text (stmt, 1);
  snprintf (hash.hash, sizeof hash.hash, "%s", str);
  hash_array_append (*pHashArray, &hash, sizeof (audio_peak_hash));

  return 0;
}

static long
cache_stat_mtime (GStatBuf *buf)
{
#if WIN32
  return buf->st_mtime;
#else
  return buf->st_mtim.tv_sec;
#endif
}

static int
cache_get_media_id (cache_t *cache, const gchar *file)
{
  int media_id;
  gboolean ret;

  media_id = -1;
  ret = cache_exec (cache, get_id_callback, &media_id,
                    "select id from media where path=?;", "%s", file);
  g_return_val_if_fail (ret, -1);

  if (media_id == -1)
    {
      GStatBuf buf[1];
      if (g_stat (file, buf) != 0)
        {
          g_warning ("stat error: %s", strerror (errno));
          return -1;
        }

      ret = cache_exec (
          cache, NULL, NULL,
          "insert into media(path, size, mtime) values(?, ?, ?);",
          "%s, %l, %l", file, buf->st_size, cache_stat_mtime (buf));
      g_return_val_if_fail (ret, -1);

      ret = cache_exec (cache, get_id_callback, &media_id,
                        "select id from media where path=?;", "%s", file);
      g_return_val_if_fail (ret, -1);
    }

  return media_id;
}

static gboolean
cache_drop_if_stale (cache_t *cache, int media_id, const gchar *file)
{
  GStatBuf buf[1];
  int same_id;

  if (g_stat (file, buf) != 0)
    {
      return FALSE;
    }

  same_id = -1;
  if (!cache_exec (cache, get_id_callback, &same_id,
                   "select id from media where id=? and size=? and mtime=?;",
                   "%d, %l, %l", media_id, buf->st_size,
                   cache_stat_mtime (buf)))
    {
      return FALSE;
    }

  if (same_id != -1)
    {
      return FALSE;
    }

  cache_remove_by_id (cache, media_id);
  return TRUE;
}

gboolean
cache_get (cache_t *cache, const gchar *file, float off, int alg, hash_t *hp)
{
  int media_id;
  gboolean ret;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);
  if (cache_drop_if_stale (cache, media_id, file))
    return FALSE;

  *hp = 0;
  ret = cache_exec (
      cache, get_hash_callback, hp,
      "select hash from hash where offset=? and alg=? and media_id=?",
      "%f %d %d", off, alg, media_id);
  g_return_val_if_fail (ret, FALSE);

  return *hp != 0;
}

gboolean
cache_set (cache_t *cache, const gchar *file, float off, int alg, hash_t h)
{
  int media_id;
  gboolean ret;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);

  ret = cache_exec (
      cache, NULL, NULL,
      "insert into hash(media_id, offset, alg, hash) values(?, ?, ?, ?);",
      "%d %f %d %l", media_id, off, alg, h);
  g_return_val_if_fail (ret, FALSE);

  return TRUE;
}

gboolean
cache_gets (cache_t *cache, const gchar *file, int alg,
            hash_array_t **pHashArray)
{
  int media_id;
  gboolean ret;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);
  if (cache_drop_if_stale (cache, media_id, file))
    return FALSE;

  *pHashArray = NULL;
  ret = cache_exec (
      cache, get_hash_array_callback, pHashArray,
      "select offset, hash from hash where alg = ? and media_id = ?;", "%d %d",
      alg, media_id);
  g_return_val_if_fail (ret, FALSE);

  return (*pHashArray != NULL);
}

gboolean
cache_sets (cache_t *cache, const gchar *file, int alg,
            hash_array_t *hashArray)
{
  int media_id, i;
  audio_peak_hash *hash;
  gboolean ret;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);

  for (i = 0; i < hash_array_size (hashArray); ++i)
    {
      hash = hash_array_index (hashArray, i);
      ret = cache_exec (cache, NULL, NULL,
                        "insert into hash(media_id, offset, alg, hash) "
                        "values(?, ?, ?, ?);",
                        "%d, %d, %d, %l", media_id, hash->offset, alg,
                        hash->hash);
      g_return_val_if_fail (ret, FALSE);
    }

  return TRUE;
}

gboolean
cache_set_ebook (cache_t *cache, const char *file, ebook_hash_t *h)
{
  int media_id;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);

  return cache_exec (
      cache, NULL, NULL,
      "insert into ebook(media_id, hash, title, author, producer, "
      "pubdate_year, pubdate_mon, pubdate_day, isbn, text_hash) values(?, ?, "
      "?, ?, ?, ?, "
      "?, ?, ?, ?);",
      "%d, %l, %s, %s, %s, %d, %d, %d, %s, %l", media_id, h->cover_hash,
      h->title, h->author, h->producer, h->public_date.year,
      h->public_date.month, h->public_date.day, h->isbn, h->text_hash);
}

struct ebook_result
{
  int got;
  ebook_hash_t *hash;
};

static int
get_ebook_callback (sqlite3_stmt *stmt, void *para)
{
  struct ebook_result *result = para;
  const unsigned char *str;
  ebook_hash_t *h = result->hash;
  result->got = 1;

  h->cover_hash = sqlite3_column_int64 (stmt, 2);
  str = sqlite3_column_text (stmt, 3);
  snprintf (h->title, sizeof (h->title), "%s", str);
  str = sqlite3_column_text (stmt, 4);
  snprintf (h->author, sizeof (h->author), "%s", str);
  str = sqlite3_column_text (stmt, 5);
  snprintf (h->producer, sizeof (h->producer), "%s", str);
  h->public_date.year = sqlite3_column_int (stmt, 6);
  h->public_date.month = sqlite3_column_int (stmt, 7);
  h->public_date.day = sqlite3_column_int (stmt, 8);
  str = sqlite3_column_text (stmt, 9);
  snprintf (h->isbn, sizeof (h->isbn), "%s", str);
  h->text_hash = sqlite3_column_int64 (stmt, 10);

  return 0;
}

gboolean
cache_get_ebook (cache_t *cache, const char *file, ebook_hash_t *h)
{
  struct ebook_result result[1];
  int media_id;
  gboolean ret;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);
  if (cache_drop_if_stale (cache, media_id, file))
    return FALSE;

  result->got = 0;
  result->hash = h;
  ret = cache_exec (cache, get_ebook_callback, result,
                    "select * from ebook where media_id=?;", "%d", media_id);
  g_return_val_if_fail (ret, FALSE);
  return result->got == 1;
}

static void
cache_remove_by_id (cache_t *cache, int media_id)
{
  cache_exec (cache, NULL, NULL, "delete from ebook where media_id=?;", "%d",
              media_id);
  cache_exec (cache, NULL, NULL, "delete from hash where media_id=?;", "%d",
              media_id);

  cache_exec (cache, NULL, NULL, "delete from media where id=?;", "%d",
              media_id);
}

gboolean
cache_remove (cache_t *cache, const gchar *file)
{
  int media_id;

  media_id = cache_get_media_id (cache, file);
  g_return_val_if_fail (media_id != -1, FALSE);

  cache_remove_by_id (cache, media_id);
  return TRUE;
}

static int
cache_remove_if_no_exists_callback (sqlite3_stmt *stmt, void *para)
{
  cache_t *cache = (cache_t *)para;
  const char *path;
  int media_id;

  path = (const char *)sqlite3_column_text (stmt, 1);
  if (g_file_test (path, G_FILE_TEST_EXISTS) == FALSE)
    {
      media_id = sqlite3_column_int (stmt, 0);
      cache_remove_by_id (cache, media_id);
    }
  return 0;
}

void
cache_cleanup (cache_t *cache)
{
  cache_exec (cache, cache_remove_if_no_exists_callback, cache,
              "select id, path from media;", "");
}
