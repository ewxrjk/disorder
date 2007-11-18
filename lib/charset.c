/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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
/** @file lib/charset.c @brief Character set conversion */

#include <config.h>
#include "types.h"

#include <iconv.h>
#include <string.h>
#include <errno.h>
#include <langinfo.h>

#include "mem.h"
#include "log.h"
#include "charset.h"
#include "configuration.h"
#include "utf8.h"
#include "vector.h"
#include "unidata.h"

/** @brief Low-level converstion routine
 * @param from Source encoding
 * @param to Destination encoding
 * @param ptr First byte to convert
 * @param n Number of bytes to convert
 * @return Converted text, 0-terminated; or NULL on error.
 */
static void *convert(const char *from, const char *to,
		     const void *ptr, size_t n) {
  iconv_t i;
  size_t len;
  char *buf = 0, *s, *d;
  size_t bufsize = 0, sl, dl;

  if((i = iconv_open(to, from)) == (iconv_t)-1)
    fatal(errno, "error calling iconv_open");
  do {
    bufsize = bufsize ? 2 * bufsize : 32;
    buf = xrealloc_noptr(buf, bufsize);
    iconv(i, 0, 0, 0, 0);
    s = (char *)ptr;
    sl = n;
    d = buf;
    dl = bufsize;
    /* (void *) to work around FreeBSD's nonstandard iconv prototype */
    len = iconv(i, (void *)&s, &sl, &d, &dl);
  } while(len == (size_t)-1 && errno == E2BIG);
  iconv_close(i);
  if(len == (size_t)-1) {
    error(errno, "error converting from %s to %s", from, to);
    return 0;
  }
  return buf;
}

/** @brief Convert UTF-8 to UCS-4
 * @param mb Pointer to 0-terminated UTF-8 string
 * @return Pointer to 0-terminated UCS-4 string
 *
 * Not everybody's iconv supports UCS-4, and it's inconvenient to have to know
 * our endianness, and it's easy to convert it ourselves, so we do.  See also
 * @ref ucs42utf8().
 */ 
uint32_t *utf82ucs4(const char *mb) {
  struct dynstr_ucs4 d;
  uint32_t c;

  dynstr_ucs4_init(&d);
  while(*mb) {
    PARSE_UTF8(mb, c,
	       error(0, "invalid UTF-8 sequence"); return 0;);
    dynstr_ucs4_append(&d, c);
  }
  dynstr_ucs4_terminate(&d);
  return d.vec;
}

/** @brief Convert one UCS-4 character to UTF-8
 * @param c Character to convert
 * @param d Dynamic string to append UTF-8 sequence to
 * @return 0 on success, -1 on error
 */
int one_ucs42utf8(uint32_t c, struct dynstr *d) {
  if(c < 0x80)
    dynstr_append(d, c);
  else if(c < 0x800) {
    dynstr_append(d, 0xC0 | (c >> 6));
    dynstr_append(d, 0x80 | (c & 0x3F));
  } else if(c < 0x10000) {
    dynstr_append(d, 0xE0 | (c >> 12));
    dynstr_append(d, 0x80 | ((c >> 6) & 0x3F));
    dynstr_append(d, 0x80 | (c & 0x3F));
  } else if(c < 0x110000) {
    dynstr_append(d, 0xF0 | (c >> 18));
    dynstr_append(d, 0x80 | ((c >> 12) & 0x3F));
    dynstr_append(d, 0x80 | ((c >> 6) & 0x3F));
    dynstr_append(d, 0x80 | (c & 0x3F));
  } else {
    error(0, "invalid UCS-4 character %#"PRIx32, c);
    return -1;
  }
  return 0;
}

/** @brief Convert UCS-4 to UTF-8
 * @param u Pointer to 0-terminated UCS-4 string
 * @return Pointer to 0-terminated UTF-8 string
 *
 * See @ref utf82ucs4().
 */
char *ucs42utf8(const uint32_t *u) {
  struct dynstr d;
  uint32_t c;

  dynstr_init(&d);
  while((c = *u++)) {
    if(one_ucs42utf8(c, &d))
      return 0;
  }
  dynstr_terminate(&d);
  return d.vec;
}

/** @brief Convert from the local multibyte encoding to UTF-8 */
char *mb2utf8(const char *mb) {
  return convert(nl_langinfo(CODESET), "UTF-8", mb, strlen(mb) + 1);
}

/** @brief Convert from UTF-8 to the local multibyte encoding */
char *utf82mb(const char *utf8) {
  return convert("UTF-8", nl_langinfo(CODESET), utf8, strlen(utf8) + 1);
}

/** @brief Convert from encoding @p from to UTF-8 */
char *any2utf8(const char *from, const char *any) {
  return convert(from, "UTF-8", any, strlen(any) + 1);
}

/** @brief Convert from encoding @p from to the local multibyte encoding */
char *any2mb(const char *from, const char *any) {
  if(from) return convert(from, nl_langinfo(CODESET), any, strlen(any) + 1);
  else return xstrdup(any);
}

/** @brief Convert from encoding @p from to encoding @p to */
char *any2any(const char *from,
	      const char *to,
	      const char *any) {
  if(from || to) return convert(from, to, any, strlen(any) + 1);
  else return xstrdup(any);
}

/** @brief strlen workalike for UCS-4 strings
 *
 * We don't rely on the local @c wchar_t being UCS-4.
 */
int ucs4cmp(const uint32_t *a, const uint32_t *b) {
  while(*a && *b && *a == *b) ++a, ++b;
  if(*a > *b) return 1;
  else if(*a < *b) return -1;
  else return 0;
}

/** @brief Return nonzero if @p c is a combining character */
static int combining(int c) {
  if(c < UNICODE_NCHARS) {
    const struct unidata *const ud = &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];

    return ud->general_category == unicode_General_Category_Mn || ud->ccc != 0;
  }
  /* Assume unknown characters are noncombining */
  return 0;
}

/** @brief Truncate a string for display purposes
 * @param s Pointer to UTF-8 string
 * @param max Maximum number of columns
 * @return @p or truncated string (never NULL)
 *
 * We don't correctly support bidi or double-width characters yet, nor
 * locate default grapheme cluster boundaries for saner truncation.
 */
const char *truncate_for_display(const char *s, long max) {
  const char *t = s, *r, *cut = 0;
  char *truncated;
  uint32_t c;
  long n = 0;

  /* We need to discover two things: firstly whether the string is
   * longer than @p max glyphs and secondly if it is not, where to cut
   * the string.
   *
   * Combining characters follow their base character (unicode
   * standard 5.0 s2.11), so after each base character we must 
   */
  while(*t) {
    PARSE_UTF8(t, c, return s);
    if(combining(c))
      /* This must be an initial combining character.  We just skip it. */
      continue;
    /* So c must be a base character.  It may be followed by any
     * number of combining characters.  We advance past them. */
    do {
      r = t;
      PARSE_UTF8(t, c, return s);
    } while(combining(c));
    /* Last character wasn't a combining character so back up */
    t = r;
    ++n;
    /* So now there are N glyphs before position T.  We might
     * therefore have reached the cut position. */
    if(n == max - 3)
      cut = t;
  }
  /* If the string is short enough we return it unmodified */
  if(n < max)
    return s;
  truncated = xmalloc_noptr(cut - s + 4);
  memcpy(truncated, s, cut - s);
  strcpy(truncated + (cut - s), "...");
  return truncated;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
