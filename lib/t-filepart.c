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

void test_filepart(void) {
  fprintf(stderr, "test_filepart\n");
  check_string(d_dirname("/"), "/");
  check_string(d_dirname("////"), "/");
  check_string(d_dirname("/spong"), "/");
  check_string(d_dirname("////spong"), "/");
  check_string(d_dirname("/foo/bar"), "/foo");
  check_string(d_dirname("////foo/////bar"), "////foo");
  check_string(d_dirname("./bar"), ".");
  check_string(d_dirname(".//bar"), ".");
  check_string(d_dirname("."), ".");
  check_string(d_dirname(".."), ".");
  check_string(d_dirname("../blat"), "..");
  check_string(d_dirname("..//blat"), "..");
  check_string(d_dirname("wibble"), ".");
  check_string(extension("foo.c"), ".c");
  check_string(extension(".c"), ".c");
  check_string(extension("."), ".");
  check_string(extension("foo"), "");
  check_string(extension("./foo"), "");
  check_string(extension("./foo.c"), ".c");
  check_string(strip_extension("foo.c"), "foo");
  check_string(strip_extension("foo.mp3"), "foo");
  check_string(strip_extension("foo.---"), "foo.---");
  check_string(strip_extension("foo.---xyz"), "foo.---xyz");
  check_string(strip_extension("foo.bar/wibble.spong"), "foo.bar/wibble");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
