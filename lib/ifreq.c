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
/** @file lib/ifreq.c
 * @brief Network interface support
 */

#include <config.h>
#include "types.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "ifreq.h"
#include "mem.h"
#include "log.h"

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
