/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

#include "common.h"

#include <netinet/in.h>
#include <sys/un.h>
#include <errno.h>
#include <netdb.h>

#include "log.h"
#include "configuration.h"
#include "client-common.h"
#include "addr.h"
#include "mem.h"

/** @brief Figure out what address to connect to
 * @param c Configuration to honor
 * @param sap Where to store pointer to sockaddr
 * @param namep Where to store socket name
 * @return Socket length, or (socklen_t)-1
 */
socklen_t find_server(struct config *c,
                      struct sockaddr **sap, char **namep) {
  struct sockaddr *sa;
  struct sockaddr_un su;
  struct addrinfo *res = 0;
  char *name;
  socklen_t len;
   
  static const struct addrinfo pref = {
    .ai_flags = 0,
    .ai_family = PF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
  };

  if(c->connect.n) {
    res = get_address(&c->connect, &pref, &name);
    if(!res) return -1;
    sa = res->ai_addr;
    len = res->ai_addrlen;
  } else {
    name = config_get_file2(c, "socket");
    if(strlen(name) >= sizeof su.sun_path) {
      error(errno, "socket path is too long");
      return -1;
    }
    memset(&su, 0, sizeof su);
    su.sun_family = AF_UNIX;
    strcpy(su.sun_path, name);
    sa = (struct sockaddr *)&su;
    len = sizeof su;
  }
  *sap = xmalloc_noptr(len);
  memcpy(*sap, sa, len);
  if(namep)
    *namep = name;
  if(res)
    freeaddrinfo(res);
  return len;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
