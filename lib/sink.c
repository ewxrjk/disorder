/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007-9, 2013 Richard Kettlewell
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
/** @file lib/sink.c
 * @brief Abstract output sink type
 */

#include "common.h"

#include <stdarg.h>
#include <errno.h>

#include "mem.h"
#include "vector.h"
#include "log.h"
#include "sink.h"
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

static int sink_generic_flush(struct sink attribute((unused)) *s) {
  return 0;
}

static int sink_generic_error(struct sink attribute((unused)) *s) {
  return 0;
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

  int error;
};

/** @brief Reinterpret a @ref sink as a @ref stdio_sink */
#define S(s) ((struct stdio_sink *)s)

/** @brief Write callback for @ref stdio_sink */
static int sink_stdio_write(struct sink *s, const void *buffer, int nbytes) {
  int n = fwrite(buffer, 1, nbytes, S(s)->fp);
  if(n < nbytes) {
    S(s)->error = errno;
    if(S(s)->name)
      disorder_fatal(errno, "error writing to %s", S(s)->name);
    else
      return -1;
  }
  return n;
}

static int sink_stdio_error(struct sink *s) {
  return S(s)->error;
}

/** @brief Create a sink that writes to a stdio stream
 * @param name Filename for use in error messages
 * @param fp Stream to write to
 * @return Pointer to new sink
 */
struct sink *sink_stdio(const char *name, FILE *fp) {
  struct stdio_sink *s = xmalloc(sizeof *s);

  s->s.write = sink_stdio_write;
  s->s.flush = sink_generic_flush;
  s->s.error = sink_stdio_error;
  s->s.eclass = ec_errno;
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
  s->s.flush = sink_generic_flush;
  s->s.error = sink_generic_error;
  s->s.eclass = ec_errno;
  s->d = output;
  return (struct sink *)s;
}

/* discard sink **************************************************************/

static int sink_discard_write(struct sink attribute((unused)) *s,
			      const void attribute((unused)) *buffer,
			      int nbytes) {
  return nbytes;
}

/** @brief Return a sink which discards all output */
struct sink *sink_discard(void) {
  struct sink *s = xmalloc(sizeof *s);

  s->write = sink_discard_write;
  s->flush = sink_generic_flush;
  s->error = sink_generic_error;
  s->eclass = ec_errno;
  return s;
}

/* error sink **************************************************************/

static int sink_error_write(struct sink attribute((unused)) *s,
			    const void attribute((unused)) *buffer,
			    int attribute((unused)) nbytes) {
  return -1;
}

/** @brief Return a sink which discards all output */
struct sink *sink_error(void) {
  struct sink *s = xmalloc(sizeof *s);

  s->write = sink_error_write;
  s->flush = sink_generic_flush;
  s->error = sink_generic_error;
  s->eclass = ec_errno;
  return s;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
