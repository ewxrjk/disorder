/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008, 2009 Richard Kettlewell
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
/** @file server/api.c
 * @brief Generic API
 *
 * These functions are made available to all kinds of plugins.
 */
#include "disorder-server.h"

void *disorder_malloc(size_t n) {
  return xmalloc(n);
}

void *disorder_realloc(void *p, size_t n) {
  return xrealloc(p, n);
}

void *disorder_malloc_noptr(size_t n) {
  return xmalloc_noptr(n);
}

void *disorder_realloc_noptr(void *p, size_t n) {
  return xrealloc_noptr(p, n);
}

char *disorder_strdup(const char *p) {
  return xstrdup(p);
}

char *disorder_strndup(const char *p, size_t n) {
  return xstrndup(p, n);
}

int disorder_snprintf(char buffer[], size_t bufsize, const char *fmt, ...) {
  int n;
  va_list ap;

  va_start(ap, fmt);
  n = byte_vsnprintf(buffer, bufsize, fmt, ap);
  va_end(ap);
  return n;
}

int disorder_asprintf(char **rp, const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = byte_vasprintf(rp, fmt, ap);
  va_end(ap);
  return n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
