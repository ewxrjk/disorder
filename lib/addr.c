/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007 Richard Kettlewell
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
/** @file lib/addr.c
 * @brief Socket address support */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "log.h"
#include "printf.h"
#include "configuration.h"
#include "addr.h"
#include "mem.h"

/** @brief Convert a pair of strings to an address
 * @param a Pointer to string list
 * @param pref Hints structure for getaddrinfo, or NULL
 * @param namep Where to store address description, or NULL
 * @return Address info structure or NULL on error
 *
 * This converts one or two strings into an address specification suitable
 * for passing to socket(), bind() etc.
 *
 * If there is only one string then it is assumed to be the service
 * name (port number).  If there are two then the first is the host
 * name and the second the service name.
 *
 * @p namep is used to return a description of the address suitable
 * for use in log messages.
 *
 * If an error occurs a message is logged and a null pointer returned.
 */
struct addrinfo *get_address(const struct stringlist *a,
			     const struct addrinfo *pref,
			     char **namep) {
  struct addrinfo *res;
  char *name;
  int rc;

  switch(a->n) {  
  case 1:
    byte_xasprintf(&name, "host * service %s", a->s[0]);
    if((rc = getaddrinfo(0, a->s[0], pref, &res))) {
      error(0, "getaddrinfo %s: %s", a->s[0], gai_strerror(rc));
      return 0;
    }
    break;
  case 2:
    byte_xasprintf(&name, "host %s service %s", a->s[0], a->s[1]);
    if((rc = getaddrinfo(a->s[0], a->s[1], pref, &res))) {
      error(0, "getaddrinfo %s %s: %s", a->s[0], a->s[1], gai_strerror(rc));
      return 0;
    }
    break;
  default:
    error(0, "invalid network address specification (n=%d)", a->n);
    return 0;
  }
  if(!res || (pref && res->ai_socktype != pref->ai_socktype)) {
    error(0, "getaddrinfo didn't give us a suitable socket address");
    if(res)
      freeaddrinfo(res);
    return 0;
  }
  if(namep)
    *namep = name;
  return res;
}

/** @brief Comparison function for address information
 *
 * Suitable for qsort().
 */
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

static inline int multicast4(const struct sockaddr_in *sin4) {
  return IN_MULTICAST(ntohl(sin4->sin_addr.s_addr));
}

static inline int multicast6(const struct sockaddr_in6 *sin6) {
  return IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr);
}

/** @brief Return true if @p sa represents a multicast address */
int multicast(const struct sockaddr *sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return multicast4((const struct sockaddr_in *)sa);
  case AF_INET6:
    return multicast6((const struct sockaddr_in6 *)sa);
  default:
    return 0;
  }
}

static inline char *format_sockaddr4(const struct sockaddr_in *sin4) {
  char buffer[1024], *r;

  if(sin4->sin_port)
    byte_xasprintf(&r, "%s port %u",
		   inet_ntop(sin4->sin_family, &sin4->sin_addr,
			     buffer, sizeof buffer),
		   ntohs(sin4->sin_port));
  else
    byte_xasprintf(&r, "%s",
		   inet_ntop(sin4->sin_family, &sin4->sin_addr,
			     buffer, sizeof buffer));
  return r;
}

static inline char *format_sockaddr6(const struct sockaddr_in6 *sin6) {
  char buffer[1024], *r;

  if(sin6->sin6_port)
    byte_xasprintf(&r, "%s port %u",
		   inet_ntop(sin6->sin6_family, &sin6->sin6_addr,
			     buffer, sizeof buffer),
		   ntohs(sin6->sin6_port));
  else
    byte_xasprintf(&r, "%s",
		   inet_ntop(sin6->sin6_family, &sin6->sin6_addr,
			     buffer, sizeof buffer));
  return r;
}

static inline char *format_sockaddrun(const struct sockaddr_un *sun) {
  return xstrdup(sun->sun_path);
}
    
/** @brief Construct a text description a sockaddr */
char *format_sockaddr(const struct sockaddr *sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return format_sockaddr4((const struct sockaddr_in *)sa);
  case AF_INET6:
    return format_sockaddr6((const struct sockaddr_in6 *)sa);
  case AF_UNIX:
    return format_sockaddrun((const struct sockaddr_un *)sa);
  default:
    return 0;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
