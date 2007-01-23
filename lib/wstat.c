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

#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "mem.h"
#include "log.h"
#include "wstat.h"
#include "printf.h"

const char *wstat(int w) {
  int n;
  char *r;

  if(WIFEXITED(w))
    n = byte_xasprintf(&r, "exited with status %d", WEXITSTATUS(w));
  else if(WIFSIGNALED(w))
    n = byte_xasprintf(&r, "terminated by signal %d (%s)%s",
		 WTERMSIG(w), strsignal(WTERMSIG(w)),
		 WCOREDUMP(w) ? " - core dumped" : "");
  else if(WIFSTOPPED(w))
    n = byte_xasprintf(&r, "stopped by signal %d (%s)",
		 WSTOPSIG(w), strsignal(WSTOPSIG(w)));
  else
    n = byte_xasprintf(&r, "terminated with unknown wait status %#x",
		      (unsigned)w);
  return n >= 0 ? r : "[could not convert wait status]";
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:266c61bd39bdfb327908ad3dd95412ae */
