/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
#include "test.h"

static void test_hash(void) {
  hash *h;
  int i, *ip;
  char **keys;

  h = hash_new(sizeof(int));
  for(i = 0; i < 10000; ++i)
    insist(hash_add(h, do_printf("%d", i), &i, HASH_INSERT) == 0);
  check_integer(hash_count(h), 10000);
  for(i = 0; i < 10000; ++i) {
    insist((ip = hash_find(h, do_printf("%d", i))) != 0);
    check_integer(*ip, i);
    insist(hash_add(h, do_printf("%d", i), &i, HASH_REPLACE) == 0);
  }
  check_integer(hash_count(h), 10000);
  keys = hash_keys(h);
  for(i = 0; i < 10000; ++i)
    insist(keys[i] != 0);
  insist(keys[10000] == 0);
  for(i = 0; i < 10000; ++i)
    insist(hash_remove(h, do_printf("%d", i)) == 0);
  check_integer(hash_count(h), 0);
}

TEST(hash);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
