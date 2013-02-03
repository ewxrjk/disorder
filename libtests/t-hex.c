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

static void test_hex(void) {
  unsigned n;
  static const unsigned char h[] = { 0x00, 0xFF, 0x80, 0x7F };
  uint8_t *u;
  size_t ul;

  for(n = 0; n <= UCHAR_MAX; ++n) {
    if(!isxdigit(n))
      insist(unhexdigitq(n) == -1);
  }
  insist(unhexdigitq('0') == 0);
  insist(unhexdigitq('1') == 1);
  insist(unhexdigitq('2') == 2);
  insist(unhexdigitq('3') == 3);
  insist(unhexdigitq('4') == 4);
  insist(unhexdigitq('5') == 5);
  insist(unhexdigitq('6') == 6);
  insist(unhexdigitq('7') == 7);
  insist(unhexdigitq('8') == 8);
  insist(unhexdigitq('9') == 9);
  insist(unhexdigitq('a') == 10);
  insist(unhexdigitq('b') == 11);
  insist(unhexdigitq('c') == 12);
  insist(unhexdigitq('d') == 13);
  insist(unhexdigitq('e') == 14);
  insist(unhexdigitq('f') == 15);
  insist(unhexdigitq('A') == 10);
  insist(unhexdigitq('B') == 11);
  insist(unhexdigitq('C') == 12);
  insist(unhexdigitq('D') == 13);
  insist(unhexdigitq('E') == 14);
  insist(unhexdigitq('F') == 15);
  check_string(hex(h, sizeof h), "00ff807f");
  check_string(hex(0, 0), "");
  u = unhex("00ff807f", &ul);
  insist(u != 0);
  insist(ul == 4);
  insist(memcmp(u, h, 4) == 0);
  u = unhex("00FF807F", &ul);
  insist(u != 0);
  insist(ul == 4);
  insist(memcmp(u, h, 4) == 0);
  u = unhex("", &ul);
  insist(u != 0);
  insist(ul == 0);
  fprintf(stderr, "2 ERROR reports expected {\n");
  insist(unhex("F", 0) == 0);
  insist(unhex("az", 0) == 0);
  fprintf(stderr, "}\n");
}

TEST(hex);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
