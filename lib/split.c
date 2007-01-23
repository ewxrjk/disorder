/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2006 Richard Kettlewell
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

#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "mem.h"
#include "split.h"
#include "log.h"
#include "charset.h"
#include "vector.h"

static inline int space(int c) {
  return (c == ' '
	  || c == '\t'
	  || c == '\n'
	  || c == '\r');
}

static void no_error_handler(const char attribute((unused)) *msg,
			     void attribute((unused)) *u) {
}

char **split(const char *p,
	     int *np,
	     unsigned flags,
	     void (*error_handler)(const char *msg, void *u),
	     void *u) {
  char *f, *g;
  const char *q;
  struct vector v;
  size_t l;
  int qc;

  if(!error_handler) error_handler = no_error_handler;
  vector_init(&v);
  while(*p && !(*p == '#' && (flags & SPLIT_COMMENTS))) {
    if(space(*p)) {
      ++p;
      continue;
    }
    if((flags & SPLIT_QUOTES) && (*p == '"' || *p == '\'')) {
      qc = *p++;
      l = 0;
      for(q = p; *q && *q != qc; ++q) {
	if(*q == '\\' && q[1])
	  ++q;
	++l;
      }
      if(!*q) {
	error_handler("unterminated quoted string", u);
	return 0;
      }
      f = g = xmalloc_noptr(l + 1);
      for(q = p; *q != qc;) {
	if(*q == '\\') {
	  ++q;
	  switch(*q) {
	  case '\\':
	  case '"':
	  case '\'':
	    *g++ = *q++;
	    break;
	  case 'n':
	    ++q;
	    *g++ = '\n';
	    break;
	  default:
	    error_handler("illegal escape sequence", u);
	    return 0;
	  }
	} else
	  *g++ = *q++;
      }
      *g = 0;
      p = q + 1;
    } else {
      for(q = p; *q && !space(*q); ++q)
	;
      l = q - p;
      f = xstrndup(p, l);
      p = q;
    }
    vector_append(&v, f);
  }
  vector_terminate(&v);
  if(np)
    *np = v.nvec;
  return v.vec;
}

const char *quoteutf8(const char *s) {
  size_t len = 3 + strlen(s);
  const char *t;
  char *r, *q;

  /* see if we need to quote */
  if(*s) {
    for(t = s; *t; t++)
      if((unsigned char)*t <= ' '
	 || *t == '"'
	 || *t == '\\'
	 || *t == '\''
	 || *t == '#')
	break;
    if(!*t)
      return s;
  }

  /* we rely on ASCII characters only ever representing themselves in UTF-8. */
  for(t = s; *t; t++) {
    switch(*t) {
    case '"':
    case '\\':
    case '\n':
      ++len;
      break;
    }
  }
  q = r = xmalloc_noptr(len);
  *q++ = '"';
  for(t = s; *t; t++) {
    switch(*t) {
    case '"':
    case '\\':
      *q++ = '\\';
      /* fall through */
    default:
      *q++ = *t;
      break;
    case '\n':
      *q++ = '\\';
      *q++ = 'n';
      break;
    }
  }
  *q++ = '"';
  *q = 0;
  return r;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
