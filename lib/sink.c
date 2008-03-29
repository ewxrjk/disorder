/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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
/** @file lib/sink.c
 * @brief Abstract output sink type
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

/** @brief Formatted output to a sink
 * @param s Sink to write to
 * @param fmt Format string
 * @param ap Argument list
 * @return Number of bytes written on success, -1 on error
 */
int sink_vprintf(struct sink *s, const char *fmt, va_list ap) {
  return byte_vsinkprintf(s, fmt, ap);
}

/** @brief Formatted output to a sink
 * @param s Sink to write to
 * @param fmt Format string
 * @return Number of bytes written on success, -1 on error
 */
int sink_printf(struct sink *s, const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = byte_vsinkprintf(s, fmt, ap);
  va_end(ap);
  return n;
}

/* stdio sink *****************************************************************/

/** @brief Sink that writes to a stdio @c FILE */
struct stdio_sink {
  /** @brief Base member */
  struct sink s;

  /** @brief Filename */
  const char *name;

  /** @brief Stream to write to */
  FILE *fp;
};

/** @brief Reinterpret a @ref sink as a @ref stdio_sink */
#define S(s) ((struct stdio_sink *)s)

/** @brief Write callback for @ref stdio_sink */
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

/** @brief Create a sink that writes to a stdio stream
 * @param name Filename for use in error messages
 * @param fp Stream to write to
 * @return Pointer to new sink
 */
struct sink *sink_stdio(const char *name, FILE *fp) {
  struct stdio_sink *s = xmalloc(sizeof *s);

  s->s.write = sink_stdio_write;
  s->name = name;
  s->fp = fp;
  return (struct sink *)s;
}

/* dynstr sink ****************************************************************/

/** @brief Sink that writes to a dynamic string */
struct dynstr_sink {
  /** @brief Base member */
  struct sink s;
  /** @brief Pointer to dynamic string to append to */
  struct dynstr *d;
};

/** @brief Write callback for @ref dynstr_sink */
static int sink_dynstr_write(struct sink *s, const void *buffer, int nbytes) {
  dynstr_append_bytes(((struct dynstr_sink *)s)->d, buffer, nbytes);
  return nbytes;
}

/** @brief Create a sink that appends to a @ref dynstr
 * @param output Dynamic string to append to
 * @return Pointer to new sink
 */
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
