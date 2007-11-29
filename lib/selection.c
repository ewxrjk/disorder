/*
 * This file is part of DisOrder
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file lib/selection.c
 * @brief Select management for Disobedience
 */

#include <config.h>
#include "types.h"

#include "mem.h"
#include "hash.h"
#include "selection.h"

/** @brief Create a new selection manager
 * @return Pointer to @ref hash used to manage the selection
 */
hash *selection_new(void) {
  return hash_new(sizeof (int));
}

/** @brief Add or remove a key in a selection
 * @param h Hash representing selection
 * @param key Key to insert
 * @param selected non-0 if key is selected, 0 if it is not
 *
 * @p key is copied so the pointer need not remain valid.  Newly selected keys
 * are not marked as live.
 */
void selection_set(hash *h, const char *key, int selected) {
  if(selected) {
    int *const liveness = xmalloc_noptr(sizeof (int));
    *liveness = 0;
    hash_add(h, key, liveness, HASH_INSERT_OR_REPLACE);
  } else
    hash_remove(h, key);
}

/** @brief Test whether a key is set in a selection
 * @param h Hash representing selection
 * @param key Key to check
 * @return non-0 if key is present, 0 if it is not
 */
int selection_selected(hash *h, const char *key) {
  return hash_find(h, key) != 0;
}

/** @brief Invert a key's selection status
 * @param h Hash representing selection
 * @param key Key to flip
 *
 * If the key is selected as a result it is not marked as live.
 */
void selection_flip(hash *h, const char *key) {
  selection_set(h, key, !selection_selected(h, key));
}

/** @brief Mark a selection key as live
 * @param h Hash representing selection
 * @param key Key to mark as live
 *
 * Live keys will survive a call to selection_cleanup().  @p need not be in the
 * selection (if it is not then the call will be ignored).
 */
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

/** @brief Delete all non-live keys from a selection
 * @param h Hash representing selection
 *
 * After cleanup, no keys are marked as live.
 */
void selection_cleanup(hash *h) {
  hash_foreach(h, selection_cleanup_callback, h);
}

static int selection_empty_callback(const char *key,
				    void attribute((unused)) *value,
				    void *v) {
  hash_remove((hash *)v, key);
  return 0;
}

/** @brief Remove all keys from a selection
 * @param h Hash representing selection
 */
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
