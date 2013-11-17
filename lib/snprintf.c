/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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
/** @file lib/snprintf.c
 * @brief UTF-8 capable *snprintf workalikes
 */

#define NO_MEMORY_ALLOCATION
/* because used from log.c */

#include "common.h"

#include <stdarg.h>
#include <stddef.h>

#include "printf.h"
#include "log.h"
#include "sink.h"

/** @brief A @ref sink that stores to a fixed buffer
 *
 * If there is too much output, it is truncated.
 */
struct fixedstr_sink {
  /** @brief Base */
  struct sink s;

  /** @brief Output buffer */
  char *buffer;

  /** @brief Bytes written so far */
  int nbytes;

  /** @brief Size of buffer */
  size_t size;
};

static int fixedstr_write(struct sink *f, const void *buffer, int nbytes) {
  struct fixedstr_sink *s = (struct fixedstr_sink *)f;
  int count;

  if((size_t)s->nbytes < s->size) {
    if((size_t)nbytes > s->size - s->nbytes)
      count = s->size - s->nbytes;
    else
      count = nbytes;
    memcpy(s->buffer + s->nbytes, buffer, count);
  }
  s->nbytes += nbytes;
  return 0;
}

int byte_vsnprintf(char buffer[],
		   size_t bufsize,
		   const char *fmt,
		   va_list ap) {
  struct fixedstr_sink s;
  int n, m;

  /* We have to make a sink directly here, since we can't safely do memory
   * allocation here (we might be formatting the error message from a failed
   * memory allocation) */
  s.s.write = fixedstr_write;
  s.buffer = buffer;
  s.nbytes = 0;
  s.size = bufsize;
  n = byte_vsinkprintf(&s.s, fmt, ap);
  if(bufsize) {
    /* add the null terminator (even if the printf failed) */
    m = s.nbytes;
    if((size_t)m >= bufsize) m = bufsize - 1;
    buffer[m] = 0;
  }
  return n;
}

int byte_snprintf(char buffer[], size_t bufsize, const char *fmt, ...) {
  int n;
  va_list ap;

  va_start(ap, fmt);
  n = byte_vsnprintf(buffer, bufsize, fmt, ap);
  va_end(ap);
  return n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
