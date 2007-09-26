/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/ffreq.c
 * @brief Network interface support
 */

#include <config.h>
#include "types.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "ifreq.h"
#include "mem.h"
#include "log.h"

/** @brief Get the list of network interfaces
 * @param fd File descriptor to use
 * @param interfaces Where to put pointer to array of interfaces
 * @param ninterfaces Where to put count of interfaces
 */
void ifreq_list(int fd, struct ifreq **interfaces, int *ninterfaces) {
  struct ifconf ifc;
  int l;

  ifc.ifc_len = 0;
  ifc.ifc_req = 0;
  do {
    l = ifc.ifc_len = ifc.ifc_len ? 2 * ifc.ifc_len
                                  : 16 * (int)sizeof (struct ifreq);
    if(!l) fatal(0, "out of memory");
    ifc.ifc_req = xrealloc(ifc.ifc_req, l);
    if(ioctl(fd, SIOCGIFCONF, &ifc) < 0)
      fatal(errno, "error calling ioctl SIOCGIFCONF");
  } while(l == ifc.ifc_len);
  *ninterfaces = ifc.ifc_len / sizeof (struct ifreq);
  *interfaces = ifc.ifc_req;
}

/** @brief Return true if two socket addresses match */
int sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b) {
  if(a->sa_family != b->sa_family)
    return 0;
  switch(a->sa_family) {
  case AF_INET:
    return ((const struct sockaddr_in *)a)->sin_addr.s_addr
           == ((const struct sockaddr_in *)b)->sin_addr.s_addr;
  case AF_INET6:
    return !memcmp(&((const struct sockaddr_in6 *)a)->sin6_addr,
		   &((const struct sockaddr_in6 *)b)->sin6_addr,
		   sizeof (struct in6_addr));
  default:
    fatal(0, "unknown address family %d", a->sa_family);
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
