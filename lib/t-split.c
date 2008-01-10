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

void test_split(void) {
  char **v;
  int nv;

  fprintf(stderr, "test_split\n");
  insist(split("\"misquoted", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0) == 0);
  insist(split("\'misquoted", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0) == 0);
  insist(split("\'misquoted\\", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0) == 0);
  insist(split("\'misquoted\\\"", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0) == 0);
  insist(split("\'mis\\escaped\'", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0) == 0);

  insist((v = split("", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 0);
  insist(*v == 0);

  insist((v = split("wibble", &nv, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 1);
  check_string(v[0], "wibble");
  insist(v[1] == 0);

  insist((v = split("   wibble \t\r\n wobble   ", &nv,
                    SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 2);
  check_string(v[0], "wibble");
  check_string(v[1], "wobble");
  insist(v[2] == 0);

  insist((v = split("wibble wobble #splat", &nv,
                    SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 2);
  check_string(v[0], "wibble");
  check_string(v[1], "wobble");
  insist(v[2] == 0);

  insist((v = split("\"wibble wobble\" #splat", &nv,
                    SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 1);
  check_string(v[0], "wibble wobble");
  insist(v[1] == 0);

  insist((v = split("\"wibble \\\"\\nwobble\"", &nv,
                    SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 1);
  check_string(v[0], "wibble \"\nwobble");
  insist(v[1] == 0);

  insist((v = split("\"wibble wobble\" #splat", &nv,
                    SPLIT_QUOTES, 0, 0)));
  check_integer(nv, 2);
  check_string(v[0], "wibble wobble");
  check_string(v[1], "#splat");
  insist(v[2] == 0);

  insist((v = split("\"wibble wobble\" #splat", &nv,
                    SPLIT_COMMENTS, 0, 0)));
  check_integer(nv, 2);
  check_string(v[0], "\"wibble");
  check_string(v[1], "wobble\"");
  insist(v[2] == 0);

  check_string(quoteutf8("wibble"), "wibble");
  check_string(quoteutf8("  wibble  "), "\"  wibble  \"");
  check_string(quoteutf8("wibble wobble"), "\"wibble wobble\"");
  check_string(quoteutf8("wibble\"wobble"), "\"wibble\\\"wobble\"");
  check_string(quoteutf8("wibble\nwobble"), "\"wibble\\nwobble\"");
  check_string(quoteutf8("wibble\\wobble"), "\"wibble\\\\wobble\"");
  check_string(quoteutf8("wibble'wobble"), "\"wibble'wobble\"");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
