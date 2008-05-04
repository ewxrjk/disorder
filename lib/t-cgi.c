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
#include "cgi.h"

static void input_from(const char *s) {
  FILE *fp = tmpfile();
  char buffer[64];

  if(fputs(s, fp) < 0 || fflush(fp) < 0)
    fatal(errno, "writing to temporary file");
  rewind(fp);
  xdup2(fileno(fp), 0);
  lseek(0, 0/*offset*/, SEEK_SET);
  snprintf(buffer, sizeof buffer, "%zu", strlen(s));
  setenv("CONTENT_LENGTH", buffer, 1);
}

static void test_cgi(void) {
  struct dynstr d[1];

  setenv("REQUEST_METHOD", "GET", 1);
  setenv("QUERY_STRING", "foo=bar&a=b+c&c=x%7ey", 1);
  cgi_init();
  check_string(cgi_get("foo"), "bar");
  check_string(cgi_get("a"), "b c");
  check_string(cgi_get("c"), "x~y");

  setenv("REQUEST_METHOD", "POST", 1);
  unsetenv("QUERY_STRING");
  input_from("foo=xbar&a=xb+c&c=xx%7ey");
  cgi_init();
  check_string(cgi_get("foo"), "xbar");
  check_string(cgi_get("a"), "xb c");
  check_string(cgi_get("c"), "xx~y");

  /* TODO multipart/form-data */

  check_string(cgi_sgmlquote("foobar"), "foobar");
  check_string(cgi_sgmlquote("<wibble>"), "&#60;wibble&#62;");
  check_string(cgi_sgmlquote("\"&\""), "&#34;&#38;&#34;");
  check_string(cgi_sgmlquote("\xC2\xA3"), "&#163;");

  dynstr_init(d);
  cgi_opentag(sink_dynstr(d), "element",
	      "foo", "bar",
	      "foo", "has space",
	      "foo", "has \"quotes\"",
	      (char *)NULL);
  dynstr_terminate(d);
  check_string(d->vec, "<element foo=bar foo=\"has space\" foo=\"has &#34;quotes&#34;\">");
  
  dynstr_init(d);
  cgi_opentag(sink_dynstr(d), "element",
	      "foo", (char *)NULL,
	      (char *)NULL);
  dynstr_terminate(d);
  check_string(d->vec, "<element foo>");
  
  dynstr_init(d);
  cgi_closetag(sink_dynstr(d), "element");
  dynstr_terminate(d);
  check_string(d->vec, "</element>");

  check_string(cgi_makeurl("http://example.com/", (char *)NULL),
	       "http://example.com/");
  check_string(cgi_makeurl("http://example.com/",
			   "foo", "bar",
			   "a", "b c",
			   "d", "f=g+h",
			   (char *)NULL),
	       "http://example.com/?foo=bar&a=b%20c&d=f%3dg%2bh");
  
}

TEST(cgi);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
