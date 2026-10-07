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

static hash_t
fnv1a64 (const char *s)
{
  hash_t h = 0xcbf29ce484222325ULL;

  for (; *s; ++s)
    {
      h ^= (unsigned char)*s;
      h *= 0x100000001b3ULL;
    }

  return h;
}

static void
add_token (GPtrArray *tokens, GString *token)
{
  if (token->len > 0)
    {
      g_ptr_array_add (tokens, g_strdup (token->str));
      g_string_truncate (token, 0);
    }
}

hash_t
text_simhash (const char *text, gsize len)
{
  const char *p, *next, *end;
  gunichar c;
  GString *token;
  GPtrArray *tokens;
  gchar *shingle;
  hash_t h;
  int sums[64] = { 0 };
  guint i, n;
  int b;

  tokens = g_ptr_array_new_with_free_func (g_free);
  token = g_string_new (NULL);
  end = text + len;
  for (p = text; p < end; p = next)
    {
      c = g_utf8_get_char_validated (p, end - p);
      next = c < (gunichar)-2 ? g_utf8_next_char (p) : p + 1;
      if (c < (gunichar)-2 && g_unichar_isalnum (c))
        g_string_append_unichar (token, g_unichar_tolower (c));
      else
        add_token (tokens, token);
    }
  add_token (tokens, token);
  g_string_free (token, TRUE);

  n = tokens->len >= 3 ? tokens->len - 2 : 0;
  h = 0;
  if (n >= 16)
    {
      for (i = 0; i < n; ++i)
        {
          shingle = g_strjoin (" ", g_ptr_array_index (tokens, i),
                               g_ptr_array_index (tokens, i + 1),
                               g_ptr_array_index (tokens, i + 2), NULL);
          h = fnv1a64 (shingle);
          g_free (shingle);
          for (b = 0; b < 64; ++b)
            sums[b] += (h >> b) & 1 ? 1 : -1;
        }
      h = 0;
      for (b = 0; b < 64; ++b)
        if (sums[b] > 0)
          h |= (hash_t)1 << b;
    }
  g_ptr_array_free (tokens, TRUE);

  return h;
}

static gchar *
ebook_normalize_isbn (const char *isbn)
{
  GString *s = g_string_new (NULL);

  for (; *isbn; ++isbn)
    if (g_ascii_isdigit (*isbn) || g_ascii_toupper (*isbn) == 'X')
      g_string_append_c (s, g_ascii_toupper (*isbn));

  return g_string_free (s, FALSE);
}

static gchar *
ebook_normalize_text (const char *text)
{
  gchar *fold, **words, *ret;

  fold = g_utf8_casefold (text, -1);
  words = g_regex_split_simple ("\\s+", g_strstrip (fold), 0, 0);
  ret = g_strjoinv (" ", words);
  g_strfreev (words);
  g_free (fold);

  return ret;
}

static gboolean
ebook_text_equal (const char *a, const char *b)
{
  gchar *na, *nb;
  gboolean ret;

  na = ebook_normalize_text (a);
  nb = ebook_normalize_text (b);
  ret = *na != '\0' && strcmp (na, nb) == 0;
  g_free (na);
  g_free (nb);

  return ret;
}

int
ebook_hash_cmp (ebook_hash_t *ha, ebook_hash_t *hb)
{
  gchar *ia, *ib;
  gboolean same;

  ia = ebook_normalize_isbn (ha->isbn);
  ib = ebook_normalize_isbn (hb->isbn);
  same = *ia != '\0' && strcmp (ia, ib) == 0;
  g_free (ia);
  g_free (ib);
  if (same)
    return 0;

  if (ebook_text_equal (ha->title, hb->title)
      && ebook_text_equal (ha->author, hb->author))
    return 0;

  if (ha->text_hash && hb->text_hash)
    return hash_cmp (ha->text_hash, hb->text_hash);

  if (ha->cover_hash && hb->cover_hash)
    return hash_cmp (ha->cover_hash, hb->cover_hash);

  return FDUPVES_EBOOK_HASH_MAX;
}
