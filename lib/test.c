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

#include "utf8.h"
#include "mem.h"
#include "log.h"
#include "vector.h"
#include "charset.h"
#include "mime.h"
#include "hex.h"
#include "words.h"
#include "heap.h"
#include "unicode.h"
#include "inputline.h"

static int tests, errors;

/** @brief Checks that @p expr is nonzero */
#define insist(expr) do {				\
  if(!(expr)) {						\
    ++errors;						\
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
    if(c >= 32 && c <= 127)
      dynstr_append(&d, c);
    else {
      sprintf(buf, "\\x%04lX", (unsigned long)c);
      dynstr_append_string(&d, buf);
    }
  }
  dynstr_terminate(&d);
  return d.vec;
}

#define check_string(GOT, WANT) do {				\
  const char *g = GOT;						\
  const char *w = WANT;						\
								\
  if(w == 0) {							\
    fprintf(stderr, "%s:%d: %s returned 0\n",			\
            __FILE__, __LINE__, #GOT);				\
    ++errors;							\
  } else if(strcmp(w, g)) {					\
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s\n",	\
	    __FILE__, __LINE__, #GOT, format(g), format(w));	\
    ++errors;							\
  }								\
  ++tests;							\
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
  ucs = utf82ucs4(CHARS);			\
  insist(ucs != 0);				\
  insist(!ucs4cmp(w, ucs));			\
  u8 = ucs42utf8(ucs);				\
  insist(u8 != 0);				\
  insist(!strcmp(u8, CHARS));			\
} while(0)

  fprintf(stderr, "test_utf8\n");

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

static void test_mime(void) {
  char *t, *n, *v;

  fprintf(stderr, "test_mime\n");

  t = n = v = 0;
  insist(!mime_content_type("text/plain", &t, &n, &v));
  insist(!strcmp(t, "text/plain"));
  insist(n == 0);
  insist(v == 0);

  t = n = v = 0;
  insist(!mime_content_type("TEXT ((nested) comment) /plain", &t, &n, &v));
  insist(!strcmp(t, "text/plain"));
  insist(n == 0);
  insist(v == 0);

  t = n = v = 0;
  insist(!mime_content_type(" text/plain ; Charset=utf-8", &t, &n, &v));
  insist(!strcmp(t, "text/plain"));
  insist(!strcmp(n, "charset"));
  insist(!strcmp(v, "utf-8"));

  t = n = v = 0;
  insist(!mime_content_type("text/plain;charset = ISO-8859-1 ", &t, &n, &v));
  insist(!strcmp(t, "text/plain"));
  insist(!strcmp(n, "charset"));
  insist(!strcmp(v, "ISO-8859-1"));

  /* XXX mime_parse */
  /* XXX mime_multipart */
  /* XXX mime_rfc2388_content_disposition */

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
      insist(!strcmp(canon_folded, "ss"));
      insist(!strcmp(compat_folded, "ss"));
      l = 0;
      break;
    }
    if(l) {
      /* Case-folded data is now normalized */
      canon_expected = ucs42utf8(utf32_decompose_canon(&l, 1, 0));
      if(strcmp(canon_folded, canon_expected)) {
	fprintf(stderr, "%s:%d: canon-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(canon_folded), format(canon_expected));
	++errors;
      }
      ++tests;
      compat_expected = ucs42utf8(utf32_decompose_compat(&l, 1, 0));
      if(strcmp(compat_folded, compat_expected)) {
	fprintf(stderr, "%s:%d: compat-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(compat_folded), format(compat_expected));
	++errors;
      }
      ++tests;
    }
  }
  check_string(casefold(""), "");
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

/** @brief Tests for @ref lib/unicode.h */
static void test_unicode(void) {
  FILE *fp;
  int lineno = 0;
  char *l, *lp;
  uint32_t buffer[1024];
  uint32_t *c[6], *NFD_c[6],  *NFKD_c[6]; /* 1-indexed */
  int cn, bn;

  fprintf(stderr, "test_unicode\n");
  if(!(fp = fopen("NormalizationTest.txt", "r"))) {
    system("wget http://www.unicode.org/Public/5.0.0/ucd/NormalizationTest.txt");
    chmod("NormalizationTest.txt", 0444);
    if(!(fp = fopen("NormalizationTest.txt", "r"))) {
      perror("NormalizationTest.txt");
      ++tests;				/* don't know how many... */
      ++errors;
      return;
    }
  }
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
    }
#define unt_check(T, A, B) do {					\
    ++tests;							\
    if(utf32_cmp(c[A], T##_c[B])) {				\
      fprintf(stderr, "L%d: c%d != "#T"(c%d)\n", lineno, A, B);	\
      fprintf(stderr, "    c%d:      %s\n",			\
              A, format_utf32(c[A]));				\
      fprintf(stderr, "%4s(c%d): %s\n",				\
              #T, B, format_utf32(T##_c[B]));			\
      ++errors;							\
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
    for(cn = 1; cn <= 5; ++cn) {
      xfree(NFD_c[cn]);
      xfree(NFKD_c[cn]);
    }
    xfree(l);
  }
}

int main(void) {
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
  /* asprintf.c */
  /* authhash.c */
  /* basen.c */
  /* charset.c */
  /* client.c */
  /* configuration.c */
  /* event.c */
  /* fprintf.c */
  /* heap.c */
  test_heap();
  /* hex.c */
  test_hex();
  /* inputline.c */
  /* kvp.c */
  /* log.c */
  /* mem.c */
  /* mime.c */
  test_mime();
  /* mixer.c */
  /* plugin.c */
  /* printf.c */
  /* queue.c */
  /* sink.c */
  /* snprintf.c */
  /* split.c */
  /* syscalls.c */
  /* table.c */
  /* unicode.c */
  test_unicode();
  /* utf8.c */
  test_utf8();
  /* vector.c */
  /* words.c */
  test_casefold();
  /* XXX words() */
  /* wstat.c */
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
