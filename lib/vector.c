/*
 * This file is part of DisOrder.
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

#include <config.h>
#include "types.h"

#include <stddef.h>
#include <string.h>

#include "mem.h"
#include "log.h"
#include "vector.h"

void vector_append_many(struct vector *v, char **vec, int nvec) {
  while(nvec-- > 0)
    vector_append(v, *vec++);
}

void dynstr_append_bytes(struct dynstr *v, const char *ptr, size_t n) {
  while(n > 0) {
    dynstr_append(v, *ptr++);
    n--;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
