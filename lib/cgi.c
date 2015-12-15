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
/** @file lib/cgi.c
 * @brief CGI tools
 */

#include "common.h"

#include <unistd.h>
#include <errno.h>

#include "cgi.h"
#include "mem.h"
#include "log.h"
#include "vector.h"
#include "hash.h"
#include "kvp.h"
#include "mime.h"
#include "unicode.h"
#include "sink.h"

/** @brief Hash of arguments */
static hash *cgi_args;

/** @brief Get CGI arguments from a GET request's query string */
static struct kvp *cgi__init_get(void) {
  const char *q;

  if((q = getenv("QUERY_STRING")))
    return kvp_urldecode(q, strlen(q));
  disorder_error(0, "QUERY_STRING not set, assuming empty");
  return NULL;
}

/** @brief Read the HTTP request body */
static void cgi__input(char **ptrp, size_t *np) {
  const char *cl;
  char *q;
  size_t n, m = 0;
  int r;

  if(!(cl = getenv("CONTENT_LENGTH")))
    disorder_fatal(0, "CONTENT_LENGTH not set");
  n = atol(cl);
  /* We check for overflow and also limit the input to 16MB. Lower
   * would probably do.  */
  if(!(n+1) || n > 16 * 1024 * 1024)
    disorder_fatal(0, "input is much too large");
  q = xmalloc_noptr(n + 1);
  while(m < n) {
    r = read(0, q + m, n - m);
    if(r > 0)
      m += r;
    else if(r == 0)
      disorder_fatal(0, "unexpected end of file reading request body");
    else switch(errno) {
    case EINTR: break;
    default: disorder_fatal(errno, "error reading request body");
    }
  }
  if(memchr(q, 0, n))
    disorder_fatal(0, "null character in request body");
  q[n] = 0;
  *ptrp = q;
  if(np)
    *np = n;
}

/** @brief Called for each part header field (see cgi__part_callback()) */
static int cgi__field_callback(const char *name, const char *value,
			       void *u) {
  char *disposition, *pname, *pvalue;
  char **namep = u;

  if(!strcmp(name, "content-disposition")) {
    if(mime_rfc2388_content_disposition(value,
					&disposition,
					&pname,
					&pvalue))
      disorder_fatal(0, "error parsing Content-Disposition field");
    if(!strcmp(disposition, "form-data")
       && pname
       && !strcmp(pname, "name")) {
      if(*namep)
	disorder_fatal(0, "duplicate Content-Disposition field");
      *namep = pvalue;
    }
  }
  return 0;
}

/** @brief Called for each part (see cgi__init_multipart()) */
static int cgi__part_callback(const char *s,
			      void *u) {
  char *name = 0;
  struct kvp *k, **head = u;

  if(!(s = mime_parse(s, cgi__field_callback, &name)))
    disorder_fatal(0, "error parsing part header");
  if(!name)
    disorder_fatal(0, "no name found");
  k = xmalloc(sizeof *k);
  k->next = *head;
  k->name = name;
  k->value = s;
  *head = k;
  return 0;
}

/** @brief Initialize CGI arguments from a multipart/form-data request body */
static struct kvp *cgi__init_multipart(const char *boundary) {
  char *q;
  struct kvp *head = 0;
  
  cgi__input(&q, 0);
  if(mime_multipart(q, cgi__part_callback, boundary, &head))
    disorder_fatal(0, "invalid multipart object");
  return head;
}

/** @brief Initialize CGI arguments from a POST request */
static struct kvp *cgi__init_post(void) {
  const char *ct, *boundary;
  char *q, *type;
  size_t n;
  struct kvp *k;

  if(!(ct = getenv("CONTENT_TYPE")))
    ct = "application/x-www-form-urlencoded";
  if(mime_content_type(ct, &type, &k))
    disorder_fatal(0, "invalid content type '%s'", ct);
  if(!strcmp(type, "application/x-www-form-urlencoded")) {
    cgi__input(&q, &n);
    return kvp_urldecode(q, n);
  }
  if(!strcmp(type, "multipart/form-data")) {
    if(!(boundary = kvp_get(k, "boundary")))
      disorder_fatal(0, "no boundary parameter found");
    return cgi__init_multipart(boundary);
  }
  disorder_fatal(0, "unrecognized content type '%s'", type);
}

/** @brief Initialize CGI arguments
 *
 * Must be called before other cgi_ functions are used.
 *
 * This function can be called more than once, in which case it
 * revisits the environment and (perhaps) standard input.  This is
 * only intended to be used for testing, actual CGI applications
 * should call it exactly once.
 */
