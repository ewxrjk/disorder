/*
 * This file is part of DisOrder
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

#include "common.h"

#include <sys/utsname.h>
#include <errno.h>
#include <netdb.h>

#include "mem.h"
#include "hostname.h"
#include "log.h"

static const char *hostname;

/** @brief Return the local hostname
 * @return Hostname
 */
const char *local_hostname(void) {
  if(!hostname) {
    struct utsname u;
    struct hostent *he;

    if(uname(&u) < 0)
      fatal(errno, "error calling uname");
    if(!(he = gethostbyname(u.nodename)))
      fatal(0, "cannot resolve '%s'", u.nodename);
    hostname = xstrdup(he->h_name);
  }
  return hostname;
}


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
