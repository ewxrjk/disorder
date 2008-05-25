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
#include "charset.h"

static void test_charset(void) {
  check_string(any2any("UTF-8", "ISO-8859-1", "\xC2\xA3"),
	       "\xA3");
  check_string(any2any("ISO-8859-1", "UTF-8", "\xA3"),
	       "\xC2\xA3");
  fprintf(stderr, "Expect a conversion error:\n");
  insist(any2any("UTF-8", "ISO-8859-1", "\xC2""a") == 0);

#define EL "\xE2\x80\xA6"	/* 2026 HORIZONTAL ELLIPSIS */
  check_string(truncate_for_display("", 0), "");
  check_string(truncate_for_display("", 1), "");
  check_string(truncate_for_display("x", 1), "x");
  check_string(truncate_for_display("xx", 1), EL);
  check_string(truncate_for_display("xx", 2), "xx");
  check_string(truncate_for_display("xxx", 2), "x"EL);
  check_string(truncate_for_display("wibble", 6), "wibble");
  check_string(truncate_for_display("wibble", 5), "wibb"EL);
}

TEST(charset);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
