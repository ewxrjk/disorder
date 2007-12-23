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
#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "disorder.h"
#include "api-client.h"
#include "mime.h"
#include "printf.h"
#include "dcgi.h"

/** @brief Infer the base URL for the web interface if it's not set
 *
 * See <a href="http://tools.ietf.org/html/rfc3875">RFC 3875</a>.
 */
static void infer_url(void) {
  if(!config->url) {
    const char *scheme = "http", *server, *script, *e;
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

    /* Figure out path to ourselves */
    if(!(script = getenv("SCRIPT_NAME")))
      fatal(0, "SCRIPT_NAME is not set");
    if(script[0] != '/')
      fatal(0, "SCRIPT_NAME does not start with a '/'");
    script = xstrdup(script);

    if(port == 80)
      byte_xasprintf(&config->url, "%s://%s%s",
		     scheme, server, script);
    else
      byte_xasprintf(&config->url, "%s://%s:%d%s",
		     scheme, server, port, script);
  }
}

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
  infer_url();
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
  disorder_cgi_login(&s, &output);
  /* TODO RFC 3875 s8.2 recommendations e.g. concerning PATH_INFO */
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
