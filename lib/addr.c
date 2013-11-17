/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008, 2013 Richard Kettlewell
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
/** @file lib/addr.c
 * @brief Socket address support */

#include "common.h"

#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if HAVE_SYS_UN_H
# include <sys/un.h>
#endif

#include "log.h"
#include "printf.h"
#include "addr.h"
#include "mem.h"
#include "syscalls.h"
#include "configuration.h"
#include "vector.h"

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
  char errbuf[1024];

  switch(a->n) {  
  case 1:
    byte_xasprintf(&name, "host * service %s", a->s[0]);
    if((rc = getaddrinfo(0, a->s[0], pref, &res))) {
      disorder_error(0, "getaddrinfo %s: %s", a->s[0],
                     format_error(ec_getaddrinfo, rc, errbuf, sizeof errbuf));
      return 0;
    }
    break;
  case 2:
    byte_xasprintf(&name, "host %s service %s", a->s[0], a->s[1]);
    if((rc = getaddrinfo(a->s[0], a->s[1], pref, &res))) {
      disorder_error(0, "getaddrinfo %s %s: %s",
                     a->s[0], a->s[1],
                     format_error(ec_getaddrinfo, rc, errbuf, sizeof errbuf));
      return 0;
    }
    break;
  default:
    disorder_error(0, "invalid network address specification (n=%d)", a->n);
    return 0;
  }
  if(!res || (pref && res->ai_socktype != pref->ai_socktype)) {
    disorder_error(0, "getaddrinfo didn't give us a suitable socket address");
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
  if(a->ai_family != b->ai_family) return a->ai_family - b->ai_family;
  if(a->ai_socktype != b->ai_socktype) return a->ai_socktype - b->ai_socktype;
  if(a->ai_protocol != b->ai_protocol) return a->ai_protocol - b->ai_protocol;
  return sockaddrcmp(a->ai_addr, b->ai_addr);
}

/** @brief Comparison function for socket addresses
 *
 * Suitable for qsort().
 */
int sockaddrcmp(const struct sockaddr *a,
		const struct sockaddr *b) {
  const struct sockaddr_in *ina, *inb;
  const struct sockaddr_in6 *in6a, *in6b;
  
  if(a->sa_family != b->sa_family) return a->sa_family - b->sa_family;
  switch(a->sa_family) {
  case PF_INET:
    ina = (const struct sockaddr_in *)a;
    inb = (const struct sockaddr_in *)b;
    if(ina->sin_port != inb->sin_port) return ina->sin_port - inb->sin_port;
    return ina->sin_addr.s_addr - inb->sin_addr.s_addr;
    break;
  case PF_INET6:
    in6a = (const struct sockaddr_in6 *)a;
    in6b = (const struct sockaddr_in6 *)b;
    if(in6a->sin6_port != in6b->sin6_port)
      return in6a->sin6_port - in6b->sin6_port;
    return memcmp(&in6a->sin6_addr, &in6b->sin6_addr,
		  sizeof (struct in6_addr));
  default:
    disorder_fatal(0, "unsupported protocol family %d", a->sa_family);
  }
}

/** @brief Return nonzero if @p sin4 is an IPv4 multicast address */
static inline int multicast4(const struct sockaddr_in *sin4) {
  return IN_MULTICAST(ntohl(sin4->sin_addr.s_addr));
}

/** @brief Return nonzero if @p sin6 is an IPv6 multicast address */
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

/** @brief Format an IPv4 address */
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

/** @brief Format an IPv6 address */
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

#if HAVE_SYS_UN_H
/** @brief Format a UNIX socket address */
static inline char *format_sockaddrun(const struct sockaddr_un *sun) {
  return xstrdup(sun->sun_path);
}
#endif
    
/** @brief Construct a text description a sockaddr
 * @param sa Socket address
 * @return Human-readable form of address
 */
char *format_sockaddr(const struct sockaddr *sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return format_sockaddr4((const struct sockaddr_in *)sa);
  case AF_INET6:
    return format_sockaddr6((const struct sockaddr_in6 *)sa);
#if HAVE_SYS_UN_H
  case AF_UNIX:
    return format_sockaddrun((const struct sockaddr_un *)sa);
#endif
  default:
    return 0;
  }
}

