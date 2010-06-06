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
/** @file cgi/cgimain.c
 * @brief DisOrder CGI
 */

#include "disorder-cgi.h"

int main(int argc, char **argv) {
  const char *conf;

  if(argc > 0)
    progname = argv[0];
  /* RFC 3875 s8.2 recommends rejecting PATH_INFO if we don't make use of
   * it. */
  /* TODO we could make disorder/ACTION equivalent to disorder?action=ACTION */
  if(getenv("PATH_INFO")) {
    /* TODO it might be nice to link back to the right place... */
    printf("Content-Type: text/html; charset=UTF-8\n");
    printf("Status: 404\n");
    printf("\n");
    printf("<p>Sorry, is PATH_INFO not supported."
           "<a href=\"%s\">Try here instead.</a></p>\n",
           cgi_sgmlquote(infer_url(0/*!include_path_info*/)));
    exit(0);
  }
  /* Parse CGI arguments */
  cgi_init();
  /* We allow various things to be overridden from the environment.  This is
   * intended for debugging and is not a documented feature. */
  if((conf = getenv("DISORDER_CONFIG")))
    configfile = xstrdup(conf);
  if(getenv("DISORDER_DEBUG"))
    debugging = 1;
  /* Read configuration */
  if(config_read(0/*!server*/))
    exit(EXIT_FAILURE);
  /* Figure out our URL.  This can still be overridden from the config file if
   * necessary but it shouldn't be necessary in ordinary installations. */
  if(!config->url)
    config->url = infer_url(1/*include_path_info*/);
  /* Pick up the cookie, if there is one */
  dcgi_get_cookie();
  /* Register expansions */
  mx_register_builtin();
  dcgi_expansions();
  /* Update search path.  We look in the config directory first and the data
   * directory second, so that the latter overrides the former. */
  mx_search_path(pkgconfdir);
  mx_search_path(pkgdatadir);
  /* Never cache anythging */
  if(printf("Cache-Control: no-cache\n") < 0)
    fatal(errno, "error writing to stdout");
  /* Create the initial connection, trying the cookie if we found a suitable
   * one. */
  dcgi_login();
  /* Do whatever the user wanted */
  dcgi_action(NULL);
  /* In practice if a write fails that probably means the web server went away,
   * but we log it anyway. */
  if(fclose(stdout) < 0)
    fatal(errno, "error closing stdout");
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
