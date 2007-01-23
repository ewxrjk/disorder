/*
 * This file is part of DisOrder
 * Copyright (C) 2004 Richard Kettlewell
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

#define NO_MEMORY_ALLOCATION
/* because used from log.c */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "printf.h"
#include "sink.h"

struct fixedstr_sink {
  struct sink s;
  char *buffer;
  int nbytes;
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
