/*
 * This file is part of DisOrder
 * Copyright (C) 2006 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef CACHE_H
#define CACHE_H

/* Defines a cache mapping keys to typed data items */

struct cache_type {
  int lifetime;                         /* Lifetime of a cache entry */
};

void cache_put(const struct cache_type *type,
               const char *key, const void *value);
/* Inserts KEY into the cache with value VALUE.  If KEY is already
 * present it is overwritten. */

const void *cache_get(const struct cache_type *type, const char *key);
/* Get a value from the cache. */

void cache_expire(void);
/* Expire values from the cache */

void cache_clean(const struct cache_type *type);
/* Clean all elements of a particular type, or all elements if TYPE=0 */

#endif /* CACHE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
/* arch-tag:8TQYM58jrfK4bi9YaWTutQ */
