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

#include <string.h>
#include <ctype.h>

#include <stdio.h>

#include "mem.h"
#include "mime.h"
#include "vector.h"
#include "hex.h"

static int whitespace(int c) {
  switch(c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return 1;
  default:
    return 0;
  }
}

static int tspecial(int c) {
  switch(c) {
  case '(':
  case ')':
  case '<':
  case '>':
  case '@':
  case ',':
  case ';':
  case ':':
  case '\\':
  case '"':
  case '/':
  case '[':
  case ']':
  case '?':
  case '=':
    return 1;
  default:
    return 0;
  }
}

static const char *skipwhite(const char *s) {
  int c, depth;
  
  for(;;) {
    switch(c = *s) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      ++s;
      break;
    case '(':
      ++s;
      depth = 1;
      while(*s && depth) {
	c = *s++;
	switch(c) {
	case '(': ++depth; break;
	case ')': --depth; break;
	case '\\':
	  if(!*s) return 0;
	  ++s;
	  break;
	}
      }
      if(depth) return 0;
      break;
    default:
      return s;
    }
  }
}

static const char *parsestring(const char *s, char **valuep) {
  struct dynstr value;
  int c;

  dynstr_init(&value);
  ++s;
  while((c = *s++) != '"') {
    switch(c) {
    case '\\':
      if(!(c = *s++)) return 0;
    default:
      dynstr_append(&value, c);
      break;
    }
  }
  if(!c) return 0;
  dynstr_terminate(&value);
  *valuep = value.vec;
  return s;
}

int mime_content_type(const char *s,
		      char **typep,
		      char **parameternamep,
		      char **parametervaluep) {
  struct dynstr type, parametername, parametervalue;

  dynstr_init(&type);
  if(!(s = skipwhite(s))) return -1;
  if(!*s) return -1;
  while(*s && !tspecial(*s) && !whitespace(*s))
    dynstr_append(&type, tolower((unsigned char)*s++));
  if(!(s = skipwhite(s))) return -1;
  if(*s++ != '/') return -1;
  dynstr_append(&type, '/');
  if(!(s = skipwhite(s))) return -1;
  while(*s && !tspecial(*s) && !whitespace(*s))
    dynstr_append(&type, tolower((unsigned char)*s++));
  if(!(s = skipwhite(s))) return -1;

  if(*s == ';') {
    dynstr_init(&parametername);
    ++s;
    if(!(s = skipwhite(s))) return -1;
    if(!*s) return -1;
    while(*s && !tspecial(*s) && !whitespace(*s))
      dynstr_append(&parametername, tolower((unsigned char)*s++));
    if(!(s = skipwhite(s))) return -1;
    if(*s++ != '=') return -1;
    if(!(s = skipwhite(s))) return -1;
    if(*s == '"') {
      if(!(s = parsestring(s, parametervaluep))) return -1;
    } else {
      dynstr_init(&parametervalue);
      while(*s && !tspecial(*s) && !whitespace(*s))
	dynstr_append(&parametervalue, *s++);
      dynstr_terminate(&parametervalue);
      *parametervaluep = parametervalue.vec;
    }
    if(!(s = skipwhite(s))) return -1;
    dynstr_terminate(&parametername);
    *parameternamep = parametername.vec;
  } else
    *parametervaluep = *parameternamep = 0;
  dynstr_terminate(&type);
  *typep = type.vec;
  return 0;
}

static int iscrlf(const char *ptr) {
  return ptr[0] == '\r' && ptr[1] == '\n';
}

const char *mime_parse(const char *s,
		       int (*callback)(const char *name, const char *value,
				       void *u),
		       void *u) {
  struct dynstr name, value;
  char *cte = 0, *p;
  
  while(*s && !iscrlf(s)) {
    dynstr_init(&name);
    dynstr_init(&value);
    while(*s && !tspecial(*s) && !whitespace(*s))
      dynstr_append(&name, tolower((unsigned char)*s++));
    if(!(s = skipwhite(s))) return 0;
    if(*s != ':') return 0;
    ++s;
    while(*s && !(*s == '\n' && !(s[1] == ' ' || s[1] == '\t')))
      dynstr_append(&value, *s++);
    if(*s) ++s;
    dynstr_terminate(&name);
    dynstr_terminate(&value);
    if(!strcmp(name.vec, "content-transfer-encoding")) {
      cte = xstrdup(value.vec);
      for(p = cte; *p; p++)
	*p = tolower((unsigned char)*p);
    }
    if(callback(name.vec, value.vec, u)) return 0;
  }
  if(*s) s += 2;
  if(cte) {
    if(!strcmp(cte, "base64")) return mime_base64(s);
    if(!strcmp(cte, "quoted-printable")) return mime_qp(s);
  }
  return s;
}

