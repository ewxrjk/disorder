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
/** @file lib/fprintf.c
 * @brief UTF-8 workalike for fprintf() and vfprintf()
 */

#include "common.h"

#include <stdarg.h>
#include <stddef.h>

#include "printf.h"
#include "sink.h"

int byte_vfprintf(FILE *fp, const char *fmt, va_list ap) {
  return byte_vsinkprintf(sink_stdio(0, fp), fmt, ap);
}

int byte_fprintf(FILE *fp, const char *fmt, ...) {
  int n;
  va_list ap;

  va_start(ap, fmt);
  n = byte_vfprintf(fp, fmt, ap);
  va_end(ap);
  return n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
