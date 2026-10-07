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
/* @CFILE ebook.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "ebook.h"
#include "cache.h"

#include <string.h>

#define FDUPVES_EBOOK_HASH_MAX (64)

extern int ebook_hash (const char *file, ebook_hash_t *ehash);

typedef enum
{
  FDUPVES_EBOOK_UNKNOWN
} fdupves_ebook_type;

struct ebook_impl
{
  fdupves_ebook_type type;
  const char *type_prefix;

  int (*func) (const char *, ebook_hash_t *);
} ebook_impls[] = { FDUPVES_EBOOK_UNKNOWN, "" };

static struct ebook_impl *
find_extra_impl (const char *file)
{
  const char *p;
  int i;

  p = strrchr (file, '.');

  if (p)
    {
      for (i = 0; i < sizeof ebook_impls / sizeof ebook_impls[0]; ++i)
        {
          if (ebook_impls[i].func == NULL)
            continue;
          if (g_ascii_strcasecmp (p + 1, ebook_impls[i].type_prefix) == 0)
            return ebook_impls + i;
        }
    }

  return NULL;
}

int
ebook_file_hash (const char *file, ebook_hash_t *ehash)
{
  struct ebook_impl const *impl;
  int ret;

  if (g_cache && cache_get_ebook (g_cache, file, ehash))
    return 0;

  impl = find_extra_impl (file);
  if (impl != NULL)
    {
      ret = impl->func (file, ehash);
    }
  else
    {
      ret = ebook_hash (file, ehash);
    }
  if (ret == 0 && g_cache)
    {
      cache_set_ebook (g_cache, file, ehash);
    }
  return ret;
}

int
ebook_hash_cmp (ebook_hash_t *ha, ebook_hash_t *hb)
{
  if (ha->cover_hash && hb->cover_hash)
    {
      return hash_cmp (ha->cover_hash, hb->cover_hash);
    }

#define _ebook_key_cmp(key)                                                   \
  do                                                                          \
    {                                                                         \
      if (*ha->key != '\0' && *hb->key != '\0'                                \
          && strcmp (ha->key, hb->key) == 0)                                  \
        return 0;                                                             \
    }                                                                         \
  while (0)

  //_ebook_key_cmp (title);
  //_ebook_key_cmp (isbn);
  //_ebook_key_cmp (author);

  return FDUPVES_EBOOK_HASH_MAX;
}
