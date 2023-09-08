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
/* @CFILE ihash.h
 *
 *  Author: Alf <naihe2010@126.com>
 */

#ifndef _FDUPVES_EBOOK_H_
#define _FDUPVES_EBOOK_H_

typedef struct
{

} ebook_hash_t;

ebook_hash_t ebook_file_hash (const char *file);

int ebook_hash_cmp (ebook_hash_t *ha, ebook_hash_t *hb);

#endif
