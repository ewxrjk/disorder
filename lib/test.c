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

#include "utf8.h"
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

#define check_string(GOT, WANT) do {				\
  const char *g = GOT;						\
  const char *w = WANT;						\
								\
  if(w == 0) {							\
    fprintf(stderr, "%s:%d: %s returned 0\n",			\
            __FILE__, __LINE__, #GOT);				\
    count_error();                                              \
  } else if(strcmp(w, g)) {					\
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s\n",	\
	    __FILE__, __LINE__, #GOT, format(g), format(w));	\
    count_error();                                              \
  }								\
  ++tests;							\
 } while(0)

#define check_string_prefix(GOT, WANT) do {                             \
  const char *g = GOT;                                                  \
  const char *w = WANT;                                                 \
                                                                        \
  if(w == 0) {                                                          \
    fprintf(stderr, "%s:%d: %s returned 0\n",                           \
            __FILE__, __LINE__, #GOT);                                  \
    count_error();							\
  } else if(strncmp(w, g, strlen(w))) {                                 \
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s...\n",	\
	    __FILE__, __LINE__, #GOT, format(g), format(w));            \
    count_error();							\
  }                                                                     \
  ++tests;                                                              \
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
  insist(!strcmp(u8, CHARS));			\
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
  check_string(d_dirname("/spong"), "/");
  check_string(d_dirname("/foo/bar"), "/foo");
  check_string(d_dirname("./bar"), ".");
  check_string(d_dirname("."), ".");
  check_string(d_dirname(".."), ".");
  check_string(d_dirname("../blat"), "..");
  check_string(d_dirname("wibble"), ".");
  check_string(extension("foo.c"), ".c");
  check_string(extension(".c"), ".c");
  check_string(extension("."), ".");
  check_string(extension("foo"), "");
  check_string(extension("./foo"), "");
  check_string(extension("./foo.c"), ".c");
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

int main(void) {
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
  /* asprintf.c */
  /* authhash.c */
  /* basen.c */
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
  test_words();
  /* wstat.c */
  test_wstat();
  /* signame.c */
  test_signame();
  /* cache.c */
  test_cache();
  /* selection.c */
  test_selection();
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
