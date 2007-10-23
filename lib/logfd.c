/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "logfd.h"
#include "event.h"
#include "log.h"

struct logfd_state {
  const char *tag;
};

/* called when bytes are available and at eof */
static int logfd_readable(ev_source attribute((unused)) *ev,
			  ev_reader *reader,
			  int fd,
			  void *ptr,
			  size_t bytes,
			  int eof,
			  void *u) {
  char *nl;
  const char *tag = u;
  int len;

  while((nl = memchr(ptr, '\n', bytes))) {
    len = nl - (char *)ptr;
    ev_reader_consume(reader, len + 1);
    info("%s: %.*s", tag, len, (char *)ptr);
    ptr = nl + 1;
    bytes -= len + 1;
  }
  if(eof && bytes) {
    info("%s: %.*s", tag, (int)bytes, (char *)ptr);
    ev_reader_consume(reader, bytes);
  }
  if(eof)
    xclose(fd);
  return 0;
}

/* called when a read error occurs */
static int logfd_error(ev_source attribute((unused)) *ev,
		       int fd,
		       int errno_value,
		       void *u) {
  const char *tag = u;
  
  error(errno_value, "error reading log pipe from %s", tag);
  xclose(fd);
  return 0;
}

int logfd(ev_source *ev, const char *tag) {
  int p[2];

  xpipe(p);
  cloexec(p[0]);
  nonblock(p[0]);
  if(!ev_reader_new(ev, p[0], logfd_readable, logfd_error, (void *)tag,
                    "logfd"))
    fatal(errno, "error calling ev_reader_new");
  return p[1];
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
