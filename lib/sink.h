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

#ifndef SINK_H
#define SINK_H

/* a sink is something you write to (the opposite would be a source) */

struct dynstr;

struct sink {
  int (*write)(struct sink *s, const void *buffer, int nbytes);
  /* return >= 0 on success, -1 on error */
};

struct sink *sink_stdio(const char *name, FILE *fp);
/* return a sink which writes to @fp@.  If @name@ is not a null
 * pointer, it will be used in (fatal) error messages; if it is a null
 * pointer then errors will be signalled by returning -1. */

struct sink *sink_dynstr(struct dynstr *output);
/* return a sink which appends to @output@. */

int sink_vprintf(struct sink *s, const char *fmt, va_list ap);
int sink_printf(struct sink *s, const char *fmt, ...)
  attribute((format (printf, 2, 3)));
/* equivalent of vfprintf/fprintf for sink @s@ */

static inline int sink_write(struct sink *s, const void *buffer, int nbytes) {
  return s->write(s, buffer, nbytes);
}

static inline int sink_writes(struct sink *s, const char *str) {
  return s->write(s, str, strlen(str));
}

static inline int sink_writec(struct sink *s, char c) {
  return s->write(s, &c, 1);
}

#endif /* SINK_H */


/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:2f4f2c2a4e65aef8f7c59b4785121083 */
