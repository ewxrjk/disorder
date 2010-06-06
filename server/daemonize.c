/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file server/daemonize.c
 * @brief Go into background
 */

#include "disorder-server.h"

/** @brief Go into background
 * @param tag Message tag, or NULL
 * @param fac Logging facility
 * @param pidfile Where to store PID, or NULL
 *
 * Become a daemon.  stdout/stderr are lost and DisOrder's logging is
 * redirected to syslog.  It is assumed that there are no FDs beyond 2
 * that need closing.
 */
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
      disorder_fatal(errno, "error opening /dev/null");
  } while(dn < 3);
  pid = xfork();
  if(pid) {
    /* Parent process.  Wait for the first child to finish, then
     * return to the caller. */
    exitfn = _exit;
    while((r = waitpid(pid, &w, 0)) == -1 && errno == EINTR)
      ;
    if(r < 0) disorder_fatal(errno, "error calling waitpid");
    if(w)
      disorder_error(0, "subprocess exited with wait status %#x", (unsigned)w);
    _exit(0);
  }
  /* First child process.  This will be the session leader, and will
   * be transient. */
  D(("first child pid=%lu", (unsigned long)getpid()));
  if(setsid() < 0)
    disorder_fatal(errno, "error calling setsid");
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
      disorder_fatal(errno, "error creating %s", pidfile);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
