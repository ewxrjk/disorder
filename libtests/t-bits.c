/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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
#include "bits.h"

static void test_bits(void) {
  int n;
  
  check_integer(leftmost_bit(0), -1);
  check_integer(leftmost_bit(0x80000000), 31);
  check_integer(leftmost_bit(0xffffffff), 31);
  for(n = 0; n < 28; ++n) {
    const uint32_t b = 1 << n, limit = 2 * b;
    uint32_t v;

    for(v = b; v < limit; ++v)
      check_integer(leftmost_bit(v), n);
  }
}

TEST(bits);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
