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
/** @file lib/test.c @brief Library tests */

#include "test.h"

long long tests, errors;
int fail_first;

void count_error(void) {
  ++errors;
  if(fail_first)
    abort();
}

const char *format(const char *s) {
  struct dynstr d;
  int c;
  char buf[10];
  
  dynstr_init(&d);
  while((c = (unsigned char)*s++)) {
    if(c >= ' ' && c <= '~')
      dynstr_append(&d, c);
    else {
      sprintf(buf, "\\x%02X", (unsigned)c);
      dynstr_append_string(&d, buf);
    }
  }
  dynstr_terminate(&d);
  return d.vec;
}

const char *format_utf32(const uint32_t *s) {
  struct dynstr d;
  uint32_t c;
  char buf[64];
  
  dynstr_init(&d);
  while((c = *s++)) {
    sprintf(buf, " %04lX", (long)c);
    dynstr_append_string(&d, buf);
  }
  dynstr_terminate(&d);
  return d.vec;
}

uint32_t *ucs4parse(const char *s) {
  struct dynstr_ucs4 d;
  char *e;

  dynstr_ucs4_init(&d);
  while(*s) {
    errno = 0;
    dynstr_ucs4_append(&d, strtoul(s, &e, 0));
    if(errno) fatal(errno, "strtoul (%s)", s);
    s = e;
  }
  dynstr_ucs4_terminate(&d);
  return d.vec;
}

const char *do_printf(const char *fmt, ...) {
  va_list ap;
  char *s;
  int rc;

  va_start(ap, fmt);
  rc = byte_vasprintf(&s, fmt, ap);
  va_end(ap);
  if(rc < 0)
    return 0;
  return s;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
