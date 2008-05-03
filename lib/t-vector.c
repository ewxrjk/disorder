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

static void test_vector(void) {
  struct vector v[1];
  static const char *s[10] = { "three", "four", "five", "six", "seven",
			       "eight", "nine", "ten", "eleven", "twelve" };
  int n;
  
  vector_init(v);
  insist(v->nvec == 0);

  vector_append(v, (char *)"one");
  insist(v->nvec == 1);
  insist(!strcmp(v->vec[0], "one"));

  vector_append(v, (char *)"two");
  insist(v->nvec == 2);
  insist(!strcmp(v->vec[0], "one"));
  insist(!strcmp(v->vec[1], "two"));

  vector_terminate(v);
  insist(v->nvec == 2);
  insist(!strcmp(v->vec[0], "one"));
  insist(!strcmp(v->vec[1], "two"));
  insist(v->vec[2] == 0);

  vector_append_many(v, (char **)s, 10);
  insist(v->nvec == 12);
  insist(!strcmp(v->vec[0], "one"));
  insist(!strcmp(v->vec[1], "two"));
  for(n = 0; n < 10; ++n)
    insist(!strcmp(v->vec[n+2], s[n]));
  vector_terminate(v);
  insist(v->nvec == 12);
  insist(v->vec[12] == 0);
}

TEST(vector);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