/** @brief Parse the text form of a network address
 * @param na Where to store result
 * @param nvec Number of strings
 * @param vec List of strings
 * @return 0 on success, -1 on syntax error
 */
int netaddress_parse(struct netaddress *na,
		     int nvec,
		     char **vec) {
  const char *port;

  na->af = AF_UNSPEC;
  if(nvec > 0 && vec[0][0] == '-') {
    if(!strcmp(vec[0], "-4"))
      na->af = AF_INET;
    else if(!strcmp(vec[0], "-6")) 
      na->af = AF_INET6;
    else if(!strcmp(vec[0], "-unix"))
      na->af = AF_UNIX;
    else if(!strcmp(vec[0], "-"))
      na->af = AF_UNSPEC;
    else
      return -1;
    --nvec;
    ++vec;
  }
  if(nvec == 0)
    return -1;
  /* Possibilities are:
   *
   *       /path/to/unix/socket      an AF_UNIX socket
   *       * PORT                    any address, specific port
   *       PORT                      any address, specific port
   *       ADDRESS PORT              specific address, specific port
   */
  if(vec[0][0] == '/' && na->af == AF_UNSPEC)
    na->af = AF_UNIX;
  if(na->af == AF_UNIX) {
    if(nvec != 1)
      return -1;
    na->address = xstrdup(vec[0]);
    na->port = -1;			/* makes no sense */
  } else {
    switch(nvec) {
    case 1:
      na->address = NULL;
      port = vec[0];
      break;
    case 2:
      if(!strcmp(vec[0], "*"))
	na->address = NULL;
      else
	na->address = xstrdup(vec[0]);
      port = vec[1];
      break;
    default:
      return -1;
    }
    if(port[strspn(port, "0123456789")])
      return -1;
    long p;
    int e = xstrtol(&p, port, NULL, 10);

    if(e)
      return -1;
    if(p > 65535)
      return -1;
    na->port = (int)p;
  }
  return 0;
}

/** @brief Format a @ref netaddress structure
 * @param na Network address to format
 * @param nvecp Where to put string count, or NULL
 * @param vecp Where to put strings
 *
 * The formatted form is suitable for passing to netaddress_parse().
 */
void netaddress_format(const struct netaddress *na,
		       int *nvecp,
		       char ***vecp) {
  struct vector v[1];

  vector_init(v);
  switch(na->af) {
  case AF_UNSPEC: vector_append(v, xstrdup("-")); break;
  case AF_INET: vector_append(v, xstrdup("-4")); break;
  case AF_INET6: vector_append(v, xstrdup("-6")); break;
  case AF_UNIX: vector_append(v, xstrdup("-unix")); break;
  }
  if(na->address)
    vector_append(v, xstrdup(na->address));
  else
    vector_append(v, xstrdup("*"));
  if(na->port != -1) {
    char buffer[64];

    snprintf(buffer, sizeof buffer, "%d", na->port);
    vector_append(v, xstrdup(buffer));
  }
  vector_terminate(v);
  if(nvecp)
    *nvecp = v->nvec;
  if(vecp)
    *vecp = v->vec;
}

/** @brief Resolve a network address
 * @param na Address structure
 * @param passive True if passive (bindable) address is desired
 * @param protocol Protocol number desired (e.g. @c IPPROTO_TCP)
 * @return List of suitable addresses or NULL
 */
struct addrinfo *netaddress_resolve(const struct netaddress *na,
				    int passive,
				    int protocol) {
  struct addrinfo *res, hints[1];
  char service[64];
  int rc;
  char errbuf[1024];

  memset(hints, 0, sizeof hints);
  hints->ai_family = na->af;
  hints->ai_protocol = protocol;
  hints->ai_flags = passive ? AI_PASSIVE : 0;
  snprintf(service, sizeof service, "%d", na->port);
  rc = getaddrinfo(na->address, service, hints, &res);
  if(rc) {
    disorder_error(0, "getaddrinfo %s %d: %s",
		   na->address ? na->address : "*",
                   na->port,
                   format_error(ec_getaddrinfo, rc, errbuf, sizeof errbuf));
    return NULL;
  }
  return res;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
