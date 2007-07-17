/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

#if GC
#include <gc.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mem.h"
#include "log.h"
#include "printf.h"

#include "disorder.h"

static void *malloc_and_zero(size_t n) {
  void *ptr = malloc(n);

  if(ptr) memset(ptr, 0, n);
  return ptr;
}

#if GC
static void *(*do_malloc)(size_t) = GC_malloc;
static void *(*do_realloc)(void *, size_t) = GC_realloc;
static void *(*do_malloc_atomic)(size_t) = GC_malloc_atomic;
static void (*do_free)(void *) = GC_free;
#else
static void *(*do_malloc)(size_t) = malloc_and_zero;
static void *(*do_realloc)(void *, size_t) = realloc;
static void *(*do_malloc_atomic)(size_t) = malloc;
static void (*do_free)(void *) = free;
#endif

void mem_init(void) {
#if GC
  const char *e;
  
  if(((e = getenv("DISORDER_GC")) && !strcmp(e, "no"))) {
    do_malloc = malloc_and_zero;
    do_malloc_atomic = malloc;
    do_realloc = realloc;
    do_free = free;
  } else {
    GC_init();
    assert(GC_all_interior_pointers);
  }
#endif
}

void *xmalloc(size_t n) {
  void *ptr;

  if(!(ptr = do_malloc(n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

void *xrealloc(void *ptr, size_t n) {
  if(!(ptr = do_realloc(ptr, n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

void *xcalloc(size_t count, size_t size) {
  if(count > SIZE_MAX / size)
    fatal(0, "excessively large calloc");
  return xmalloc(count * size);
}

void *xmalloc_noptr(size_t n) {
  void *ptr;

  if(!(ptr = do_malloc_atomic(n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

void *xrealloc_noptr(void *ptr, size_t n) {
  if(ptr == 0)
    return xmalloc_noptr(n);
  if(!(ptr = do_realloc(ptr, n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

char *xstrdup(const char *s) {
  char *t;

  if(!(t = do_malloc_atomic(strlen(s) + 1)))
    fatal(errno, "error allocating memory");
  return strcpy(t, s);
}

char *xstrndup(const char *s, size_t n) {
  char *t;

  if(!(t = do_malloc_atomic(n + 1)))
    fatal(errno, "error allocating memory");
  memcpy(t, s, n);
  t[n] = 0;
  return t;
}

void xfree(void *ptr) {
  do_free(ptr);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
