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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <locale.h>
#include <string.h>
#include <stdarg.h>

#include "client.h"
#include "sink.h"
#include "cgi.h"
#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "disorder.h"
#include "api-client.h"
#include "mime.h"
#include "printf.h"
#include "dcgi.h"
#include "url.h"

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

int main(int argc, char **argv) {
  const char *cookie_env, *conf;
  dcgi_global g;
  dcgi_state s;
  cgi_sink output;
  int n, best_cookie;
  struct cookiedata cd;

  if(argc > 0) progname = argv[0];
  /* RFC 3875 s8.2 recommends rejecting PATH_INFO if we don't make use of
   * it. */
  if(getenv("PATH_INFO")) {
    printf("Content-Type: text/html\n");
    printf("Status: 404\n");
    printf("\n");
    printf("<p>Sorry, PATH_INFO not supported.</p>\n");
    exit(0);
  }
  cgi_parse();
  if((conf = getenv("DISORDER_CONFIG"))) configfile = xstrdup(conf);
  if(getenv("DISORDER_DEBUG")) debugging = 1;
  if(config_read(0)) exit(EXIT_FAILURE);
  if(!config->url)
    config->url = infer_url();
  memset(&g, 0, sizeof g);
  memset(&s, 0, sizeof s);
  s.g = &g;
  g.client = disorder_get_client();
  output.quote = 1;
  output.sink = sink_stdio("stdout", stdout);
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
	login_cookie = cd.cookies[best_cookie].value;
    }
  }
  disorder_cgi_login(&s, &output);
  disorder_cgi(&output, &s);
  if(fclose(stdout) < 0) fatal(errno, "error closing stdout");
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
