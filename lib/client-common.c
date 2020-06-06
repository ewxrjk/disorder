/*
 * This file is part of DisOrder
 * Copyright (C) 2004-7, 2009, 2011-13 Richard Kettlewell
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
/** @file lib/client-common.c
 * @brief Common code to client APIs
 */

#include "common.h"

#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#include <errno.h>
#if HAVE_NETDB_H
# include <netdb.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_WS2TCPIP_H
# include <Ws2tcpip.h>
#endif

#include "log.h"
#include "configuration.h"
#include "client-common.h"
#include "addr.h"
#include "mem.h"

/** @brief Figure out what address to connect to
 * @param c Configuration to honor
 * @param flags Flags to guide the choice
 * @param sap Where to store pointer to sockaddr
 * @param namep Where to store socket name
 * @return Socket length, or (socklen_t)-1
 */
socklen_t disorder_find_server(struct config *c, unsigned flags,
                               struct sockaddr **sap, char **namep) {
  struct sockaddr *sa;
#if !_WIN32
  struct sockaddr_un su;
#endif
  struct resolved *res;
  size_t nres;
  char *name = NULL;
  socklen_t len;

  if(c->connect.af != -1) {
    if(netaddress_resolve(&c->connect, 0, SOCK_STREAM, &res, &nres))
      return -1;
    sa = res->sa;
    len = res->len;
  } else {
#if _WIN32
    disorder_fatal(0, "local connections are not supported on Windows");
#else
    /* use the private socket if possible (which it should be) */
    if (!(flags & DISORDER_FS_NOTPRIV)) {
      name = config_get_file2(c, "private/socket");
      if(access(name, R_OK) != 0) {
        xfree(name);
        name = NULL;
      }
    }
    if(!name)
      name = config_get_file2(c, "socket");
    if(strlen(name) >= sizeof su.sun_path) {
      disorder_error(0, "socket path is too long");
      return -1;
    }
    memset(&su, 0, sizeof su);
    su.sun_family = AF_UNIX;
    strcpy(su.sun_path, name);
    sa = (struct sockaddr *)&su;
    len = sizeof su;
    res = 0;
    xfree(name);
#endif
  }
  *sap = xmalloc_noptr(len);
  memcpy(*sap, sa, len);
  if(namep)
    *namep = format_sockaddr(sa);
  if(res)
    netaddress_free_resolved(res, nres);
  return len;
}

/** @brief Figure out what address to connect to
 * @param c Configuration to honor
 * @param sap Where to store pointer to sockaddr
 * @param namep Where to store socket name
 * @return Socket length, or (socklen_t)-1
 *
 * The function disorder_find_server() isn't a namespace violation, and has
 * more functionality.  This function is equivalent, to disorder_find_server()
 * with a zero @c flags argument.
 */
socklen_t find_server(struct config *c,
                      struct sockaddr **sap, char **namep) {
  return disorder_find_server(c, 0, sap, namep);
}

const char disorder__body[1];
const char disorder__list[1];
const char disorder__integer[1];
const char disorder__time[1];

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
