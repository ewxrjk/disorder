/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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

#ifndef SYSCALLS_H
#define SYSCALLS_H

/* various error-handling wrappers.  Not actually all system calls! */

struct sockaddr;
struct sigaction;
struct timezone;

#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>

#include "types.h"

pid_t xfork(void);
void xclose_guts(const char *, int, int);
#define xclose(fd) xclose_guts(__FILE__, __LINE__, fd)
void xdup2(int, int);
void xpipe(int *);
int xfcntl(int, int, long);
void xlisten(int, int);
void xshutdown(int, int);
void xsetsockopt(int, int, int, const void *, socklen_t);
int xsocket(int, int, int);
void xconnect(int, const struct sockaddr *, socklen_t);
void xsigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void xsigaction(int sig, const struct sigaction *sa, struct sigaction *oldsa);
int xprintf(const char *, ...)
  attribute((format (printf, 1, 2)));
void xfclose(FILE *);
int xnice(int);
void xgettimeofday(struct timeval *, struct timezone *);
/* the above all call @fatal@ if the system call fails */

void nonblock(int fd);
void blocking(int fd);
void cloexec(int fd);
/* make @fd@ non-blocking/blocking/close-on-exec; call @fatal@ on error. */

int mustnotbeminus1(const char *what, int value);
/* If @value@ is -1, report an error including @what@. */

int xstrtol(long *n, const char *s, char **ep, int base);
int xstrtoll(long_long *n, const char *s, char **ep, int base);
/* like strtol() but returns errno on error, 0 on success */

#endif /* SYSCALLS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
