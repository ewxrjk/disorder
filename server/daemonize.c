/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <syslog.h>

#include "daemonize.h"
#include "syscalls.h"
#include "log.h"

void daemonize(const char *tag, int fac, const char *pidfile) {
  pid_t pid, r;
  int w, dn;
  FILE *fp;

  D(("daemonize tag=%s fac=%d pidfile=%s",
     tag ? tag : "NULL", fac, pidfile ? pidfile : "NULL"));
  /* make sure that FDs 0, 1, 2 all at least exist (and get a
   * /dev/null) */
  do {
    if((dn = open("/dev/null", O_RDWR, 0)) < 0)
      fatal(errno, "error opening /dev/null");
  } while(dn < 3);
  pid = xfork();
  if(pid) {
    /* Parent process.  Wait for the first child to finish, then
     * return to the caller. */
    exitfn = _exit;
    while((r = waitpid(pid, &w, 0)) == -1 && errno == EINTR)
      ;
    if(r < 0) fatal(errno, "error calling waitpid");
    if(w) error(0, "subprocess exited with wait status %#x", (unsigned)w);
    _exit(0);
  }
  /* First child process.  This will be the session leader, and will
   * be transient. */
  D(("first child pid=%lu", (unsigned long)getpid()));
  if(setsid() < 0) fatal(errno, "error calling setsid");
  /* we'll log to syslog */
  openlog(tag, LOG_PID, fac);
  log_default = &log_syslog;
  /* stdin/out/err we lose */
  xdup2(dn, 0);
  xdup2(dn, 1);
  xdup2(dn, 2);
  xclose(dn);
  pid = xfork();
  if(pid)
    _exit(0);
  /* second child.  Write a pidfile if someone wanted it. */
  D(("second child pid=%lu", (unsigned long)getpid()));
  if(pidfile) {
    if(!(fp = fopen(pidfile, "w"))
       || fprintf(fp, "%lu\n", (unsigned long)getpid()) < 0
       || fclose(fp) < 0)
      fatal(errno, "error creating %s", pidfile);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
