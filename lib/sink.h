/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008, 2013 Richard Kettlewell
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
/** @file lib/sink.h
 * @brief Abstract output sink type
 */

#ifndef SINK_H
#define SINK_H

#include <stdarg.h>

struct dynstr;
struct socketio;

/** @brief Sink type
 *
 * A sink is something you write bytes to; the opposite would be a
 * source.  We provide sink_stdio() and sink_dynstr() to create sinks
 * to write to stdio streams and dynamic strings.
 */
struct sink {
  /** @brief Write callback
   * @param s Sink to write to
   * @param buffer First byte to write
   * @param nbytes Number of bytes to write
   * @return non-negative on success, -1 on error
   */
  int (*write)(struct sink *s, const void *buffer, int nbytes);

  /** @brief Flush callback
   * @param s Sink to write to
   * @return non-negative on success, -1 on error
   */
  int (*flush)(struct sink *s);

  /** @brief Error callback
   * @param s Sink
   * @return Last error code
   */
  int (*error)(struct sink *s);

  enum error_class eclass;
};

struct sink *sink_stdio(const char *name, FILE *fp);
/* return a sink which writes to @fp@.  If @name@ is not a null
 * pointer, it will be used in (fatal) error messages; if it is a null
 * pointer then errors will be signalled by returning -1. */

struct sink *sink_dynstr(struct dynstr *output);
/* return a sink which appends to @output@. */

struct sink *sink_discard(void);
/* return a sink which junks everything */

struct sink *sink_error(void);
/* return a sink which fails all writes */

struct sink *sink_socketio(struct socketio *sio);
/* return a sink which writes to a socket */

int sink_vprintf(struct sink *s, const char *fmt, va_list ap);
int sink_printf(struct sink *s, const char *fmt, ...)
  attribute((format (printf, 2, 3)));
/* equivalent of vfprintf/fprintf for sink @s@ */

/** @brief Write bytes to a sink
 * @param s Sink to write to
 * @param buffer First byte to write
 * @param nbytes Number of bytes to write
 * @return non-negative on success, -1 on error
 */
static inline int sink_write(struct sink *s, const void *buffer, int nbytes) {
  return s->write(s, buffer, nbytes);
}

/** @brief Write string to a sink
 * @param s Sink to write to
 * @param str String to write
 * @return non-negative on success, -1 on error
 */
static inline int sink_writes(struct sink *s, const char *str) {
  return s->write(s, str, strlen(str));
}

static inline int sink_flush(struct sink *s) {
  return s->flush(s);
}

/** @brief Write one byte to a sink
 * @param s Sink to write to
 * @param c Byte to write (as a @c char)
 * @return non-negative on success, -1 on error
 */
static inline int sink_writec(struct sink *s, char c) {
  return s->write(s, &c, 1);
}

static inline int sink_err(struct sink *s) {
  return s->error(s);
}

struct source {
  int (*getch)(struct source *s);
  int (*error)(struct source *s);
  int (*eof)(struct source *s);

  enum error_class eclass;
};

struct source *source_stdio(FILE *fp);
struct source *source_socketio(struct socketio *sio);

static inline int source_getc(struct source *s) {
  return s->getch(s);
}

static inline int source_err(struct source *s) {
  return s->error(s);
}

static inline int source_eof(struct source *s) {
  return s->eof(s);
}

#endif /* SINK_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
