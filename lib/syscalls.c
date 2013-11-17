/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007-9, 2013 Richard Kettlewell
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
/** @file lib/syscalls.c
 * @brief Error-checking library call wrappers
 */
#include "common.h"

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <signal.h>
#include <time.h>

#include "syscalls.h"
#include "log.h"
#include "printf.h"

int mustnotbeminus1(const char *what, int ret) {
  if(ret == -1)
    disorder_fatal(errno, "error calling %s", what);
  return ret;
}

#if !_WIN32
pid_t xfork(void) {
  pid_t pid;

  if((pid = fork()) < 0)
    disorder_fatal(errno, "error calling fork");
  return pid;
}

void xclose_guts(const char *path, int line, int fd) {
  if(close(fd) < 0)
    disorder_fatal(errno, "%s:%d: close %d", path, line, fd);
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
#endif

void xlisten(SOCKET fd, int q) {
  mustnotbeminus1("listen", listen(fd, q));
}

void xshutdown(SOCKET fd, int how) {
  mustnotbeminus1("shutdown", shutdown(fd, how));
}

void xsetsockopt(SOCKET fd, int l, int o, const void *v, socklen_t vl) {
  mustnotbeminus1("setsockopt", setsockopt(fd, l, o, v, vl));
}

SOCKET xsocket(int d, int t, int p) {
  SOCKET s = socket(d, t, p);
  if(s == INVALID_SOCKET)
    disorder_fatal(errno, "error calling socket");
  return s;
}

void xconnect(SOCKET fd, const struct sockaddr *sa, socklen_t sl) {
  mustnotbeminus1("connect", connect(fd, sa, sl));
}

#if !_WIN32
void xsigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  mustnotbeminus1("sigprocmask", sigprocmask(how, set, oldset));
}

void xsigaction(int sig, const struct sigaction *sa, struct sigaction *oldsa) {
  mustnotbeminus1("sigaction", sigaction(sig, sa, oldsa));
}
#endif

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

#if !_WIN32
int xnice(int inc) {
  int ret;

  /* some versions of nice() return the new nice value which in principle could
   * be -1 */
  errno = 0;
  ret = nice(inc);
  if(errno)
    disorder_fatal(errno, "error calling nice");
  return ret;
}
#endif

void xgettimeofday(struct timeval *tv, struct timezone *tz) {
  mustnotbeminus1("gettimeofday", gettimeofday(tv, tz));
}

time_t xtime(time_t *whenp) {
  struct timeval tv;

  xgettimeofday(&tv, NULL);
  if(whenp)
    *whenp = tv.tv_sec;
  return tv.tv_sec;
}

#if !_WIN32
void xnanosleep(const struct timespec *req, struct timespec *rem) {
  mustnotbeminus1("nanosleep", nanosleep(req, rem));
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
