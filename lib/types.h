/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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

#ifndef TYPES_H
#define TYPES_H

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <limits.h>
#include <sys/types.h>

/* had better be before atol/atoll redefinition */
#include <stdlib.h>

#if HAVE_LONG_LONG
typedef long long long_long;
typedef unsigned long long u_long_long;
# if ! DECLARES_STRTOLL
long long strtoll(const char *, char **, int);
# endif
# if ! DECLARES_ATOLL
long long atoll(const char *);
# endif
#else
typedef long long_long;
typedef unsigned long u_long_long;
# define atoll atol
# define strtoll strtol
#endif

#if __APPLE__
/* apple define these to j[dxu], which gcc -std=c99 -pedantic then rejects */
# undef PRIdMAX
# undef PRIxMAX
# undef PRIuMAX
#endif

#if HAVE_INTMAX_T
# ifndef PRIdMAX
#  define PRIdMAX "jd"
# endif
#elif HAVE_LONG_LONG
typedef long long intmax_t;
# define PRIdMAX "lld"
#else
typedef long intmax_t;
# define PRIdMAX "ld"
#endif

#if HAVE_UINTMAX_T
# ifndef PRIuMAX
#  define PRIuMAX "ju"
# endif
# ifndef PRIxMAX
#  define PRIxMAX "jx"
# endif
#elif HAVE_LONG_LONG
typedef unsigned long long uintmax_t;
# define PRIuMAX "llu"
# define PRIxMAX "llx"
#else
typedef unsigned long uintmax_t;
# define PRIuMAX "lu"
# define PRIxMAX "lx"
#endif

#if ! HAVE_UINT8_T
# if CHAR_BIT == 8
typedef unsigned char uint8_t;
# else
#  error cannot determine uint8_t
# endif
#endif

#if ! HAVE_UINT32_T
# if UINT_MAX == 4294967295
typedef unsigned int uint32_t;
# elif ULONG_MAX == 4294967295
typedef unsigned long uint32_t;
# elif USHRT_MAX == 4294967295
typedef unsigned short uint32_t;
# elif UCHAR_MAX == 4294967295
typedef unsigned char uint32_t;
# else
#  error cannot determine uint32_t
# endif
#endif

#endif /* TYPES_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
