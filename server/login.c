/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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

#include "disorder-cgi.h"

/** @brief Client used by CGI
 *
 * The caller should arrange for this to be created before any of
 * these expansions are used (if it cannot connect then it's safe to
 * leave it as NULL).
 */
disorder_client *dcgi_client;

/** @brief Return true if @p a is better than @p b
 *
 * NB. We don't bother checking if the path is right, we merely check for the
 * longest path.  This isn't a security hole: if the browser wants to send us
 * bad cookies it's quite capable of sending just the right path anyway.  The
 * point of choosing the longest path is to avoid using a cookie set by another
 * CGI script which shares a path prefix with us, which would allow it to
 * maliciously log users out.
 *
 * Such a script could still "maliciously" log someone in, if it had acquired a
 * suitable cookie.  But it could just log in directly if it had that, so there
 * is no obvious vulnerability here either.
 */
static int better_cookie(const struct cookie *a, const struct cookie *b) {
  if(a->path && b->path)
    /* If both have a path then the one with the longest path is best */
    return strlen(a->path) > strlen(b->path);
  else if(a->path)
    /* If only @p a has a path then it is better */
    return 1;
  else
    /* If neither have a path, or if only @p b has a path, then @p b is
     * better */
    return 0;
}

/** @brief Login cookie */
char *dcgi_cookie;

/** @brief Set @ref login_cookie */
void dcgi_get_cookie(void) {
  const char *cookie_env;
  int n, best_cookie;
  struct cookiedata cd;

  /* See if there's a cookie */
  cookie_env = getenv("HTTP_COOKIE");
  if(cookie_env) {
    /* This will be an HTTP header */
    if(!parse_cookie(cookie_env, &cd)) {
      /* Pick the best available cookie from all those offered */
      best_cookie = -1;
      for(n = 0; n < cd.ncookies; ++n) {
	/* Is this the right cookie? */
	if(strcmp(cd.cookies[n].name, "disorder"))
	  continue;
	/* Is it better than anything we've seen so far? */
	if(best_cookie < 0
	   || better_cookie(&cd.cookies[n], &cd.cookies[best_cookie]))
	  best_cookie = n;
      }
      if(best_cookie != -1)
	dcgi_cookie = cd.cookies[best_cookie].value;
    } else
      error(0, "could not parse cookie field '%s'", cookie_env);
  }
}

/** @brief Return a Cookie: header */
char *dcgi_cookie_header(void) {
  struct dynstr d[1];
  struct url u;
  char *s;

  memset(&u, 0, sizeof u);
  dynstr_init(d);
  parse_url(config->url, &u);
  if(dcgi_cookie) {
    dynstr_append_string(d, "disorder=");
    dynstr_append_string(d, dcgi_cookie);
  } else {
    /* Force browser to discard cookie */
    dynstr_append_string(d, "disorder=none;Max-Age=0");
  }
  if(u.path) {
    /* The default domain matches the request host, so we need not override
     * that.  But the default path only goes up to the rightmost /, which would
     * cause the browser to expose the cookie to other CGI programs on the same
     * web server. */
    dynstr_append_string(d, ";Version=1;Path=");
    /* Formally we are supposed to quote the path, since it invariably has a
     * slash in it.  However Safari does not parse quoted paths correctly, so
     * this won't work.  Fortunately nothing else seems to care about proper
     * quoting of paths, so in practice we get with it.  (See also
     * parse_cookie() where we are liberal about cookie paths on the way back
     * in.) */
    dynstr_append_string(d, u.path);
  }
  dynstr_terminate(d);
  byte_xasprintf(&s, "Set-Cookie: %s", d->vec);
  return s;
}

/** @brief Log in as the current user or guest if none */
void dcgi_login(void) {
  /* Junk old data */
  dcgi_lookup_reset();
  /* Junk the old connection if there is one */
  if(dcgi_client)
    disorder_close(dcgi_client);
  /* Create a new connection */
  dcgi_client = disorder_new(0);
  /* Reconnect */
  if(disorder_connect_cookie(dcgi_client, dcgi_cookie)) {
    dcgi_error("Cannot connect to server");
    exit(0);
  }
  /* If there was a cookie but it went bad, we forget it */
  if(dcgi_cookie && !strcmp(disorder_user(dcgi_client), "guest"))
    dcgi_cookie = 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
