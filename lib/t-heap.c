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

/** @brief Less-than comparison function for integer heap */
static inline int int_lt(int a, int b) { return a < b; }

/** @struct iheap
 * @brief A heap with @c int elements */
HEAP_TYPE(iheap, int, int_lt);
HEAP_DEFINE(iheap, int, int_lt);

/** @brief Tests for @ref heap.h */
void test_heap(void) {
  struct iheap h[1];
  int n;
  int last = -1;

  fprintf(stderr, "test_heap\n");

  iheap_init(h);
  for(n = 0; n < 1000; ++n)
    iheap_insert(h, random() % 100);
  for(n = 0; n < 1000; ++n) {
    const int latest = iheap_remove(h);
    if(last > latest)
      fprintf(stderr, "should have %d <= %d\n", last, latest);
    insist(last <= latest);
    last = latest;
  }
  putchar('\n');
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
