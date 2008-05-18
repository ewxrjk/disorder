/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2008 Richard Kettlewell
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

#ifndef HASH_H
#define HASH_H

typedef struct hash hash;
struct kvp;

hash *hash_new(size_t valuesize);
/* Create a new hash */

int hash_add(hash *h, const char *key, const void *value, int mode);
#define HASH_INSERT 0
#define HASH_REPLACE 1
#define HASH_INSERT_OR_REPLACE 2
/* Insert/replace a value in the hash.  Returns 0 on success, -1 on
 * error. */

int hash_remove(hash *h, const char *key);
/* Remove a value in the hash.  Returns 0 on success, -1 on error. */

void *hash_find(hash *h, const char *key);
/* Find a value in the hash.  Returns a null pointer if not found. */

int hash_foreach(hash *h,
                 int (*callback)(const char *key, void *value, void *u),
                 void *u);
/* Visit all the elements in a hash in any old order.  It's safe to remove
 * items from inside the callback including the visited one.  It is not safe to
 * add items from inside the callback however.
 *
 * If the callback ever returns non-0 then that value is immediately returned.
 * Otherwise the return value is 0.
 */

size_t hash_count(hash *h);
/* Return the number of items in the hash */

char **hash_keys(hash *h);
/* Return all the keys of H */

#endif /* HASH_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
