/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "vector.h"
#include "printf.h"
#include "eventlog.h"
#include "split.h"

static struct eventlog_output *outputs;

void eventlog_add(struct eventlog_output *lo) {
  lo->next = outputs;
  outputs = lo;
}

void eventlog_remove(struct eventlog_output *lo) {
  struct eventlog_output *p, **pp;

  for(pp = &outputs; (p = *pp) && p != lo; pp = &p->next)
    ;
  if(p == lo)
    *pp = lo->next;
}

static void veventlog(const char *keyword, const char *raw, va_list ap) {
  struct eventlog_output *p;
  struct dynstr d;
  const char *param;

  dynstr_init(&d);
  dynstr_append_string(&d, keyword);
  while((param = va_arg(ap, const char *))) {
    dynstr_append(&d, ' ');
    dynstr_append_string(&d, quoteutf8(param));
  }
  if(raw) {
    dynstr_append(&d, ' ');
    dynstr_append_string(&d, raw);
  }
  dynstr_terminate(&d);
  for(p = outputs; p; p = p->next)
    p->fn(d.vec, p->user);
}

void eventlog_raw(const char *keyword, const char *raw, ...) {
  va_list ap;

  va_start(ap, raw);
  veventlog(keyword, raw, ap);
  va_end(ap);
}

void eventlog(const char *keyword, ...) {
  va_list ap;

  va_start(ap, keyword);
  veventlog(keyword, 0, ap);
  va_end(ap);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
