/*
 * This file is part of DisOrder.
 * Copyright (C) 2004 Richard Kettlewell
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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "log.h"
#include "printf.h"
#include "configuration.h"
#include "addr.h"

struct addrinfo *get_address(const struct stringlist *a,
			     const struct addrinfo *pref,
			     char **namep) {
  struct addrinfo *res;
  char *name;
  int rc;
  
  if(a->n == 1) {
    byte_xasprintf(&name, "host * service %s", a->s[0]);
    if((rc = getaddrinfo(0, a->s[0], pref, &res))) {
      error(0, "getaddrinfo %s: %s", a->s[0], gai_strerror(rc));
      return 0;
    }
  } else {
    byte_xasprintf(&name, "host %s service %s", a->s[0], a->s[1]);
    if((rc = getaddrinfo(a->s[0], a->s[1], pref, &res))) {
      error(0, "getaddrinfo %s %s: %s", a->s[0], a->s[1], gai_strerror(rc));
      return 0;
    }
  }
  if(!res || res->ai_socktype != SOCK_STREAM) {
    error(0, "getaddrinfo didn't give us a stream socket");
    if(res)
      freeaddrinfo(res);
    return 0;
  }
  if(namep)
    *namep = name;
  return res;
}

int addrinfocmp(const struct addrinfo *a,
		const struct addrinfo *b) {
  const struct sockaddr_in *ina, *inb;
  const struct sockaddr_in6 *in6a, *in6b;
  
  if(a->ai_family != b->ai_family) return a->ai_family - b->ai_family;
  if(a->ai_socktype != b->ai_socktype) return a->ai_socktype - b->ai_socktype;
  if(a->ai_protocol != b->ai_protocol) return a->ai_protocol - b->ai_protocol;
  switch(a->ai_protocol) {
  case PF_INET:
    ina = (const struct sockaddr_in *)a->ai_addr;
    inb = (const struct sockaddr_in *)b->ai_addr;
    if(ina->sin_port != inb->sin_port) return ina->sin_port - inb->sin_port;
    return ina->sin_addr.s_addr - inb->sin_addr.s_addr;
    break;
  case PF_INET6:
    in6a = (const struct sockaddr_in6 *)a->ai_addr;
    in6b = (const struct sockaddr_in6 *)b->ai_addr;
    if(in6a->sin6_port != in6b->sin6_port)
      return in6a->sin6_port - in6b->sin6_port;
    return memcmp(&in6a->sin6_addr, &in6b->sin6_addr,
		  sizeof (struct in6_addr));
  default:
    error(0, "unsupported protocol family %d", a->ai_protocol);
    return memcmp(a->ai_addr, b->ai_addr, a->ai_addrlen); /* kludge */
  }
}
  
/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:e143b5b1b677e108d957da2f6d09bccd */
