/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/url.c
 * @brief URL support functions
 */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mem.h"
#include "log.h"
#include "printf.h"
#include "url.h"
#include "kvp.h"

/** @brief Infer the for the web interface
 * @return Inferred URL
 *
 * See <a href="http://tools.ietf.org/html/rfc3875">RFC 3875</a>.
 */
char *infer_url(void) {
  const char *scheme = "http", *server, *script, *e, *request_uri;
  char *url;
  int port;
  
  /* Figure out the server.  'MUST' be set and we don't cope if it
   * is not. */
  if(!(server = getenv("SERVER_NAME")))
    fatal(0, "SERVER_NAME is not set");
  server = xstrdup(server);
  
  /* Figure out the port.  'MUST' be set but we cope if it is not. */
  if((e = getenv("SERVER_PORT")))
    port = atoi(e);
  else
    port = 80;
  
  /* Figure out path to ourselves. */
  if((request_uri = getenv("REQUEST_URI"))) {
    /* REQUEST_URI is an Apache extexnsion.  If it's available it results in
     * more accurate self-referencing URLs.  */
    if((e = strchr(request_uri, '?')))
      script = xstrndup(request_uri, e - request_uri);
    else
      script = xstrdup(request_uri);
  } else {
    /* RFC3875 s4.1.13 */
    if(!(script = getenv("SCRIPT_NAME")))
      fatal(0, "SCRIPT_NAME is not set");
    /* SCRIPT_NAME may be "" */
    if(!*script)
      script = "/";
    /* SCRIPT_NAME is not URL-encoded */
    script = urlencodestring(script);
  }
  if(script[0] != '/')
    fatal(0, "SCRIPT_NAME does not start with a '/'");
  script = xstrdup(script);
  
  if(port == 80)
    byte_xasprintf(&url, "%s://%s%s",
		   scheme, server, script);
  else
    byte_xasprintf(&url, "%s://%s:%d%s",
		   scheme, server, port, script);
  return url;
}

/** @brief Parse a URL
 * @param url URL to parsed
 * @param parsed Where to store parsed URL data
 * @return 0 on success, non-0 on error
 *
 * NB that URLs with usernames and passwords are NOT currently supported.
 */
int parse_url(const char *url, struct url *parsed) {
  const char *s;
  long n;

  /* The scheme */
  for(s = url; *s && *s != '/' && *s != ':'; ++s)
    ;
  if(*s == ':') {
    parsed->scheme = xstrndup(url, s - url);
    url = s + 1;
  } else
    parsed->scheme = 0;

  /* The host and port */
  if(*url == '/' && url[1] == '/') {
    /* //user:password@host:port, but we don't support the
     * user:password@ part. */
    url += 2;
    for(s = url; *s && *s != '/' && *s != ':'; ++s)
      ;
    parsed->host = xstrndup(url, s - url);
    if(*s == ':') {
      /* We have host:port[/...] */
      ++s;
      errno = 0;
      n = strtol(s, (char **)&url, 10);
      if(errno)
	return -1;
      if(n < 0 || n > 65535)
	return -1;
      parsed->port = n;
    } else {
      /* We just have host[/...] */
      url = s;
      parsed->port = -1;
    }
  }

  /* The path */
  for(s = url; *s && *s != '?'; ++s)
    ;
  if(!(parsed->path = urldecodestring(url, s - url)))
    return -1;
  url = s;

  /* The query */
  if(*url == '?')
    parsed->query = xstrdup(url + 1);
  else
    parsed->query = 0;

  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
