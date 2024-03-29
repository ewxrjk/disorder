/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007-2009 Richard Kettlewell
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
/** @file lib/charset.h @brief Character set conversion */
#ifndef CHARSET_H
#define CHARSET_H

#include "log.h"

struct dynstr;

/* Character encoding conversion routines */

char *mb2utf8(const char *mb);
char *utf82mb(const char *utf8);
/* various conversions, between multibyte strings (mb) in
 * whatever the current encoding is, and UTF-8 strings (utf8).  On
 * error, a null pointer is returned and @errno@ set. */

char *any2utf8(const char *from/*encoding*/,
	       const char *any/*string*/);
/* arbitrary conversions from any null-free byte-based encoding that
 * iconv knows about to UTF-8 */

char *any2mb(const char *from/*encoding or 0*/,
	     const char *any/*string*/);
/* Arbitrary conversions from any null-free byte-based encoding that
 * iconv knows about to a multibyte string.  If FROM is 0 then ANY is
 * returned unchanged. */

char *any2any(const char *from/*encoding or 0*/,
	      const char *to/*encoding to 0*/,
	      const char *any/*string*/);
/* Arbitrary conversions between any null-free byte-based encodings
 * that iconv knows.  If FROM and TO are both 0 then ANY is returned
 * unchanged. */

char *mb2utf8_f(char *mb);
char *utf82mb_f(char *utf8);
char *any2utf8_f(const char *from/*encoding*/,
                 char *any/*string*/);
char *any2mb_f(const char *from/*encoding or 0*/,
               char *any/*string*/);
char *any2any_f(const char *from/*encoding or 0*/,
                const char *to/*encoding to 0*/,
                char *any/*string*/);

/** @brief Insist that @p s is not null
 * @param s Pointer to check
 * @return @p s
 *
 * Terminates the process if @p s is a null pointer.
 */
static inline char *nullcheck(char *s) {
  if(!s) exitfn(1);			/* assume an error already reported */
  return s;
}

const char *truncate_for_display(const char *s, long max);

#endif /* CHARSET_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
