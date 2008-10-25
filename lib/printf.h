/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2006-2008 Richard Kettlewell
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
/** @file lib/printf.h
 * @brief UTF-8 *printf workalikes
 */
#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>

struct sink;

int byte_vsinkprintf(struct sink *output,
		     const char *fmt,
		     va_list ap);
int byte_sinkprintf(struct sink *output, const char *fmt, ...);
/* partial printf implementation that takes ASCII format strings but
 * arbitrary byte strings as args to %s and friends.  Lots of bits are
 * missing! */

int byte_vsnprintf(char buffer[],
		   size_t bufsize,
		   const char *fmt,
		   va_list ap);
int byte_snprintf(char buffer[],
		  size_t bufsize,
		  const char *fmt,
		  ...)
  attribute((format (printf, 3, 4)));
/* analogues of [v]snprintf */

int byte_vasprintf(char **ptrp,
		   const char *fmt,
		   va_list ap);
int byte_asprintf(char **ptrp,
		  const char *fmt,
		  ...)
  attribute((format (printf, 2, 3)));
/* analogues of [v]asprintf (uses xmalloc/xrealloc) */

int byte_xvasprintf(char **ptrp,
		    const char *fmt,
		    va_list ap);
int byte_xasprintf(char **ptrp,
		   const char *fmt,
		   ...)
  attribute((format (printf, 2, 3)));
/* same but terminate on error */

int byte_vfprintf(FILE *fp, const char *fmt, va_list ap);
int byte_fprintf(FILE *fp, const char *fmt, ...)
  attribute((format (printf, 2, 3)));
/* analogues of [v]fprintf */

#endif /* PRINTF_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
