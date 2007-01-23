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

#include <config.h>
#include "types.h"

#include "mem.h"
#include "hash.h"
#include "selection.h"

hash *selection_new(void) {
  return hash_new(sizeof (int));
}

void selection_set(hash *h, const char *key, int selected) {
  if(selected)
    hash_add(h, key, xmalloc_noptr(sizeof (int)), HASH_INSERT_OR_REPLACE);
  else
    hash_remove(h, key);
}

int selection_selected(hash *h, const char *key) {
  return hash_find(h, key) != 0;
}

void selection_flip(hash *h, const char *key) {
  selection_set(h, key, !selection_selected(h, key));
}

void selection_live(hash *h, const char *key) {
  int *ptr = hash_find(h, key);

  if(ptr)
    *ptr = 1;
}

static int selection_cleanup_callback(const char *key,
				      void *value,
				      void *v) {
  if(*(int *)value)
    *(int *)value = 0;
  else
    hash_remove((hash *)v, key);
  return 0;
}

void selection_cleanup(hash *h) {
  hash_foreach(h, selection_cleanup_callback, h);
}

static int selection_empty_callback(const char *key,
				    void attribute((unused)) *value,
				    void *v) {
  hash_remove((hash *)v, key);
  return 0;
}

void selection_empty(hash *h) {
  hash_foreach(h, selection_empty_callback, h);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