static int isboundary(const char *ptr, const char *boundary, size_t bl) {
  return (ptr[0] == '-'
	  && ptr[1] == '-'
	  && !strncmp(ptr + 2, boundary, bl)
	  && (iscrlf(ptr + bl + 2)
	      || (ptr[bl + 2] == '-'
		  && ptr[bl + 3] == '-'
		  && (iscrlf(ptr + bl + 4) || *(ptr + bl + 4) == 0))));
}

static int isfinal(const char *ptr, const char *boundary, size_t bl) {
  return (ptr[0] == '-'
	  && ptr[1] == '-'
	  && !strncmp(ptr + 2, boundary, bl)
	  && ptr[bl + 2] == '-'
	  && ptr[bl + 3] == '-'
	  && (iscrlf(ptr + bl + 4) || *(ptr + bl + 4) == 0));
}

int mime_multipart(const char *s,
		   int (*callback)(const char *s, void *u),
		   const char *boundary,
		   void *u) {
  size_t bl = strlen(boundary);
  const char *start, *e;
  int ret;

  /* We must start with a boundary string */
  if(!isboundary(s, boundary, bl))
    return -1;
  /* Keep going until we hit a final boundary */
  while(!isfinal(s, boundary, bl)) {
    s = strstr(s, "\r\n") + 2;
    start = s;
    while(!isboundary(s, boundary, bl)) {
      if(!(e = strstr(s, "\r\n")))
	return -1;
      s = e + 2;
    }
    if((ret = callback(xstrndup(start,
				s == start ? 0 : s - start - 2),
		       u)))
      return ret;
  }
  return 0;
}

int mime_rfc2388_content_disposition(const char *s,
				     char **dispositionp,
				     char **parameternamep,
				     char **parametervaluep) {
  struct dynstr disposition, parametername, parametervalue;

  dynstr_init(&disposition);
  if(!(s = skipwhite(s))) return -1;
  if(!*s) return -1;
  while(*s && !tspecial(*s) && !whitespace(*s))
    dynstr_append(&disposition, tolower((unsigned char)*s++));
  if(!(s = skipwhite(s))) return -1;

  if(*s == ';') {
    dynstr_init(&parametername);
    ++s;
    if(!(s = skipwhite(s))) return -1;
    if(!*s) return -1;
    while(*s && !tspecial(*s) && !whitespace(*s))
      dynstr_append(&parametername, tolower((unsigned char)*s++));
    if(!(s = skipwhite(s))) return -1;
    if(*s++ != '=') return -1;
    if(!(s = skipwhite(s))) return -1;
    if(*s == '"') {
      if(!(s = parsestring(s, parametervaluep))) return -1;
    } else {
      dynstr_init(&parametervalue);
      while(*s && !tspecial(*s) && !whitespace(*s))
	dynstr_append(&parametervalue, *s++);
      dynstr_terminate(&parametervalue);
      *parametervaluep = parametervalue.vec;
    }
    if(!(s = skipwhite(s))) return -1;
    dynstr_terminate(&parametername);
    *parameternamep = parametername.vec;
  } else
    *parametervaluep = *parameternamep = 0;
  dynstr_terminate(&disposition);
  *dispositionp = disposition.vec;
  return 0;
}

char *mime_qp(const char *s) {
  struct dynstr d;
  int c, a, b;
  const char *t;

  dynstr_init(&d);
  while((c = *s++)) {
    switch(c) {
    case '=':
      if((a = unhexdigitq(s[0])) != -1
	 && (b = unhexdigitq(s[1])) != -1) {
	dynstr_append(&d, a * 16 + b);
	s += 2;
      } else {
	t = s;
	while(*t == ' ' || *t == '\t') ++t;
	if(iscrlf(t)) {
	  /* soft line break */
	  s = t + 2;
	} else
	  return 0;
      }
      break;
    case ' ':
    case '\t':
      t = s;
      while(*t == ' ' || *t == '\t') ++t;
      if(iscrlf(t))
	/* trailing space is always eliminated */
	s = t;
      else
	dynstr_append(&d, c);
      break;
    default:
      dynstr_append(&d, c);
      break;
    }
  }
  dynstr_terminate(&d);
  return d.vec;
}

char *mime_base64(const char *s) {
  struct dynstr d;
  const char *t;
  int b[4], n, c;
  static const char table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  dynstr_init(&d);
  n = 0;
  while((c = (unsigned char)*s++)) {
    if((t = strchr(table, c))) {
      b[n++] = t - table;
      if(n == 4) {
	dynstr_append(&d, (b[0] << 2) + (b[1] >> 4));
	dynstr_append(&d, (b[1] << 4) + (b[2] >> 2));
	dynstr_append(&d, (b[2] << 6) + b[3]);
	n = 0;
      }
    } else if(c == '=') {
      if(n >= 2) {
	dynstr_append(&d, (b[0] << 2) + (b[1] >> 4));
	if(n == 3)
	  dynstr_append(&d, (b[1] << 4) + (b[2] >> 2));
      }
      break;
    }
  }
  dynstr_terminate(&d);
  return d.vec;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
