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

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "mem.h"
#include "vector.h"
#include "sink.h"
#include "log.h"
#include "printf.h"

int sink_vprintf(struct sink *s, const char *fmt, va_list ap) {
  return byte_vsinkprintf(s, fmt, ap);
}

int sink_printf(struct sink *s, const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = byte_vsinkprintf(s, fmt, ap);
  va_end(ap);
  return n;
}

/* stdio sink *****************************************************************/

struct stdio_sink {
  struct sink s;
  const char *name;
  FILE *fp;
};

#define S(s) ((struct stdio_sink *)s)

static int sink_stdio_write(struct sink *s, const void *buffer, int nbytes) {
  int n = fwrite(buffer, 1, nbytes, S(s)->fp);
  if(n < nbytes) {
    if(S(s)->name)
      fatal(errno, "error writing to %s", S(s)->name);
    else
      return -1;
  }
  return n;
}

struct sink *sink_stdio(const char *name, FILE *fp) {
  struct stdio_sink *s = xmalloc(sizeof *s);

  s->s.write = sink_stdio_write;
  s->name = name;
  s->fp = fp;
  return (struct sink *)s;
}

/* dynstr sink ****************************************************************/

struct dynstr_sink {
  struct sink s;
  struct dynstr *d;
};

static int sink_dynstr_write(struct sink *s, const void *buffer, int nbytes) {
  dynstr_append_bytes(((struct dynstr_sink *)s)->d, buffer, nbytes);
  return nbytes;
}

struct sink *sink_dynstr(struct dynstr *output) {
  struct dynstr_sink *s = xmalloc(sizeof *s);

  s->s.write = sink_dynstr_write;
  s->d = output;
  return (struct sink *)s;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:10f08a9ea316137ef7259088403a636e */
