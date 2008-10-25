/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/logfd.c
 * @brief Redirect subprocess stderr to DisOrder server's log
 */

#include "common.h"

#include <unistd.h>
#include <errno.h>

#include "syscalls.h"
#include "logfd.h"
#include "event.h"
#include "log.h"

/* called when bytes are available and at eof */
static int logfd_readable(ev_source attribute((unused)) *ev,
			  ev_reader *reader,
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
  return 0;
}

/* called when a read error occurs */
static int logfd_error(ev_source attribute((unused)) *ev,
		       int errno_value,
		       void *u) {
  const char *tag = u;
  
  error(errno_value, "error reading log pipe from %s", tag);
  return 0;
}

/** @brief Create file descriptor for a subprocess to log to
 * @param ev Event loop
 * @param tag Tag for this log
 * @return File descriptor
 *
 * Returns a file descriptor which a subprocess can log to.  The normal thing
 * to do would be to dup2() this fd onto the subprocess's stderr (and to close
 * it in the parent).
 *
 * Any lines written to this fd (i.e. by the subprocess) will be logged via
 * info(), with @p tag included.
 */
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
