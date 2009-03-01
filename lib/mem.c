/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Richard Kettlewell
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
/** @file lib/mem.c
 * @brief Memory management
 */

#include "common.h"

#if GC
#include <gc.h>
#endif
#include <errno.h>

#include "mem.h"
#include "log.h"
#include "printf.h"

#include "disorder.h"

/** @brief Allocate and zero out
 * @param n Number of bytes to allocate
 * @return Pointer to allocated memory,  or 0
 */
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

/** @brief Initialize memory management
 *
 * Must be called by all programs that use garbage collection.  Define
 * @c ${DISORDER_GC} to @c no to suppress use of the collector
 * (e.g. for debugging purposes).
 */
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

/** @brief Allocate memory
 * @param n Bytes to allocate
 * @return Pointer to allocated memory
 *
 * Terminates the process on error.  The allocated memory is always
 * 0-filled.
 */
void *xmalloc(size_t n) {
  void *ptr;

  if(!(ptr = do_malloc(n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

/** @brief Reallocate memory
 * @param ptr Block to reallocated
 * @param n Bytes to allocate
 * @return Pointer to allocated memory
 *
 * Terminates the process on error.  It is NOT guaranteed that any
 * additional memory allocated is 0-filled.
 */
void *xrealloc(void *ptr, size_t n) {
  if(!(ptr = do_realloc(ptr, n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

/** @brief Allocate memory
 * @param count Number of objects to allocate
 * @param size Size of one object
 * @return Pointer to allocated memory
 *
 * Terminates the process on error.  The allocated memory is always
 * 0-filled.
 */
void *xcalloc(size_t count, size_t size) {
  if(count > SIZE_MAX / size)
    fatal(0, "excessively large calloc");
  return xmalloc(count * size);
}

/** @brief Allocate memory
 * @param n Bytes to allocate
 * @return Pointer to allocated memory
 *
 * Terminates the process on error.  The allocated memory is not
 * guaranteed to be 0-filled and is not suitable for storing pointers
 * in.
 */
void *xmalloc_noptr(size_t n) {
  void *ptr;

  if(!(ptr = do_malloc_atomic(n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

/** @brief Allocate memory
 * @param count Number of objects to allocate
 * @param size Size of one object
 * @return Pointer to allocated memory
 *
 * Terminates the process on error.  IMPORTANT: the allocated memory is NOT
 * 0-filled (unlike @c calloc()).
 */
void *xcalloc_noptr(size_t count, size_t size) {
  if(count > SIZE_MAX / size)
    fatal(0, "excessively large calloc");
  return xmalloc_noptr(count * size);
}

/** @brief Reallocate memory
 * @param ptr Block to reallocated
 * @param n Bytes to allocate
 * @return Pointer to allocated memory
 *
 * Terminates the processf on error.  It is NOT guaranteed that any
 * additional memory allocated is 0-filled.  The block must have been
 * allocated with xmalloc_noptr() (or xrealloc_noptr()) initially.
 */
void *xrealloc_noptr(void *ptr, size_t n) {
  if(ptr == 0)
    return xmalloc_noptr(n);
  if(!(ptr = do_realloc(ptr, n)) && n)
    fatal(errno, "error allocating memory");
  return ptr;
}

/** @brief Duplicate a string
 * @param s String to copy
 * @return New copy of string
 *
 * This uses the equivalent of xmalloc_noptr() to allocate the new string.
 */
char *xstrdup(const char *s) {
  char *t;

  if(!(t = do_malloc_atomic(strlen(s) + 1)))
    fatal(errno, "error allocating memory");
  return strcpy(t, s);
}

/** @brief Duplicate a prefix of a string
 * @param s String to copy
 * @param n Prefix of string to copy
 * @return New copy of string
 *
 * This uses the equivalent of xmalloc_noptr() to allocate the new string.
 * @p n must not exceed the length of the string.
 */
char *xstrndup(const char *s, size_t n) {
  char *t;

  if(!(t = do_malloc_atomic(n + 1)))
    fatal(errno, "error allocating memory");
  memcpy(t, s, n);
  t[n] = 0;
  return t;
}

/** @brief Free memory
 * @param ptr Block to free or 0
 */
void xfree(void *ptr) {
  do_free(ptr);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
