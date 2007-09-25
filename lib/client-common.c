/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "log.h"
#include "configuration.h"
#include "client-common.h"
#include "addr.h"

/** @brief Invoke a function with the connect address
 * @param c Passed to callback
 * @param function Function to call
 *
 * Calls @p function with the result of looking up the connect address.
 */
int with_sockaddr(void *c,
		  int (*function)(void *c,
				  const struct sockaddr *sa,
				  socklen_t len,
				  const char *ident)) {
  const char *path;
  struct sockaddr_un su;
  struct addrinfo *res;
  char *name;
  int n;
   
  static const struct addrinfo pref = {
    0,
    PF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    0,
    0,
    0,
    0
  };

  if(config->connect.n) {
    res = get_address(&config->connect, &pref, &name);
    if(!res) return -1;
    n = function(c, res->ai_addr, res->ai_addrlen, name);
    freeaddrinfo(res);
    return n;
  } else {
    path = config_get_file("socket");
    if(strlen(path) >= sizeof su.sun_path) {
      error(errno, "socket path is too long");
      return -1;
    }
    memset(&su, 0, sizeof su);
    su.sun_family = AF_UNIX;
    strcpy(su.sun_path, path);
    return function(c, (struct sockaddr *)&su, sizeof su, path);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
