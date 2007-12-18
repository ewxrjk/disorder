/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "charset.h"
#include "mime.h"
#include "hex.h"
#include "heap.h"
#include "unicode.h"
#include "inputline.h"
#include "wstat.h"
#include "signame.h"
#include "cache.h"
#include "filepart.h"
#include "hash.h"
#include "selection.h"
#include "syscalls.h"
#include "kvp.h"
#include "sink.h"
#include "printf.h"
#include "basen.h"
#include "split.h"
#include "configuration.h"
#include "addr.h"

static int tests, errors;
static int fail_first;

static void count_error() {
  ++errors;
  if(fail_first)
    abort();
}

/** @brief Checks that @p expr is nonzero */
#define insist(expr) do {				\
  if(!(expr)) {						\
    count_error();						\
    fprintf(stderr, "%s:%d: error checking %s\n",	\
            __FILE__, __LINE__, #expr);			\
  }							\
  ++tests;						\
} while(0)

static const char *format(const char *s) {
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

static const char *format_utf32(const uint32_t *s) {
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

#define check_string(GOT, WANT) do {                                    \
  const char *got = GOT;                                                \
  const char *want = WANT;                                              \
                                                                        \
  if(want == 0) {                                                       \
    fprintf(stderr, "%s:%d: %s returned 0\n",                           \
            __FILE__, __LINE__, #GOT);                                  \
    count_error();                                                      \
  } else if(strcmp(want, got)) {                                        \
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s\n",         \
	    __FILE__, __LINE__, #GOT, format(got), format(want));       \
    count_error();                                                      \
  }                                                                     \
  ++tests;                                                              \
 } while(0)

#define check_string_prefix(GOT, WANT) do {                             \
  const char *got = GOT;                                                \
  const char *want = WANT;                                              \
                                                                        \
  if(want == 0) {                                                       \
    fprintf(stderr, "%s:%d: %s returned 0\n",                           \
            __FILE__, __LINE__, #GOT);                                  \
    count_error();                                                      \
  } else if(strncmp(want, got, strlen(want))) {                         \
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s...\n",      \
	    __FILE__, __LINE__, #GOT, format(got), format(want));       \
    count_error();                                                      \
  }                                                                     \
  ++tests;                                                              \
 } while(0)

#define check_integer(GOT, WANT) do {                           \
  const intmax_t got = GOT, want = WANT;                        \
  if(got != want) {                                             \
    fprintf(stderr, "%s:%d: %s returned: %jd  expected: %jd\n", \
            __FILE__, __LINE__, #GOT, got, want);               \
    count_error();                                              \
  }                                                             \
  ++tests;                                                      \
} while(0)

static uint32_t *ucs4parse(const char *s) {
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

static void test_utf8(void) {
  /* Test validutf8, convert to UCS-4, check the answer is right,
   * convert back to UTF-8, check we got to where we started */
#define U8(CHARS, WORDS) do {			\
  uint32_t *w = ucs4parse(WORDS);		\
  uint32_t *ucs;				\
  char *u8;					\
						\
  insist(validutf8(CHARS));			\
  ucs = utf8_to_utf32(CHARS, strlen(CHARS), 0); \
  insist(ucs != 0);				\
  insist(!utf32_cmp(w, ucs));			\
  u8 = utf32_to_utf8(ucs, utf32_len(ucs), 0);   \
  insist(u8 != 0);				\
  check_string(u8, CHARS);			\
} while(0)

  fprintf(stderr, "test_utf8\n");
#define validutf8(S) utf8_valid((S), strlen(S))

  /* empty string */

  U8("", "");
  
  /* ASCII characters */

  U8(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
     "0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x2a 0x2b 0x2c 0x2d "
     "0x2e 0x2f 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39 0x3a "
     "0x3b 0x3c 0x3d 0x3e 0x3f 0x40 0x41 0x42 0x43 0x44 0x45 0x46 0x47 "
     "0x48 0x49 0x4a 0x4b 0x4c 0x4d 0x4e 0x4f 0x50 0x51 0x52 0x53 0x54 "
     "0x55 0x56 0x57 0x58 0x59 0x5a 0x5b 0x5c 0x5d 0x5e 0x5f 0x60 0x61 "
     "0x62 0x63 0x64 0x65 0x66 0x67 0x68 0x69 0x6a 0x6b 0x6c 0x6d 0x6e "
     "0x6f 0x70 0x71 0x72 0x73 0x74 0x75 0x76 0x77 0x78 0x79 0x7a 0x7b "
     "0x7c 0x7d 0x7e");
  U8("\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\177",
     "0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xa 0xb 0xc 0xd 0xe 0xf 0x10 "
     "0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1a 0x1b 0x1c 0x1d "
     "0x1e 0x1f 0x7f");

  /* from RFC3629 */

  /* UTF8-2      = %xC2-DF UTF8-tail */
  insist(!validutf8("\xC0\x80"));
  insist(!validutf8("\xC1\x80"));
  insist(!validutf8("\xC2\x7F"));
  U8("\xC2\x80", "0x80");
  U8("\xDF\xBF", "0x7FF");
  insist(!validutf8("\xDF\xC0"));

  /*  UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
   *                %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
   */
  insist(!validutf8("\xE0\x9F\x80"));
  U8("\xE0\xA0\x80", "0x800");
  U8("\xE0\xBF\xBF", "0xFFF");
  insist(!validutf8("\xE0\xC0\xBF"));

  insist(!validutf8("\xE1\x80\x7F"));
  U8("\xE1\x80\x80", "0x1000");
  U8("\xEC\xBF\xBF", "0xCFFF");
  insist(!validutf8("\xEC\xC0\xBF"));
  
  U8("\xED\x80\x80", "0xD000");
  U8("\xED\x9F\xBF", "0xD7FF");
  insist(!validutf8("\xED\xA0\xBF"));

  insist(!validutf8("\xEE\x7f\x80"));
  U8("\xEE\x80\x80", "0xE000");
  U8("\xEF\xBF\xBF", "0xFFFF");
  insist(!validutf8("\xEF\xC0\xBF"));

  /*  UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
   *                %xF4 %x80-8F 2( UTF8-tail )
   */
  insist(!validutf8("\xF0\x8F\x80\x80"));
  U8("\xF0\x90\x80\x80", "0x10000");
  U8("\xF0\xBF\xBF\xBF", "0x3FFFF");
  insist(!validutf8("\xF0\xC0\x80\x80"));

  insist(!validutf8("\xF1\x80\x80\x7F"));
  U8("\xF1\x80\x80\x80", "0x40000");
  U8("\xF3\xBF\xBF\xBF", "0xFFFFF");
  insist(!validutf8("\xF3\xC0\x80\x80"));

  insist(!validutf8("\xF4\x80\x80\x7F"));
  U8("\xF4\x80\x80\x80", "0x100000");
  U8("\xF4\x8F\xBF\xBF", "0x10FFFF");
  insist(!validutf8("\xF4\x90\x80\x80"));
  insist(!validutf8("\xF4\x80\xFF\x80"));

  /* miscellaneous non-UTF-8 rubbish */
  insist(!validutf8("\x80"));
  insist(!validutf8("\xBF"));
  insist(!validutf8("\xC0"));
  insist(!validutf8("\xC0\x7F"));
  insist(!validutf8("\xC0\xC0"));
  insist(!validutf8("\xE0"));
  insist(!validutf8("\xE0\x7F"));
  insist(!validutf8("\xE0\xC0"));
  insist(!validutf8("\xE0\x80"));
  insist(!validutf8("\xE0\x80\x7f"));
  insist(!validutf8("\xE0\x80\xC0"));
  insist(!validutf8("\xF0"));
  insist(!validutf8("\xF0\x7F"));
  insist(!validutf8("\xF0\xC0"));
  insist(!validutf8("\xF0\x80"));
  insist(!validutf8("\xF0\x80\x7f"));
  insist(!validutf8("\xF0\x80\xC0"));
  insist(!validutf8("\xF0\x80\x80\x7f"));
  insist(!validutf8("\xF0\x80\x80\xC0"));
  insist(!validutf8("\xF5\x80\x80\x80"));
  insist(!validutf8("\xF8"));
}

static int test_multipart_callback(const char *s, void *u) {
  struct vector *parts = u;

  vector_append(parts, (char *)s);
  return 0;
}

static void test_mime(void) {
  char *t, *n, *v;
  struct vector parts[1];

  fprintf(stderr, "test_mime\n");

  t = n = v = 0;
  insist(!mime_content_type("text/plain", &t, &n, &v));
  check_string(t, "text/plain");
  insist(n == 0);
  insist(v == 0);

  insist(mime_content_type("TEXT ((broken) comment", &t, &n, &v) < 0);
  insist(mime_content_type("TEXT ((broken) comment\\", &t, &n, &v) < 0);
  
  t = n = v = 0;
  insist(!mime_content_type("TEXT ((nested)\\ comment) /plain", &t, &n, &v));
  check_string(t, "text/plain");
  insist(n == 0);
  insist(v == 0);

  t = n = v = 0;
  insist(!mime_content_type(" text/plain ; Charset=\"utf-\\8\"", &t, &n, &v));
  check_string(t, "text/plain");
  check_string(n, "charset");
  check_string(v, "utf-8");

  t = n = v = 0;
  insist(!mime_content_type("text/plain;charset = ISO-8859-1 ", &t, &n, &v));
  check_string(t, "text/plain");
  check_string(n, "charset");
  check_string(v, "ISO-8859-1");

  t = n = v = 0;
  insist(!mime_rfc2388_content_disposition("form-data; name=\"field1\"", &t, &n, &v));
  check_string(t, "form-data");
  check_string(n, "name");
  check_string(v, "field1");

  insist(!mime_rfc2388_content_disposition("inline", &t, &n, &v));
  check_string(t, "inline");
  insist(n == 0);
  insist(v == 0);

  /* Current versions of the code only understand a single arg to these
   * headers.  This is a bug at the level they work at but suffices for
   * DisOrder's current purposes. */

  insist(!mime_rfc2388_content_disposition(
              "attachment; filename=genome.jpeg;\n"
              "modification-date=\"Wed, 12 Feb 1997 16:29:51 -0500\"",
         &t, &n, &v));
  check_string(t, "attachment");
  check_string(n, "filename");
  check_string(v, "genome.jpeg");

  vector_init(parts);
  insist(mime_multipart("--outer\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-1\r\n"
                        "\r\n"
                        "Some text goes here\r\n"
                        "\r\n"
                        "--outer\r\n"
                        "Content-Type: multipart/mixed; boundary=inner\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: multipart-2\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-2\r\n"
                        "\r\n"
                        "Some more text here.\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: jpeg-1\r\n"
                        "\r\n"
                        "<jpeg data>\r\n"
                        "--inner--\r\n"
                        "--outer--\r\n",
                        test_multipart_callback,
                        "outer",
                        parts) == 0);
  check_integer(parts->nvec, 2);
  check_string(parts->vec[0],
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-1\r\n"
               "\r\n"
               "Some text goes here\r\n");
  check_string(parts->vec[1],
               "Content-Type: multipart/mixed; boundary=inner\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: multipart-2\r\n"
               "\r\n"
               "--inner\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-2\r\n"
               "\r\n"
               "Some more text here.\r\n"
               "\r\n"
               "--inner\r\n"
               "Content-Type: image/jpeg\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: jpeg-1\r\n"
               "\r\n"
               "<jpeg data>\r\n"
               "--inner--");
  /* No trailing CRLF is _correct_ - see RFC2046 5.1.1 note regarding CRLF
   * preceding the boundary delimiter line.  An implication of this is that we
   * must cope with partial lines at the end of the input when recursively
   * decomposing a multipart message. */
  vector_init(parts);
  insist(mime_multipart("--inner\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-2\r\n"
                        "\r\n"
                        "Some more text here.\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: jpeg-1\r\n"
                        "\r\n"
                        "<jpeg data>\r\n"
                        "--inner--",
                        test_multipart_callback,
                        "inner",
                        parts) == 0);
  check_integer(parts->nvec, 2);
  check_string(parts->vec[0],
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-2\r\n"
               "\r\n"
               "Some more text here.\r\n");
  check_string(parts->vec[1],
               "Content-Type: image/jpeg\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: jpeg-1\r\n"
               "\r\n"
               "<jpeg data>");
 
  /* XXX mime_parse */

  check_string(mime_qp(""), "");
  check_string(mime_qp("foobar"), "foobar");
  check_string(mime_qp("foo=20bar"), "foo bar");
  check_string(mime_qp("x \r\ny"), "x\r\ny");
  check_string(mime_qp("x=\r\ny"), "xy");
  check_string(mime_qp("x= \r\ny"), "xy");
  check_string(mime_qp("x =\r\ny"), "x y");
  check_string(mime_qp("x = \r\ny"), "x y");

  /* from RFC2045 */
  check_string(mime_qp("Now's the time =\r\n"
"for all folk to come=\r\n"
" to the aid of their country."),
	       "Now's the time for all folk to come to the aid of their country.");

  check_string(mime_base64(""),  "");
  check_string(mime_base64("BBBB"), "\x04\x10\x41");
  check_string(mime_base64("////"), "\xFF\xFF\xFF");
  check_string(mime_base64("//BB"), "\xFF\xF0\x41");
  check_string(mime_base64("BBBB//BB////"),
	       "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_string(mime_base64("B B B B  / / B B / / / /"),
	       "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_string(mime_base64("B\r\nBBB.// B-B//~//"),
	       "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_string(mime_base64("BBBB="),
	       "\x04\x10\x41");
  check_string(mime_base64("BBBBx="),	/* not actually valid base64 */
	       "\x04\x10\x41");
  check_string(mime_base64("BBBB BB=="),
	       "\x04\x10\x41" "\x04");
  check_string(mime_base64("BBBB BBB="),
	       "\x04\x10\x41" "\x04\x10");
}

static void test_cookies(void) {
  struct cookiedata cd[1];

  fprintf(stderr, "test_cookies\n");

  /* These are the examples from RFC2109 */
  insist(!parse_cookie("$Version=\"1\"; Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\"", cd));
  insist(!strcmp(cd->version, "1"));
  insist(cd->ncookies = 1);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  insist(!parse_cookie("$Version=\"1\";\n"
                       "Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\";\n"
                       "Part_Number=\"Rocket_Launcher_0001\"; $Path=\"/acme\"",
                       cd));
  insist(cd->ncookies = 2);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  insist(find_cookie(cd, "Part_Number") == &cd->cookies[1]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  check_string(cd->cookies[1].value, "Rocket_Launcher_0001");
  check_string(cd->cookies[1].path, "/acme");
  insist(cd->cookies[1].domain == 0);
  insist(!parse_cookie("$Version=\"1\";\n"
                       "Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\";\n"
                       "Part_Number=\"Rocket_Launcher_0001\"; $Path=\"/acme\";\n"
                       "Shipping=\"FedEx\"; $Path=\"/acme\"",
                       cd));
  insist(cd->ncookies = 3);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  insist(find_cookie(cd, "Part_Number") == &cd->cookies[1]);
  insist(find_cookie(cd, "Shipping") == &cd->cookies[2]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  check_string(cd->cookies[1].value, "Rocket_Launcher_0001");
  check_string(cd->cookies[1].path, "/acme");
  insist(cd->cookies[1].domain == 0);
  check_string(cd->cookies[2].value, "FedEx");
  check_string(cd->cookies[2].path, "/acme");
  insist(cd->cookies[2].domain == 0);
}

static void test_hex(void) {
  unsigned n;
  static const unsigned char h[] = { 0x00, 0xFF, 0x80, 0x7F };
  uint8_t *u;
  size_t ul;

  fprintf(stderr, "test_hex\n");

  for(n = 0; n <= UCHAR_MAX; ++n) {
    if(!isxdigit(n))
      insist(unhexdigitq(n) == -1);
  }
  insist(unhexdigitq('0') == 0);
  insist(unhexdigitq('1') == 1);
  insist(unhexdigitq('2') == 2);
  insist(unhexdigitq('3') == 3);
  insist(unhexdigitq('4') == 4);
  insist(unhexdigitq('5') == 5);
  insist(unhexdigitq('6') == 6);
  insist(unhexdigitq('7') == 7);
  insist(unhexdigitq('8') == 8);
  insist(unhexdigitq('9') == 9);
  insist(unhexdigitq('a') == 10);
  insist(unhexdigitq('b') == 11);
  insist(unhexdigitq('c') == 12);
  insist(unhexdigitq('d') == 13);
  insist(unhexdigitq('e') == 14);
  insist(unhexdigitq('f') == 15);
  insist(unhexdigitq('A') == 10);
  insist(unhexdigitq('B') == 11);
  insist(unhexdigitq('C') == 12);
  insist(unhexdigitq('D') == 13);
  insist(unhexdigitq('E') == 14);
  insist(unhexdigitq('F') == 15);
  check_string(hex(h, sizeof h), "00ff807f");
  check_string(hex(0, 0), "");
  u = unhex("00ff807f", &ul);
  insist(ul == 4);
  insist(memcmp(u, h, 4) == 0);
  u = unhex("00FF807F", &ul);
  insist(ul == 4);
  insist(memcmp(u, h, 4) == 0);
  u = unhex("", &ul);
  insist(ul == 0);
  fprintf(stderr, "2 ERROR reports expected {\n");
  insist(unhex("F", 0) == 0);
  insist(unhex("az", 0) == 0);
  fprintf(stderr, "}\n");
}

static void test_casefold(void) {
  uint32_t c, l;
  const char *input, *canon_folded, *compat_folded, *canon_expected, *compat_expected;

  fprintf(stderr, "test_casefold\n");

  /* This isn't a very exhaustive test.  Unlike for normalization, there don't
   * seem to be any public test vectors for these algorithms. */
  
  for(c = 1; c < 256; ++c) {
    input = utf32_to_utf8(&c, 1, 0);
    canon_folded = utf8_casefold_canon(input, strlen(input), 0);
    compat_folded = utf8_casefold_compat(input, strlen(input), 0);
    switch(c) {
    default:
      if((c >= 'A' && c <= 'Z')
	 || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
	l = c ^ 0x20;
      else
	l = c;
      break;
    case 0xB5:				/* MICRO SIGN */
      l = 0x3BC;			/* GREEK SMALL LETTER MU */
      break;
    case 0xDF:				/* LATIN SMALL LETTER SHARP S */
      check_string(canon_folded, "ss");
      check_string(compat_folded, "ss");
      l = 0;
      break;
    }
    if(l) {
      uint32_t *d;
      /* Case-folded data is now normalized */
      d = utf32_decompose_canon(&l, 1, 0);
      canon_expected = utf32_to_utf8(d, utf32_len(d), 0);
      if(strcmp(canon_folded, canon_expected)) {
	fprintf(stderr, "%s:%d: canon-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(canon_folded), format(canon_expected));
	count_error();
      }
      ++tests;
      d = utf32_decompose_compat(&l, 1, 0);
      compat_expected = utf32_to_utf8(d, utf32_len(d), 0);
      if(strcmp(compat_folded, compat_expected)) {
	fprintf(stderr, "%s:%d: compat-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(compat_folded), format(compat_expected));
	count_error();
      }
      ++tests;
    }
  }
  check_string(utf8_casefold_canon("", 0, 0), "");
}

struct {
  const char *in;
  const char *expect[10];
} wtest[] = {
  /* Empty string */
  { "", { 0 } },
  /* Only whitespace and punctuation */
  { "    ", { 0 } },
  { " '   ", { 0 } },
  { " !  ", { 0 } },
  { " \"\"  ", { 0 } },
  { " @  ", { 0 } },
  /* Basics */
  { "wibble", { "wibble", 0 } },
  { " wibble", { "wibble", 0 } },
  { " wibble ", { "wibble", 0 } },
  { "wibble ", { "wibble", 0 } },
  { "wibble spong", { "wibble", "spong", 0 } },
  { " wibble  spong", { "wibble", "spong", 0 } },
  { " wibble  spong   ", { "wibble", "spong", 0 } },
  { "wibble   spong  ", { "wibble", "spong", 0 } },
  { "wibble   spong splat foo zot  ", { "wibble", "spong", "splat", "foo", "zot", 0 } },
  /* Apostrophes */
  { "wibble 'spong", { "wibble", "spong", 0 } },
  { " wibble's", { "wibble's", 0 } },
  { " wibblespong'   ", { "wibblespong", 0 } },
  { "wibble   sp''ong  ", { "wibble", "sp", "ong", 0 } },
};
#define NWTEST (sizeof wtest / sizeof *wtest)

static void test_words(void) {
  size_t t, nexpect, ngot, i;
  int right;
  
  fprintf(stderr, "test_words\n");
  for(t = 0; t < NWTEST; ++t) {
    char **got = utf8_word_split(wtest[t].in, strlen(wtest[t].in), &ngot, 0);

    for(nexpect = 0; wtest[t].expect[nexpect]; ++nexpect)
      ;
    if(nexpect == ngot) {
      for(i = 0; i < ngot; ++i)
        if(strcmp(wtest[t].expect[i], got[i]))
          break;
      right = i == ngot;
    } else
      right = 0;
    if(!right) {
      fprintf(stderr, "word split %zu failed\n", t);
      fprintf(stderr, "input: %s\n", wtest[t].in);
      fprintf(stderr, "    | %-30s | %-30s\n",
              "expected", "got");
      for(i = 0; i < nexpect || i < ngot; ++i) {
        const char *e = i < nexpect ? wtest[t].expect[i] : "<none>";
        const char *g = i < ngot ? got[i] : "<none>";
        fprintf(stderr, " %2zu | %-30s | %-30s\n", i, e, g);
      }
      count_error();
    }
    ++tests;
  }
}

/** @brief Less-than comparison function for integer heap */
static inline int int_lt(int a, int b) { return a < b; }

/** @struct iheap
 * @brief A heap with @c int elements */
HEAP_TYPE(iheap, int, int_lt);
HEAP_DEFINE(iheap, int, int_lt);

/** @brief Tests for @ref heap.h */
static void test_heap(void) {
  struct iheap h[1];
  int n;
  int last = -1;

  fprintf(stderr, "test_heap\n");

  iheap_init(h);
  for(n = 0; n < 1000; ++n)
    iheap_insert(h, random() % 100);
  for(n = 0; n < 1000; ++n) {
    const int latest = iheap_remove(h);
    if(last > latest)
      fprintf(stderr, "should have %d <= %d\n", last, latest);
    insist(last <= latest);
    last = latest;
  }
  putchar('\n');
}

/** @brief Open a Unicode test file */
static FILE *open_unicode_test(const char *path) {
  const char *base;
  FILE *fp;
  char buffer[1024];
  int w;

  if((base = strrchr(path, '/')))
    ++base;
  else
    base = path;
  if(!(fp = fopen(base, "r"))) {
    snprintf(buffer, sizeof buffer,
             "wget http://www.unicode.org/Public/5.0.0/ucd/%s", path);
    if((w = system(buffer)))
      fatal(0, "%s: %s", buffer, wstat(w));
    if(chmod(base, 0444) < 0)
      fatal(errno, "chmod %s", base);
    if(!(fp = fopen(base, "r")))
      fatal(errno, "%s", base);
  }
  return fp;
}

/** @brief Run breaking tests for utf32_grapheme_boundary() etc */
static void breaktest(const char *path,
                      int (*breakfn)(const uint32_t *, size_t, size_t)) {
  FILE *fp = open_unicode_test(path);
  int lineno = 0;
  char *l, *lp;
  size_t bn, n;
  char break_allowed[1024];
  uint32_t buffer[1024];

  while(!inputline(path, fp, &l, '\n')) {
    ++lineno;
    if(l[0] == '#') continue;
    bn = 0;
    lp = l;
    while(*lp) {
      if(*lp == ' ' || *lp == '\t') {
        ++lp;
        continue;
      }
      if(*lp == '#')
        break;
      if((unsigned char)*lp == 0xC3 && (unsigned char)lp[1] == 0xB7) {
        /* 00F7 DIVISION SIGN */
        break_allowed[bn] = 1;
        lp += 2;
        continue;
      }
      if((unsigned char)*lp == 0xC3 && (unsigned char)lp[1] == 0x97) {
        /* 00D7 MULTIPLICATION SIGN */
        break_allowed[bn] = 0;
        lp += 2;
        continue;
      }
      if(isxdigit((unsigned char)*lp)) {
        buffer[bn++] = strtoul(lp, &lp, 16);
        continue;
      }
      fatal(0, "%s:%d: evil line: %s", path, lineno, l);
    }
    for(n = 0; n <= bn; ++n) {
      if(breakfn(buffer, bn, n) != break_allowed[n]) {
        fprintf(stderr,
                "%s:%d: offset %zu: mismatch\n"
                "%s\n"
                "\n",
                path, lineno, n, l);
        count_error();
      }
      ++tests;
    }
    xfree(l);
  }
  fclose(fp);
}

/** @brief Tests for @ref lib/unicode.h */
static void test_unicode(void) {
  FILE *fp;
  int lineno = 0;
  char *l, *lp;
  uint32_t buffer[1024];
  uint32_t *c[6], *NFD_c[6], *NFKD_c[6], *NFC_c[6], *NFKC_c[6]; /* 1-indexed */
  int cn, bn;

  fprintf(stderr, "test_unicode\n");
  fp = open_unicode_test("NormalizationTest.txt");
  while(!inputline("NormalizationTest.txt", fp, &l, '\n')) {
    ++lineno;
    if(*l == '#' || *l == '@')
      continue;
    bn = 0;
    cn = 1;
    lp = l;
    c[cn++] = &buffer[bn];
    while(*lp && *lp != '#') {
      if(*lp == ' ') {
	++lp;
	continue;
      }
      if(*lp == ';') {
	buffer[bn++] = 0;
	if(cn == 6)
	  break;
	c[cn++] = &buffer[bn];
	++lp;
	continue;
      }
      buffer[bn++] = strtoul(lp, &lp, 16);
    }
    buffer[bn] = 0;
    assert(cn == 6);
    for(cn = 1; cn <= 5; ++cn) {
      NFD_c[cn] = utf32_decompose_canon(c[cn], utf32_len(c[cn]), 0);
      NFKD_c[cn] = utf32_decompose_compat(c[cn], utf32_len(c[cn]), 0);
      NFC_c[cn] = utf32_compose_canon(c[cn], utf32_len(c[cn]), 0);
      NFKC_c[cn] = utf32_compose_compat(c[cn], utf32_len(c[cn]), 0);
    }
#define unt_check(T, A, B) do {					\
    ++tests;							\
    if(utf32_cmp(c[A], T##_c[B])) {				\
      fprintf(stderr,                                           \
              "NormalizationTest.txt:%d: c%d != "#T"(c%d)\n",   \
              lineno, A, B);                                    \
      fprintf(stderr, "      c%d:%s\n",                         \
              A, format_utf32(c[A]));				\
      fprintf(stderr, "      c%d:%s\n",                         \
              B, format_utf32(c[B]));				\
      fprintf(stderr, "%4s(c%d):%s\n",				\
              #T, B, format_utf32(T##_c[B]));			\
      count_error();						\
    }								\
  } while(0)
    unt_check(NFD, 3, 1);
    unt_check(NFD, 3, 2);
    unt_check(NFD, 3, 3);
    unt_check(NFD, 5, 4);
    unt_check(NFD, 5, 5);
    unt_check(NFKD, 5, 1);
    unt_check(NFKD, 5, 2);
    unt_check(NFKD, 5, 3);
    unt_check(NFKD, 5, 4);
    unt_check(NFKD, 5, 5);
    unt_check(NFC, 2, 1);
    unt_check(NFC, 2, 2);
    unt_check(NFC, 2, 3);
    unt_check(NFC, 4, 4);
    unt_check(NFC, 4, 5);
    unt_check(NFKC, 4, 1);
    unt_check(NFKC, 4, 2);
    unt_check(NFKC, 4, 3);
    unt_check(NFKC, 4, 4);
    unt_check(NFKC, 4, 5);
    for(cn = 1; cn <= 5; ++cn) {
      xfree(NFD_c[cn]);
      xfree(NFKD_c[cn]);
    }
    xfree(l);
  }
  fclose(fp);
  breaktest("auxiliary/GraphemeBreakTest.txt", utf32_is_grapheme_boundary);
  breaktest("auxiliary/WordBreakTest.txt", utf32_is_word_boundary);
  insist(utf32_combining_class(0x40000) == 0);
  insist(utf32_combining_class(0xE0000) == 0);
}

static void test_signame(void) {
  fprintf(stderr, "test_signame\n");
  insist(find_signal("SIGTERM") == SIGTERM);
  insist(find_signal("SIGHUP") == SIGHUP);
  insist(find_signal("SIGINT") == SIGINT);
  insist(find_signal("SIGQUIT") == SIGQUIT);
  insist(find_signal("SIGKILL") == SIGKILL);
  insist(find_signal("SIGYOURMUM") == -1);
}

static void test_cache(void) {
  const struct cache_type t1 = { 1 }, t2 = { 10 };
  const char v11[] = "spong", v12[] = "wibble", v2[] = "blat";
  fprintf(stderr, "test_cache\n");
  cache_put(&t1, "1_1", v11);
  cache_put(&t1, "1_2", v12);
  cache_put(&t2, "2", v2);
  insist(cache_count() == 3);
  insist(cache_get(&t2, "2") == v2);
  insist(cache_get(&t1, "1_1") == v11);
  insist(cache_get(&t1, "1_2") == v12);
  insist(cache_get(&t1, "2") == 0);
  insist(cache_get(&t2, "1_1") == 0);
  insist(cache_get(&t2, "1_2") == 0);
  insist(cache_get(&t1, "2") == 0);
  insist(cache_get(&t2, "1_1") == 0);
  insist(cache_get(&t2, "1_2") == 0);
  sleep(2);
  cache_expire();
  insist(cache_count() == 1);
  insist(cache_get(&t1, "1_1") == 0);
  insist(cache_get(&t1, "1_2") == 0);
  insist(cache_get(&t2, "2") == v2);
  cache_clean(0);
  insist(cache_count() == 0);
  insist(cache_get(&t2, "2") == 0); 
}

static void test_filepart(void) {
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

static void test_selection(void) {
  hash *h;
  fprintf(stderr, "test_selection\n");
  insist((h = selection_new()) != 0);
  selection_set(h, "one", 1);
  selection_set(h, "two", 1);
  selection_set(h, "three", 0);
  selection_set(h, "four", 1);
  insist(selection_selected(h, "one") == 1);
  insist(selection_selected(h, "two") == 1);
  insist(selection_selected(h, "three") == 0);
  insist(selection_selected(h, "four") == 1);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 3);
  selection_flip(h, "one"); 
  selection_flip(h, "three"); 
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "three") == 1);
  insist(hash_count(h) == 3);
  selection_live(h, "one");
  selection_live(h, "two");
  selection_live(h, "three");
  selection_cleanup(h);
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "two") == 1);
  insist(selection_selected(h, "three") == 1);
  insist(selection_selected(h, "four") == 0);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 2);
  selection_empty(h);
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "two") == 0);
  insist(selection_selected(h, "three") == 0);
  insist(selection_selected(h, "four") == 0);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 0);
}

static void test_wstat(void) {
  pid_t pid;
  int w;
  
  fprintf(stderr, "test_wstat\n");
  if(!(pid = xfork())) {
    _exit(1);
  }
  while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
    ;
  check_string(wstat(w), "exited with status 1");
  if(!(pid = xfork())) {
    kill(getpid(), SIGTERM);
    _exit(-1);
  }
  while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
    ;
  check_string_prefix(wstat(w), "terminated by signal 15");
}

static void test_kvp(void) {
  struct kvp *k;
  size_t n;
  
  fprintf(stderr, "test_kvp\n");
  /* decoding */
#define KVP_URLDECODE(S) kvp_urldecode((S), strlen(S))
  insist(KVP_URLDECODE("=%zz") == 0);
  insist(KVP_URLDECODE("=%0") == 0);
  insist(KVP_URLDECODE("=%0z") == 0);
  insist(KVP_URLDECODE("=%%") == 0);
  insist(KVP_URLDECODE("==%") == 0);
  insist(KVP_URLDECODE("wibble") == 0);
  insist(KVP_URLDECODE("") == 0);
  insist(KVP_URLDECODE("wibble&") == 0);
  insist((k = KVP_URLDECODE("one=bl%61t+foo")) != 0);
  check_string(kvp_get(k, "one"), "blat foo");
  insist(kvp_get(k, "ONE") == 0);
  insist(k->next == 0);
  insist((k = KVP_URLDECODE("wibble=splat&bar=spong")) != 0);
  check_string(kvp_get(k, "wibble"), "splat");
  check_string(kvp_get(k, "bar"), "spong");
  insist(kvp_get(k, "ONE") == 0);
  insist(k->next->next == 0);
  /* encoding */
  insist(kvp_set(&k, "bar", "spong") == 0);
  insist(kvp_set(&k, "bar", "foo") == 1);
  insist(kvp_set(&k, "zog", "%") == 1);
  insist(kvp_set(&k, "wibble", 0) == 1);
  insist(kvp_set(&k, "wibble", 0) == 0);
  check_string(kvp_urlencode(k, 0),
               "bar=foo&zog=%25");
  check_string(kvp_urlencode(k, &n),
               "bar=foo&zog=%25");
  insist(n == strlen("bar=foo&zog=%25"));
  check_string(urlencodestring("abc% +\n"),
               "abc%25%20%2b%0a");
}

static void test_sink(void) {
  struct sink *s;
  struct dynstr d[1];
  FILE *fp;
  char *l;
  
  fprintf(stderr, "test_sink\n");

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
  
  dynstr_init(d);
  s = sink_dynstr(d);
  insist(sink_printf(s, "test: %d\n", 999) == 10);
  insist(sink_printf(s, "wibble: %s\n", "foobar") == 15);
  dynstr_terminate(d);
  check_string(d->vec, "test: 999\nwibble: foobar\n");
}

static const char *do_printf(const char *fmt, ...) {
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

static void test_printf(void) {
  char c;
  short s;
  int i;
  long l;
  long long ll;
  intmax_t m;
  ssize_t ssz;
  ptrdiff_t p;
  char *cp;
  char buffer[16];
  
  fprintf(stderr, "test_printf\n");
  check_string(do_printf("%d", 999), "999");
  check_string(do_printf("%d", -999), "-999");
  check_string(do_printf("%i", 999), "999");
  check_string(do_printf("%i", -999), "-999");
  check_string(do_printf("%u", 999), "999");
  check_string(do_printf("%2u", 999), "999");
  check_string(do_printf("%10u", 999), "       999");
  check_string(do_printf("%-10u", 999), "999       ");
  check_string(do_printf("%010u", 999), "0000000999");
  check_string(do_printf("%-10d", -999), "-999      ");
  check_string(do_printf("%-010d", -999), "-999      "); /* "-" beats "0" */
  check_string(do_printf("%66u", 999), "                                                               999");
  check_string(do_printf("%o", 999), "1747");
  check_string(do_printf("%#o", 999), "01747");
  check_string(do_printf("%#o", 0), "0");
  check_string(do_printf("%x", 999), "3e7");
  check_string(do_printf("%#x", 999), "0x3e7");
  check_string(do_printf("%#X", 999), "0X3E7");
  check_string(do_printf("%#x", 0), "0");
  check_string(do_printf("%hd", (short)999), "999");
  check_string(do_printf("%hhd", (short)99), "99");
  check_string(do_printf("%ld", 100000L), "100000");
  check_string(do_printf("%lld", 10000000000LL), "10000000000");
  check_string(do_printf("%qd", 10000000000LL), "10000000000");
  check_string(do_printf("%jd", (intmax_t)10000000000LL), "10000000000");
  check_string(do_printf("%zd", (ssize_t)2000000000), "2000000000");
  check_string(do_printf("%td", (ptrdiff_t)2000000000), "2000000000");
  check_string(do_printf("%hu", (short)999), "999");
  check_string(do_printf("%hhu", (short)99), "99");
  check_string(do_printf("%lu", 100000L), "100000");
  check_string(do_printf("%llu", 10000000000LL), "10000000000");
  check_string(do_printf("%ju", (uintmax_t)10000000000LL), "10000000000");
  check_string(do_printf("%zu", (size_t)2000000000), "2000000000");
  check_string(do_printf("%tu", (ptrdiff_t)2000000000), "2000000000");
  check_string(do_printf("%p", (void *)0x100), "0x100");
  check_string(do_printf("%s", "wibble"), "wibble");
  check_string(do_printf("%s-%s", "wibble", "wobble"), "wibble-wobble");
  check_string(do_printf("%10s", "wibble"), "    wibble");
  check_string(do_printf("%010s", "wibble"), "    wibble"); /* 0 ignored for %s */
  check_string(do_printf("%-10s", "wibble"), "wibble    ");
  check_string(do_printf("%2s", "wibble"), "wibble");
  check_string(do_printf("%.2s", "wibble"), "wi");
  check_string(do_printf("%.2s", "w"), "w");
  check_string(do_printf("%4.2s", "wibble"), "  wi");
  check_string(do_printf("%c", 'a'), "a");
  check_string(do_printf("%4c", 'a'), "   a");
  check_string(do_printf("%-4c", 'a'), "a   ");
  check_string(do_printf("%*c", 0, 'a'), "a");
  check_string(do_printf("x%hhny", &c), "xy");
  insist(c == 1);
  check_string(do_printf("xx%hnyy", &s), "xxyy");
  insist(s == 2);
  check_string(do_printf("xxx%nyyy", &i), "xxxyyy");
  insist(i == 3);
  check_string(do_printf("xxxx%lnyyyy", &l), "xxxxyyyy");
  insist(l == 4);
  check_string(do_printf("xxxxx%llnyyyyy", &ll), "xxxxxyyyyy");
  insist(ll == 5);
  check_string(do_printf("xxxxxx%jnyyyyyy", &m), "xxxxxxyyyyyy");
  insist(m == 6);
  check_string(do_printf("xxxxxxx%znyyyyyyy", &ssz), "xxxxxxxyyyyyyy");
  insist(ssz == 7);
  check_string(do_printf("xxxxxxxx%tnyyyyyyyy", &p), "xxxxxxxxyyyyyyyy");
  insist(p == 8);
  check_string(do_printf("%*d", 5, 99), "   99");
  check_string(do_printf("%*d", -5, 99), "99   ");
  check_string(do_printf("%.*d", 5, 99), "00099");
  check_string(do_printf("%.*d", -5, 99), "99");
  check_string(do_printf("%.0d", 0), "");
  check_string(do_printf("%.d", 0), "");
  check_string(do_printf("%.d", 0), "");
  check_string(do_printf("%%"), "%");
  check_string(do_printf("wibble"), "wibble");
  insist(do_printf("%") == 0);
  insist(do_printf("%=") == 0);
  i = byte_asprintf(&cp, "xyzzy %d", 999);
  insist(i == 9);
  check_string(cp, "xyzzy 999");
  i = byte_snprintf(buffer, sizeof buffer, "xyzzy %d", 999);
  insist(i == 9);
  check_string(buffer, "xyzzy 999");
  i = byte_snprintf(buffer, sizeof buffer, "%*d", 32, 99);
  insist(i == 32);
  check_string(buffer, "               ");
  {
    /* bizarre workaround for compiler checking of format strings */
    char f[] = "xyzzy %";
    i = byte_asprintf(&cp, f);
    insist(i == -1);
  }
}

static void test_basen(void) {
  unsigned long v[64];
  char buffer[1024];

  fprintf(stderr, "test_basen\n");
  v[0] = 999;
  insist(basen(v, 1, buffer, sizeof buffer, 10) == 0);
  check_string(buffer, "999");

  v[0] = 1+2*7+3*7*7+4*7*7*7;
  insist(basen(v, 1, buffer, sizeof buffer, 7) == 0);
  check_string(buffer, "4321");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, sizeof buffer, 256) == 0);
  check_string(buffer, "123456789abcdef");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, sizeof buffer, 16) == 0);
  check_string(buffer, "102030405060708090a0b0c0d0e0f");

  v[0] = 0x00010203;
  v[1] = 0x04050607;
  v[2] = 0x08090A0B;
  v[3] = 0x0C0D0E0F;
  insist(basen(v, 4, buffer, 10, 16) == -1);
}

static void test_split(void) {
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

static void test_hash(void) {
  hash *h;
  int i, *ip;
  char **keys;

  fprintf(stderr, "test_hash\n");
  h = hash_new(sizeof(int));
  for(i = 0; i < 10000; ++i)
    insist(hash_add(h, do_printf("%d", i), &i, HASH_INSERT) == 0);
  check_integer(hash_count(h), 10000);
  for(i = 0; i < 10000; ++i) {
    insist((ip = hash_find(h, do_printf("%d", i))) != 0);
    check_integer(*ip, i);
    insist(hash_add(h, do_printf("%d", i), &i, HASH_REPLACE) == 0);
  }
  check_integer(hash_count(h), 10000);
  keys = hash_keys(h);
  for(i = 0; i < 10000; ++i)
    insist(keys[i] != 0);
  insist(keys[10000] == 0);
  for(i = 0; i < 10000; ++i)
    insist(hash_remove(h, do_printf("%d", i)) == 0);
  check_integer(hash_count(h), 0);
}

static void test_addr(void) {
  struct stringlist a;
  const char *s[2];
  struct addrinfo *ai;
  char *name;
  const struct sockaddr_in *sin;

  static const struct addrinfo pref = {
    AI_PASSIVE,
    PF_INET,
    SOCK_STREAM,
    0,
    0,
    0,
    0,
    0
  };

  a.n = 1;
  a.s = (char **)s;
  s[0] = "smtp";
  ai = get_address(&a, &pref, &name);
  insist(ai != 0);
  check_integer(ai->ai_family, PF_INET);
  check_integer(ai->ai_socktype, SOCK_STREAM);
  check_integer(ai->ai_protocol, IPPROTO_TCP);
  check_integer(ai->ai_addrlen, sizeof(struct sockaddr_in));
  sin = (const struct sockaddr_in *)ai->ai_addr;
  check_integer(sin->sin_family, AF_INET);
  check_integer(sin->sin_addr.s_addr, 0);
  check_integer(ntohs(sin->sin_port), 25);
  check_string(name, "host * service smtp");

  a.n = 2;
  s[0] = "localhost";
  s[1] = "nntp";
  ai = get_address(&a, &pref, &name);
  insist(ai != 0);
  check_integer(ai->ai_family, PF_INET);
  check_integer(ai->ai_socktype, SOCK_STREAM);
  check_integer(ai->ai_protocol, IPPROTO_TCP);
  check_integer(ai->ai_addrlen, sizeof(struct sockaddr_in));
  sin = (const struct sockaddr_in *)ai->ai_addr;
  check_integer(sin->sin_family, AF_INET);
  check_integer(ntohl(sin->sin_addr.s_addr), 0x7F000001);
  check_integer(ntohs(sin->sin_port), 119);
  check_string(name, "host localhost service nntp");
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
  fprintf(stderr,  "%d errors out of %d tests\n", errors, tests);
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
