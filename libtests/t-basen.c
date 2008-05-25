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

static void test_basen(void) {
  unsigned long v[64];
  char buffer[1024];

  v[0] = 999;
  insist(basen(v, 1, buffer, sizeof buffer, 10) == 0);
  check_string(buffer, "999");

  v[0] = 1+2*7+3*7*7+4*7*7*7;
  insist(basen(v, 1, buffer, sizeof buffer, 7) == 0);
  check_string(buffer, "4321");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, sizeof buffer, 256) == 0);
  check_string(buffer, "123456789abcdef");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, sizeof buffer, 16) == 0);
  check_string(buffer, "102030405060708090a0b0c0d0e0f");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, 10, 16) == -1);
}

TEST(basen);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
