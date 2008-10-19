/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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

static void test_cache(void) {
  const struct cache_type t1 = { 1 }, t2 = { 10 };
  const char v11[] = "spong", v12[] = "wibble", v2[] = "blat";

  cache_put(&t1, "1_1", v11);
  cache_put(&t1, "1_2", v12);
  cache_put(&t2, "2", v2);
  insist(cache_count() == 3);
  insist(cache_get(&t2, "2") == v2);
  insist(cache_get(&t1, "1_1") == v11);
  insist(cache_get(&t1, "1_2") == v12);
  insist(cache_get(&t1, "2") == 0);
  insist(cache_get(&t2, "1_1") == 0);
  insist(cache_get(&t2, "1_2") == 0);
  insist(cache_get(&t1, "2") == 0);
  insist(cache_get(&t2, "1_1") == 0);
  insist(cache_get(&t2, "1_2") == 0);
  sleep(2);
  cache_expire();
  insist(cache_count() == 1);
  insist(cache_get(&t1, "1_1") == 0);
  insist(cache_get(&t1, "1_2") == 0);
  insist(cache_get(&t2, "2") == v2);
  cache_clean(0);
  insist(cache_count() == 0);
  insist(cache_get(&t2, "2") == 0); 
}

TEST(cache);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
