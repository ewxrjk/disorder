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

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "syscalls.h"
#include "log.h"
#include "printf.h"

int mustnotbeminus1(const char *what, int ret) {
  if(ret == -1)
    fatal(errno, "error calling %s", what);
  return ret;
}

pid_t xfork(void) {
  pid_t pid;

  if((pid = fork()) < 0) fatal(errno, "error calling fork");
  return pid;
}

void xclose_guts(const char *path, int line, int fd) {
  if(close(fd) < 0)
    fatal(errno, "%s:%d: close %d", path, line, fd);
}

void xdup2(int fd1, int fd2) {
  mustnotbeminus1("dup2", dup2(fd1, fd2));
}

void xpipe(int *fdp) {
  mustnotbeminus1("pipe", pipe(fdp));
}

void nonblock(int fd) {
  mustnotbeminus1("fcntl F_SETFL",
		  fcntl(fd, F_SETFL,
			mustnotbeminus1("fcntl F_GETFL",
					fcntl(fd, F_GETFL)) | O_NONBLOCK));
}

void blocking(int fd) {
  mustnotbeminus1("fcntl F_SETFL",
		  fcntl(fd, F_SETFL,
			mustnotbeminus1("fcntl F_GETFL",
					fcntl(fd, F_GETFL)) & ~O_NONBLOCK));
}

void cloexec(int fd) {
  mustnotbeminus1("fcntl F_SETFD",
		  fcntl(fd, F_SETFD,
			mustnotbeminus1("fcntl F_GETFD",
					fcntl(fd, F_GETFD)) | FD_CLOEXEC));
}

void xlisten(int fd, int q) {
  mustnotbeminus1("listen", listen(fd, q));
}

void xshutdown(int fd, int how) {
  mustnotbeminus1("shutdown", shutdown(fd, how));
}

void xsetsockopt(int fd, int l, int o, const void *v, socklen_t vl) {
  mustnotbeminus1("setsockopt", setsockopt(fd, l, o, v, vl));
}

int xsocket(int d, int t, int p) {
  return mustnotbeminus1("socket", socket(d, t, p));
}

void xconnect(int fd, const struct sockaddr *sa, socklen_t sl) {
  mustnotbeminus1("connect", connect(fd, sa, sl));
}

void xsigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  mustnotbeminus1("sigprocmask", sigprocmask(how, set, oldset));
}

void xsigaction(int sig, const struct sigaction *sa, struct sigaction *oldsa) {
  mustnotbeminus1("sigaction", sigaction(sig, sa, oldsa));
}

int xprintf(const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = mustnotbeminus1("byte_vfprintf", byte_vfprintf(stdout, fmt, ap));
  va_end(ap);
  return n;
}

void xfclose(FILE *fp) {
  mustnotbeminus1("fclose", fclose(fp));
}

int xstrtol(long *n, const char *s, char **ep, int base) {
  errno = 0;
  *n = strtol(s, ep, base);
  return errno;
}

int xstrtoll(long_long *n, const char *s, char **ep, int base) {
  errno = 0;
  *n = strtoll(s, ep, base);
  return errno;
}

int xnice(int inc) {
  int ret;

  /* some versions of nice() return the new nice value which in principle could
   * be -1 */
  errno = 0;
  ret = nice(inc);
  if(errno) fatal(errno, "error calling nice");
  return ret;
}

void xgettimeofday(struct timeval *tv, struct timezone *tz) {
  mustnotbeminus1("gettimeofday", gettimeofday(tv, tz));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
