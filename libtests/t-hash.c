/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008, 2010 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "test.h"

static int count;

static int test_hash_callback(const char attribute((unused)) *key,
                              void attribute((unused)) *value,
                              void attribute((unused)) *u) {
  if(u)
    insist(count < 100);
  ++count;
  if(u)
    return count >= 100 ? 99 : 0;
  else
    return 0;
}

static void test_hash(void) {
  hash *h;
  int i, *ip;
  char **keys;

  h = hash_new(sizeof(int));
  for(i = 0; i < 10000; ++i)
    insist(hash_add(h, do_printf("%d", i), &i, HASH_INSERT) == 0);
  check_integer(hash_count(h), 10000);
  i = hash_foreach(h, test_hash_callback, NULL);
  check_integer(i, 0);
  check_integer(count, 10000);
  count = 0;
  i = hash_foreach(h, test_hash_callback, h);
  check_integer(i, 99);
  check_integer(count, 100);
  
  for(i = 0; i < 10000; ++i) {
    insist((ip = hash_find(h, do_printf("%d", i))) != 0);
    if(ip) {
      check_integer(*ip, i);
      insist(hash_add(h, do_printf("%d", i), &i, HASH_REPLACE) == 0);
    }
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
