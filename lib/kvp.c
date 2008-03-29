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

#include <config.h>
#include "types.h"

#include <string.h>
#include <stdio.h>

#include "mem.h"
#include "kvp.h"
#include "log.h"
#include "vector.h"
#include "hex.h"
#include "sink.h"

int urldecode(struct sink *sink, const char *ptr, size_t n) {
  int c, d1, d2;
  
  while(n-- > 0) {
    switch(c = *ptr++) {
    case '%':
      if((d1 = unhexdigit(ptr[0])) == -1
	 || (d2 = unhexdigit(ptr[1])) == -1)
	return -1;
      c  = d1 * 16 + d2;
      ptr += 2;
      n -= 2;
      break;
    case '+':
      c = ' ';
      break;
    default:
      break;
    }
    if(sink_writec(sink,c) < 0)
      return -1;
  }
  return 0;
}

static char *decode(const char *ptr, size_t n) {
  struct dynstr d;
  struct sink *s;

  dynstr_init(&d);
  s = sink_dynstr(&d);
  if(urldecode(s, ptr, n))
    return 0;
  dynstr_terminate(&d);
  return d.vec;
}

struct kvp *kvp_urldecode(const char *ptr, size_t n) {
  struct kvp *kvp, **kk = &kvp, *k;
  const char *q, *r, *top = ptr + n, *next;

  while(ptr < top) {
    *kk = k = xmalloc(sizeof *k);
    if(!(q = memchr(ptr, '=', top - ptr)))
      break;
    if(!(k->name = decode(ptr, q - ptr))) break;
    if((r = memchr(ptr, '&', top - ptr)))
      next = r + 1;
    else
      next = r = top;
    if(r < q)
      break;
    if(!(k->value = decode(q + 1, r - (q + 1)))) break;
    kk = &k->next;
    ptr = next;
  }
  *kk = 0;
  return kvp;
}

int urlencode(struct sink *sink, const char *s, size_t n) {
  unsigned char c;

  while(n > 0) {
    c = *s++;
    n--;
    switch(c) {
    default:
      if((c >= '0' && c <= '9')
	 || (c >= 'a' && c <= 'z')
	 || (c >= 'A' && c <= 'Z')) {
	/* RFC2396 2.3 unreserved characters */
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '\'':
      case '(':
      case ')':
	/* additional unreserved characters */
      case '/':
	if(sink_writec(sink, c) < 0)
	  return -1;
      } else
	if(sink_printf(sink, "%%%02x", (unsigned int)c) < 0)
	  return -1;
    }
  }
  return 0;
}

/** @brief URL-encode @p s
 * @param s String to encode
 * @return Encoded string
 */
char *urlencodestring(const char *s) {
  struct dynstr d;

  dynstr_init(&d);
  urlencode(sink_dynstr(&d), s, strlen(s));
  dynstr_terminate(&d);
  return d.vec;
}

/** @brief URL-decode @p s
 * @param s String to decode
 * @param ns Length of string
 * @return Decoded string or NULL
 */
char *urldecodestring(const char *s, size_t ns) {
  struct dynstr d;

  dynstr_init(&d);
  if(urldecode(sink_dynstr(&d), s, ns))
    return NULL;
  dynstr_terminate(&d);
  return d.vec;
}

char *kvp_urlencode(const struct kvp *kvp, size_t *np) {
  struct dynstr d;
  struct sink *sink;

  dynstr_init(&d);
  sink = sink_dynstr(&d);
  while(kvp) {
    urlencode(sink, kvp->name, strlen(kvp->name));
    dynstr_append(&d, '=');
    urlencode(sink, kvp->value, strlen(kvp->value));
    if((kvp = kvp->next))
      dynstr_append(&d, '&');
    
  }
  dynstr_terminate(&d);
  if(np)
    *np = d.nvec;
  return d.vec;
}

int kvp_set(struct kvp **kvpp, const char *name, const char *value) {
  struct kvp *k, **kk;

  for(kk = kvpp; (k = *kk) && strcmp(name, k->name); kk = &k->next)
    ;
  if(k) {
    if(value) {
      if(strcmp(k->value, value)) {
	k->value = xstrdup(value);
	return 1;
      } else
	return 0;
    } else {
      *kk = k->next;
      return 1;
    }
  } else {
    if(value) {
      *kk = k = xmalloc(sizeof *k);
      k->name = xstrdup(name);
      k->value = xstrdup(value);
      return 1;
    } else
      return 0;
  }
}

const char *kvp_get(const struct kvp *kvp, const char *name) {
  for(;kvp && strcmp(kvp->name, name); kvp = kvp->next)
    ;
  return kvp ? kvp->value : 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
