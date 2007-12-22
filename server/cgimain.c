/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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
#include "dcgi.h"
#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "disorder.h"
#include "api-client.h"
#include "mime.h"

int main(int argc, char **argv) {
  const char *cookie_env, *conf;
  dcgi_global g;
  dcgi_state s;
  cgi_sink output;
  int n;
  struct cookiedata cd;

  if(argc > 0) progname = argv[0];
  cgi_parse();
  if((conf = getenv("DISORDER_CONFIG"))) configfile = xstrdup(conf);
  if(getenv("DISORDER_DEBUG")) debugging = 1;
  if(config_read(0)) exit(EXIT_FAILURE);
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
      for(n = 0; n < cd.ncookies
	    && strcmp(cd.cookies[n].name, "disorder"); ++n)
	;
      if(n < cd.ncookies)
	login_cookie = cd.cookies[n].value;
    }
  }
  /* Log in with the cookie if possible otherwise as guest */
  if(disorder_connect_cookie(g.client, login_cookie)) {
    disorder_cgi_error(&output, &s, "connect");
    return 0;
  }
  disorder_cgi(&output, &s);
  if(fclose(stdout) < 0) fatal(errno, "error closing stdout");
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