void cgi_init(void) {
  const char *p;
  struct kvp *k;

  cgi_args = hash_new(sizeof (char *));
  if(!(p = getenv("REQUEST_METHOD")))
    disorder_error(0, "REQUEST_METHOD not set, assuming GET");
  if(!p || !strcmp(p, "GET"))
    k = cgi__init_get();
  else if(!strcmp(p, "POST"))
    k = cgi__init_post();
  else
    disorder_fatal(0, "unknown request method %s", p);
  /* Validate the arguments and put them in a hash */
  for(; k; k = k->next) {
    if(!utf8_valid(k->name, strlen(k->name))
       || !utf8_valid(k->value, strlen(k->value)))
      disorder_error(0, "invalid UTF-8 sequence in cgi argument %s", k->name);
    else
      hash_add(cgi_args, k->name, &k->value, HASH_INSERT_OR_REPLACE);
    /* We just drop bogus arguments. */
  }
}

/** @brief Get a CGI argument by name
 *
 * cgi_init() must be called first.  Names and values are all valid
 * UTF-8 strings (and this is enforced at initialization time).
 */
const char *cgi_get(const char *name) {
  const char **v = hash_find(cgi_args, name);

  return v ? *v : NULL;
}

/** @brief Set a CGI argument */
void cgi_set(const char *name, const char *value) {
  value = xstrdup(value);
  hash_add(cgi_args, name, &value, HASH_INSERT_OR_REPLACE);
}

/** @brief Clear CGI arguments */
void cgi_clear(void) {
  cgi_args = hash_new(sizeof (char *));
}

/** @brief Add SGML-style quoting
 * @param src String to quote (UTF-8)
 * @return Quoted string
 *
 * Quotes characters for insertion into HTML output.  Anything that is
 * not a printable ASCII character will be converted to a numeric
 * character references, as will '"', '&', '<' and '>' (since those
 * have special meanings).
 *
 * Quoting everything down to ASCII means we don't care what the
 * content encoding really is (as long as it's not anything insane
 * like EBCDIC).
 */
char *cgi_sgmlquote(const char *src) {
  uint32_t *ucs, c;
  struct dynstr d[1];
  struct sink *s;

  if(!(ucs = utf8_to_utf32(src, strlen(src), 0)))
    exit(1);
  dynstr_init(d);
  s = sink_dynstr(d);
  /* format the string */
  while((c = *ucs++)) {
    switch(c) {
    default:
      if(c > 126 || c < 32) {
      case '"':
      case '&':
      case '<':
      case '>':
	/* For simplicity we always use numeric character references
	 * even if a named reference is available. */
	sink_printf(s, "&#%"PRIu32";", c);
	break;
      } else
	sink_writec(s, (char)c);
    }
  }
  dynstr_terminate(d);
  return d->vec;
}

/** @brief Construct a URL
 * @param url Base URL
 * @param ... Name/value pairs for constructed query string
 * @return Constructed URL
 *
 * The name/value pair list is terminated by a single (char *)0.
 */
char *cgi_makeurl(const char *url, ...) {
  va_list ap;
  struct kvp *kvp, *k, **kk = &kvp;
  struct dynstr d;
  const char *n, *v;
  
  dynstr_init(&d);
  dynstr_append_string(&d, url);
  va_start(ap, url);
  while((n = va_arg(ap, const char *))) {
    v = va_arg(ap, const char *);
    *kk = k = xmalloc(sizeof *k);
    kk = &k->next;
    k->name = n;
    k->value = v;
  }
  va_end(ap);
  *kk = 0;
  if(kvp) {
    dynstr_append(&d, '?');
    dynstr_append_string(&d, kvp_urlencode(kvp, 0));
  }
  dynstr_terminate(&d);
  return d.vec;
}

/** @brief Construct a URL from current parameters
 * @param url Base URL
 * @return Constructed URL
 */
char *cgi_thisurl(const char *url) {
  struct dynstr d[1];
  char **keys = hash_keys(cgi_args);
  int n;

  dynstr_init(d);
  dynstr_append_string(d, url);
  for(n = 0; keys[n]; ++n) {
    dynstr_append(d, n ? '&' : '?');
    dynstr_append_string(d, urlencodestring(keys[n]));
    dynstr_append(d, '=');
    dynstr_append_string(d, urlencodestring(cgi_get(keys[n])));
  }
  dynstr_terminate(d);
  return d->vec;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
