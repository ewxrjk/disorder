/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007-2009, 2011 Richard Kettlewell
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
             "wget http://www.unicode.org/Public/6.0.0/ucd/%s", path);
    if((w = system(buffer)))
      disorder_fatal(0, "%s: %s", buffer, wstat(w));
    if(chmod(base, 0444) < 0)
      disorder_fatal(errno, "chmod %s", base);
    if(!(fp = fopen(base, "r")))
      disorder_fatal(errno, "%s", base);
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
      disorder_fatal(0, "%s:%d: evil line: %s", path, lineno, l);
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

TEST(unicode);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
