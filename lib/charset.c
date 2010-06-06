/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/charset.c @brief Character set conversion */

#include "common.h"

#include <iconv.h>
#include <errno.h>
#include <langinfo.h>

#include "mem.h"
#include "log.h"
#include "charset.h"
#include "configuration.h"
#include "vector.h"
#include "unicode.h"

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
    disorder_fatal(errno, "error calling iconv_open");
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
    disorder_error(errno, "error converting from %s to %s", from, to);
    return 0;
  }
  return buf;
}

/** @brief Convert from the local multibyte encoding to UTF-8
 * @param mb String in current locale's multibyte encoding
 * @return Same string in UTF-8
 */
char *mb2utf8(const char *mb) {
  return convert(nl_langinfo(CODESET), "UTF-8", mb, strlen(mb) + 1);
}

/** @brief Convert from UTF-8 to the local multibyte encoding
 * @param utf8 String in UTF-8
 * @return Same string in current locale's multibyte encoding
 */
char *utf82mb(const char *utf8) {
  return convert("UTF-8", nl_langinfo(CODESET), utf8, strlen(utf8) + 1);
}

/** @brief Convert from encoding @p from to UTF-8
 * @param from Source encoding
 * @param any String in encoding @p from
 * @return @p any converted to UTF-8
 */
char *any2utf8(const char *from, const char *any) {
  return convert(from, "UTF-8", any, strlen(any) + 1);
}

/** @brief Convert from encoding @p from to the local multibyte encoding
 * @param from Source encoding
 * @param any String in encoding @p from
 * @return @p any converted to current locale's multibyte encoding
 */
char *any2mb(const char *from, const char *any) {
  if(from) return convert(from, nl_langinfo(CODESET), any, strlen(any) + 1);
  else return xstrdup(any);
}

/** @brief Convert from encoding @p from to encoding @p to
 * @param from Source encoding
 * @param to Destination encoding
 * @param any String in encoding @p from
 * @return @p any converted to encoding @p to
 */
char *any2any(const char *from,
	      const char *to,
	      const char *any) {
  if(from || to) return convert(from, to, any, strlen(any) + 1);
  else return xstrdup(any);
}

/** @brief Truncate a string for display purposes
 * @param s Pointer to UTF-8 string
 * @param max Maximum number of columns
 * @return @p or truncated string (never NULL)
 *
 * Returns a string that is no longer than @p max graphemes long and is either
 * (canonically) equal to @p s or is a truncated form of it with an ellipsis
 * appended.
 *
 * We don't take display width into account (tricky for HTML!) and we don't
 * attempt to implement the Bidi algorithm.  If you have track names for which
 * either of these matter in practice then get in touch.
 */
const char *truncate_for_display(const char *s, long max) {
  uint32_t *s32;
  size_t l32, cut;
  utf32_iterator it;
  long graphemes;

  /* Convert to UTF-32 for processing */
  if(!(s32 = utf8_to_utf32(s, strlen(s), &l32)))
    return 0;
  it = utf32_iterator_new(s32, l32);
  cut = l32;
  graphemes = 0;                        /* # of graphemes left of it */
  while(graphemes <= max && utf32_iterator_where(it) < l32) {
    if(graphemes == max - 1)
      cut = utf32_iterator_where(it);
    utf32_iterator_advance(it, 1);
    if(utf32_iterator_grapheme_boundary(it))
      ++graphemes;
  }
  if(graphemes > max) {                 /* we need to cut */
    s32[cut] = 0x2026;                  /* HORIZONTAL ELLIPSIS */
    l32 = cut + 1;
    s = utf32_to_utf8(s32, l32, 0);
  }
  xfree(s32);
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
