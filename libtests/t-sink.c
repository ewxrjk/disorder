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

static void test_sink(void) {
  struct sink *s;
  struct dynstr d[1];
  FILE *fp;
  char *l;

  fp = tmpfile();
  assert(fp != 0);
  s = sink_stdio("tmpfile", fp);
  insist(sink_printf(s, "test: %d\n", 999) == 10);
  insist(sink_printf(s, "wibble: %s\n", "foobar") == 15);
  rewind(fp);
  insist(inputline("tmpfile", fp, &l, '\n') == 0);
  check_string(l, "test: 999");
  insist(inputline("tmpfile", fp, &l, '\n') == 0);
  check_string(l, "wibble: foobar");
  insist(inputline("tmpfile", fp, &l, '\n') == -1);

  fp = tmpfile();
  assert(fp != 0);
  fprintf(fp, "foo\rbar\nwibble\r\n");
  fprintf(fp, "second\n\rspong\r\n");
  rewind(fp);
  insist(inputline("tmpfile", fp, &l, CRLF) == 0);
  check_string(l, "foo\rbar\nwibble");
  insist(inputline("tmpfile", fp, &l, CRLF) == 0);
  check_string(l, "second\n\rspong");
  insist(inputline("tmpfile", fp, &l, CRLF) == -1);
  
  dynstr_init(d);
  s = sink_dynstr(d);
  insist(sink_printf(s, "test: %d\n", 999) == 10);
  insist(sink_printf(s, "wibble: %s\n", "foobar") == 15);
  dynstr_terminate(d);
  check_string(d->vec, "test: 999\nwibble: foobar\n");
}

TEST(sink);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
