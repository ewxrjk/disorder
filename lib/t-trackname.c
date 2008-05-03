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
#include "trackname.h"

#define CHECK_PATH_ORDER(A,B,EXPECTED) do {			\
  const unsigned char a[] = A, b[] = B;				\
  insist(compare_path_raw(a, (sizeof a) - 1,			\
			  b, (sizeof b) - 1) == (EXPECTED));	\
  insist(compare_path_raw(b, (sizeof b) - 1,			\
			  a, (sizeof a) - 1) == -(EXPECTED));	\
} while(0)

static void test_trackname(void) {
  CHECK_PATH_ORDER("/a/b", "/aa/", -1);
  CHECK_PATH_ORDER("/a/b", "/a", 1);
  CHECK_PATH_ORDER("/ab", "/a", 1);
  CHECK_PATH_ORDER("/ab", "/aa", 1);
  CHECK_PATH_ORDER("/aa", "/aa", 0);
  CHECK_PATH_ORDER("/", "/", 0);
}

TEST(trackname);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
