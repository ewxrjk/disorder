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

int main(void) {
  mem_init();
  fail_first = !!getenv("FAIL_FIRST");
  insist('\n' == 0x0A);
  insist('\r' == 0x0D);
  insist(' ' == 0x20);
  insist('0' == 0x30);
  insist('9' == 0x39);
  insist('A' == 0x41);
  insist('Z' == 0x5A);
  insist('a' == 0x61);
  insist('z' == 0x7A);
  /* addr.c */
  test_addr();
  /* asprintf.c */
  /* authhash.c */
  /* basen.c */
  test_basen();
  /* charset.c */
  /* client.c */
  /* configuration.c */
  /* event.c */
  /* filepart.c */
  test_filepart();
  /* fprintf.c */
  /* heap.c */
  test_heap();
  /* hex.c */
  test_hex();
  /* inputline.c */
  /* kvp.c */
  test_kvp();
  /* log.c */
  /* mem.c */
  /* mime.c */
  test_mime();
  test_cookies();
  /* mixer.c */
  /* plugin.c */
  /* printf.c */
  test_printf();
  /* queue.c */
  /* sink.c */
  test_sink();
  /* snprintf.c */
  /* split.c */
  test_split();
  /* syscalls.c */
  /* table.c */
  /* unicode.c */
  test_unicode();
  /* utf8.c */
  test_utf8();
  /* vector.c */
  /* words.c */
  test_casefold();
  test_words();
  /* wstat.c */
  test_wstat();
  /* signame.c */
  test_signame();
  /* cache.c */
  test_cache();
  /* selection.c */
  test_selection();
  test_hash();
  test_url();
  test_regsub();
  test_bits();
  test_vector();
  test_syscalls();
  test_trackname();
  fprintf(stderr,  "%lld errors out of %lld tests\n", errors, tests);
  return !!errors;
}
  
/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
