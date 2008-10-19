
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

#define check_filepart(PATH, DIR, BASE) do {            \
  char *d = d_dirname(PATH), *b = d_basename(PATH);     \
                                                        \
  if(strcmp(d, DIR)) {                                  \
    fprintf(stderr, "%s:%d: d_dirname failed:\n"        \
            "    path: %s\n"                            \
            "     got: %s\n"                            \
            "expected: %s\n",                           \
            __FILE__, __LINE__,                         \
            PATH, d, DIR);                              \
    count_error();                                      \
  }                                                     \
  if(strcmp(b, BASE)) {                                 \
    fprintf(stderr, "%s:%d: d_basename failed:\n"       \
            "    path: %s\n"                            \
            "     got: %s\n"                            \
            "expected: %s\n",                           \
            __FILE__, __LINE__,                         \
            PATH, d, DIR);                              \
    count_error();                                      \
  }                                                     \
} while(0)

static void test_filepart(void) {
  check_filepart("", "", "");
  check_filepart("/", "/", "/");
  check_filepart("////", "/", "/");
  check_filepart("/spong", "/", "spong");
  check_filepart("/spong/", "/", "spong");
  check_filepart("/spong//", "/", "spong");
  check_filepart("////spong", "/", "spong");
  check_filepart("/foo/bar", "/foo", "bar");
  check_filepart("/foo/bar/", "/foo", "bar");
  check_filepart("////foo/////bar", "////foo", "bar");
  check_filepart("./bar", ".", "bar");
  check_filepart(".//bar", ".", "bar");
  check_filepart(".", ".", ".");
  check_filepart("..", ".", "..");
  check_filepart("../blat", "..", "blat");
  check_filepart("..//blat", "..", "blat");
  check_filepart("wibble", ".", "wibble");
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

TEST(filepart);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
