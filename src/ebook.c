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
#include "ini.h"

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

static void
ebook_normalize_isbn (const char *isbn, char *out, gsize size)
{
  GString *s = g_string_new (NULL);
  gsize digits;

  for (; *isbn; ++isbn)
    if (g_ascii_isdigit (*isbn) || g_ascii_toupper (*isbn) == 'X')
      g_string_append_c (s, g_ascii_toupper (*isbn));

  digits = strspn (s->str, "0123456789");
  if (!(s->len == 13 && digits == 13) && !(s->len == 10 && digits >= 9))
    g_string_truncate (s, 0);

  g_strlcpy (out, s->str, size);
  g_string_free (s, TRUE);
}

static void
ebook_normalize_text (const char *text, char *out, gsize size)
{
  gchar *fold;
  const gchar *p;
  gunichar c;
  GString *s;
  gboolean space;
  const gchar *end;

  g_utf8_validate (text, -1, &end);
  fold = g_utf8_casefold (text, end - text);
  s = g_string_new (NULL);
  space = FALSE;
  for (p = fold; *p; p = g_utf8_next_char (p))
    {
      c = g_utf8_get_char (p);
      if (g_unichar_isspace (c))
        {
          space = s->len > 0;
          continue;
        }
      if (space)
        g_string_append_c (s, ' ');
      space = FALSE;
      g_string_append_unichar (s, c);
    }
  g_free (fold);

  g_strlcpy (out, s->str, size);
  g_string_free (s, TRUE);
}

static void
ebook_normalize (ebook_hash_t *h)
{
  if (h->normalized)
    return;

  ebook_normalize_isbn (h->isbn, h->norm_isbn, sizeof h->norm_isbn);
  ebook_normalize_text (h->title, h->norm_title, sizeof h->norm_title);
  ebook_normalize_text (h->author, h->norm_author, sizeof h->norm_author);
  h->normalized = TRUE;
}

static gboolean
ebook_text_equal (const char *a, const char *b)
{
  return *a != '\0' && strcmp (a, b) == 0;
}

int
ebook_hash_cmp (ebook_hash_t *ha, ebook_hash_t *hb)
{
  gboolean text_same, cover_same;

  ebook_normalize (ha);
  ebook_normalize (hb);

  if (ebook_text_equal (ha->norm_isbn, hb->norm_isbn))
    return 0;

  text_same = ha->text_hash && hb->text_hash
              && hash_cmp (ha->text_hash, hb->text_hash)
                     < g_ini->same_image_distance;
  cover_same = ha->cover_hash && hb->cover_hash
               && hash_cmp (ha->cover_hash, hb->cover_hash)
                      < g_ini->same_image_distance;
  if ((text_same || cover_same)
      && ebook_text_equal (ha->norm_title, hb->norm_title)
      && ebook_text_equal (ha->norm_author, hb->norm_author))
    return 0;

  if (ha->text_hash && hb->text_hash)
    return hash_cmp (ha->text_hash, hb->text_hash);

  if (ha->cover_hash && hb->cover_hash)
    return hash_cmp (ha->cover_hash, hb->cover_hash);

  return FDUPVES_EBOOK_HASH_MAX;
}
