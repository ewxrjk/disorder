/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/kvp.c
 * @brief Linked list of key-value pairs
 *
 * Also supports URL encoding/decoding (of raw strings and kvp lists).
 *
 * For large sets of keys, see @ref lib/hash.c.
 */

#include "common.h"

#include "mem.h"
#include "kvp.h"
#include "log.h"
#include "vector.h"
#include "hex.h"
#include "sink.h"

/** @brief Decode a URL-encoded string to a ink
 * @param sink Where to store result
 * @param ptr Start of string
 * @param n Length of string
 * @return 0 on success, non-0 if string could not be decoded or sink write failed
 */
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

/** @brief URL-decode a string
 * @param ptr Start of URL-encoded string
 * @param n Length of @p ptr
 * @return Decoded string (0-terminated)
 */
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

/** @brief Decode a URL-decoded key-value pair list
 * @param ptr Start of input string
 * @param n Length of input string
 * @return @ref kvp of values from input
 *
 * The KVP is in the same order as the original input.
 *
 * If the original input contains duplicates names, so will the KVP.
 */
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

/** @brief URL-encode a string to a sink
 * @param sink Where to send output
 * @param s String to encode
 * @param n Length of string to encode
 * @return 0 on success or non-0 if sink write failed
 */
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

/** @brief URL-encode a KVP
 * @param kvp Linked list to encode
 * @param np Where to store length (or NULL)
 * @return Newly created string
 */
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

/** @brief Set or remove a value in a @ref kvp
 * @param kvpp Address of KVP head to modify
 * @param name Key to search for
 * @param value New value or NULL to delete
 * @return 1 if any change was made otherwise 0
 *
 * If @p value is not NULL then the first matching key is replaced; if
 * there was no matching key a new one is added at the end.
 *
 * If @p value is NULL then the first matching key is removed.
 *
 * If anything actually changes the return value is 1.  If no actual
 * change is made then 0 is returned instead.
 */
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

/** @brief Look up a value in a @ref kvp
 * @param kvp Head of KVP linked list
 * @param name Key to search for
 * @return Value or NULL
 *
 * The returned value is owned by the KVP so must not be modified or
 * freed.
 */
const char *kvp_get(const struct kvp *kvp, const char *name) {
  for(;kvp && strcmp(kvp->name, name); kvp = kvp->next)
    ;
  return kvp ? kvp->value : 0;
}

/** @brief Construct a KVP from arguments
 * @param name First name
 * @return Newly created KVP
 *
 * Arguments must come in name/value pairs and must be followed by a (char *)0.
 *
 * The order of the new KVP is not formally defined though the test
 * programs rely on it nonetheless so update them if you change it.
 */
struct kvp *kvp_make(const char *name, ...) {
  const char *value;
  struct kvp *kvp = 0, *k;
  va_list ap;

  va_start(ap, name);
  while(name) {
    value = va_arg(ap, const char *);
    k = xmalloc(sizeof *k);
    k->name = name;
    k->value = value ? xstrdup(value) : "";
    k->next = kvp;
    kvp = k;
    name = va_arg(ap, const char *);
  }
  va_end(ap);
  return kvp;
}

void kvp_free(struct kvp *k) {
  if(k) {
    kvp_free(k->next);
    xfree((void *)k->name);
    xfree((void *)k->value);
    xfree(k);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
