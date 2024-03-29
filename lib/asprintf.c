/*
 * This file is part of DisOrder
 * Copyright (C) 2004-2009 Richard Kettlewell
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
/** @file lib/asprintf.c @brief printf() workalikes */

#include "common.h"

#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

#include "printf.h"
#include "log.h"
#include "sink.h"
#include "mem.h"
#include "vector.h"

/** @brief vasprintf() workalike without encoding errors
 *
 * This acts like vasprintf() except that it does not throw an error
 * if you use a string outside the current locale's encoding rules,
 * and it uses the memory allocation calls from @ref mem.h.
 */
int byte_vasprintf(char **ptrp,
		   const char *fmt,
		   va_list ap) {
  struct sink *s;
  struct dynstr d;
  int n;

  dynstr_init(&d);
  s = sink_dynstr(&d);
  n = byte_vsinkprintf(s, fmt, ap);
  xfree(s);
  if(n >= 0) {
    dynstr_terminate(&d);
    *ptrp = d.vec;
  }
  return n;
}

/** @brief asprintf() workalike without encoding errors
 *
 * This acts like asprintf() except that it does not throw an error
 * if you use a string outside the current locale's encoding rules,
 * and it uses the memory allocation calls from @ref mem.h.
 */
int byte_asprintf(char **ptrp,
		  const char *fmt,
		  ...) {
  int n;
  va_list ap;

  va_start(ap, fmt);
  n = byte_vasprintf(ptrp, fmt, ap);
  va_end(ap);
  return n;
}

/** @brief asprintf() workalike without encoding errors
 *
 * This acts like asprintf() except that it does not throw an error if
 * you use a string outside the current locale's encoding rules; it
 * uses the memory allocation calls from @ref mem.h; and it terminates
 * the program on error.
 */
int byte_xasprintf(char **ptrp,
		   const char *fmt,
		   ...) {
  int n;
  va_list ap;

  va_start(ap, fmt);
  n = byte_xvasprintf(ptrp, fmt, ap);
  va_end(ap);
  return n;
}

/** @brief vasprintf() workalike without encoding errors
 *
 * This acts like vasprintf() except that it does not throw an error
 * if you use a string outside the current locale's encoding rules; it
 * uses the memory allocation calls from @ref mem.h; and it terminates
 * the program on error.
 */
int byte_xvasprintf(char **ptrp,
		    const char *fmt,
		    va_list ap) {
  int n;

  if((n = byte_vasprintf(ptrp, fmt, ap)) < 0)
    disorder_fatal(errno, "error calling byte_vasprintf");
  return n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
