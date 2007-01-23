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
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <stdio.h>
#include "event.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"
#include "printf.h"
#include "sink.h"

struct timeout {
  struct timeout *next;
  struct timeval when;
  ev_timeout_callback *callback;
  void *u;
  int resolve;
};

struct fd {
  int fd;
  ev_fd_callback *callback;
  void *u;
};

struct fdmode {
  fd_set enabled;
  fd_set tripped;
  int nfds, fdslots;
  struct fd *fds;
  int maxfd;
};

struct signal {
  struct sigaction oldsa;
  ev_signal_callback *callback;
  void *u;
};

struct child {
  pid_t pid;
  int options;
  ev_child_callback *callback;
  void *u;
};

struct ev_source {
  struct fdmode mode[ev_nmodes];
  struct timeout *timeouts;
  struct signal signals[NSIG];
  sigset_t sigmask;
  int escape;
  int sigpipe[2];
  int nchildren, nchildslots;
  struct child *children;
};

static const char *modenames[] = { "read", "write", "except" };

/* utilities ******************************************************************/

static inline int gt(const struct timeval *a, const struct timeval *b) {
  if(a->tv_sec > b->tv_sec)
    return 1;
  if(a->tv_sec == b->tv_sec
     && a->tv_usec > b->tv_usec)
    return 1;
  return 0;
}

static inline int ge(const struct timeval *a, const struct timeval *b) {
  return !gt(b, a);
}

/* creation *******************************************************************/

ev_source *ev_new(void) {
  ev_source *ev = xmalloc(sizeof *ev);
  int n;

  memset(ev, 0, sizeof *ev);
  for(n = 0; n < ev_nmodes; ++n)
    FD_ZERO(&ev->mode[n].enabled);
  ev->sigpipe[0] = ev->sigpipe[1] = -1;
  sigemptyset(&ev->sigmask);
  return ev;
}

/* event loop *****************************************************************/

int ev_run(ev_source *ev) {
  for(;;) {
    struct timeval now;
    struct timeval delta;
    int n, mode;
    int ret;
    int maxfd;
    struct timeout *t, **tt;

    xgettimeofday(&now, 0);
    /* Handle timeouts.  We don't want to handle any timeouts that are added
     * while we're handling them (otherwise we'd have to break out of infinite
     * loops, preferrably without starving better-behaved subsystems).  Hence
     * the slightly complicated two-phase approach here. */
    for(t = ev->timeouts;
	t && ge(&now, &t->when);
	t = t->next) {
      t->resolve = 1;
      D(("calling timeout for %ld.%ld callback %p %p",
	 (long)t->when.tv_sec, (long)t->when.tv_usec,
	 (void *)t->callback, t->u));
      ret = t->callback(ev, &now, t->u);
      if(ret)
	return ret;
    }
    tt = &ev->timeouts;
    while((t = *tt)) {
      if(t->resolve)
	*tt = t->next;
      else
	tt = &t->next;
    }
    maxfd = 0;
    for(mode = 0; mode < ev_nmodes; ++mode) {
      ev->mode[mode].tripped = ev->mode[mode].enabled;
      if(ev->mode[mode].maxfd > maxfd)
	maxfd = ev->mode[mode].maxfd;
    }
    xsigprocmask(SIG_UNBLOCK, &ev->sigmask, 0);
    do {
      if(ev->timeouts) {
	xgettimeofday(&now, 0);
	delta.tv_sec = ev->timeouts->when.tv_sec - now.tv_sec;
	delta.tv_usec = ev->timeouts->when.tv_usec - now.tv_usec;
	if(delta.tv_usec < 0) {
	  delta.tv_usec += 1000000;
	  --delta.tv_sec;
	}
	if(delta.tv_sec < 0)
	  delta.tv_sec = delta.tv_usec = 0;
	n = select(maxfd + 1,
		   &ev->mode[ev_read].tripped,
		   &ev->mode[ev_write].tripped,
		   &ev->mode[ev_except].tripped,
		   &delta);
      } else {
	n = select(maxfd + 1,
		   &ev->mode[ev_read].tripped,
		   &ev->mode[ev_write].tripped,
		   &ev->mode[ev_except].tripped,
		   0);
      }
    } while(n < 0 && errno == EINTR);
    xsigprocmask(SIG_BLOCK, &ev->sigmask, 0);
    if(n < 0) {
      error(errno, "error calling select");
      return -1;
    }
    if(n > 0) {
      /* if anything deranges the meaning of an fd, or re-orders the
       * fds[] tables, we'd better give up; such operations will
       * therefore set @escape@. */
      ev->escape = 0;
      for(mode = 0; mode < ev_nmodes && !ev->escape; ++mode)
	for(n = 0; n < ev->mode[mode].nfds && !ev->escape; ++n) {
	  int fd = ev->mode[mode].fds[n].fd;
	  if(FD_ISSET(fd, &ev->mode[mode].tripped)) {
	    D(("calling %s fd %d callback %p %p", modenames[mode], fd,
	       (void *)ev->mode[mode].fds[n].callback,
	       ev->mode[mode].fds[n].u));
	    ret = ev->mode[mode].fds[n].callback(ev, fd,
						 ev->mode[mode].fds[n].u);
	    if(ret)
	      return ret;
	  }
	}
    }
    /* we'll pick up timeouts back round the loop */
  }
}

/* file descriptors ***********************************************************/

int ev_fd(ev_source *ev,
	  ev_fdmode mode,
	  int fd,
	  ev_fd_callback *callback,
	  void *u) {
  int n;

  D(("registering %s fd %d callback %p %p", modenames[mode], fd,
     (void *)callback, u));
  assert(mode < ev_nmodes);
  if(ev->mode[mode].nfds >= ev->mode[mode].fdslots) {
    ev->mode[mode].fdslots = (ev->mode[mode].fdslots
			       ? 2 * ev->mode[mode].fdslots : 16);
    D(("expanding %s fd table to %d entries", modenames[mode],
       ev->mode[mode].fdslots));
    ev->mode[mode].fds = xrealloc(ev->mode[mode].fds,
				  ev->mode[mode].fdslots * sizeof (struct fd));
  }
  n = ev->mode[mode].nfds++;
  FD_SET(fd, &ev->mode[mode].enabled);
  ev->mode[mode].fds[n].fd = fd;
  ev->mode[mode].fds[n].callback = callback;
  ev->mode[mode].fds[n].u = u;
  if(fd > ev->mode[mode].maxfd)
    ev->mode[mode].maxfd = fd;
  ev->escape = 1;
  return 0;
}

int ev_fd_cancel(ev_source *ev, ev_fdmode mode, int fd) {
  int n;
  int maxfd;

  D(("cancelling mode %s fd %d", modenames[mode], fd));
  /* find the right struct fd */
  for(n = 0; n < ev->mode[mode].nfds && fd != ev->mode[mode].fds[n].fd; ++n)
    ;
  assert(n < ev->mode[mode].nfds);
  /* swap in the last fd and reduce the count */
  if(n != ev->mode[mode].nfds - 1)
    ev->mode[mode].fds[n] = ev->mode[mode].fds[ev->mode[mode].nfds - 1];
  --ev->mode[mode].nfds;
  /* if that was the biggest fd, find the new biggest one */
  if(fd == ev->mode[mode].maxfd) {
    maxfd = 0;
    for(n = 0; n < ev->mode[mode].nfds; ++n)
      if(ev->mode[mode].fds[n].fd > maxfd)
	maxfd = ev->mode[mode].fds[n].fd;
    ev->mode[mode].maxfd = maxfd;
  }
  /* don't tell select about this fd any more */
  FD_CLR(fd, &ev->mode[mode].enabled);
  ev->escape = 1;
  return 0;
}

int ev_fd_enable(ev_source *ev, ev_fdmode mode, int fd) {
  D(("enabling mode %s fd %d", modenames[mode], fd));
  FD_SET(fd, &ev->mode[mode].enabled);
  return 0;
}

int ev_fd_disable(ev_source *ev, ev_fdmode mode, int fd) {
  D(("disabling mode %s fd %d", modenames[mode], fd));
  FD_CLR(fd, &ev->mode[mode].enabled);
  FD_CLR(fd, &ev->mode[mode].tripped);
  return 0;
}

/* timeouts *******************************************************************/

int ev_timeout(ev_source *ev,
	       ev_timeout_handle *handlep,
	       const struct timeval *when,
	       ev_timeout_callback *callback,
	       void *u) {
  struct timeout *t, *p, **pp;

  D(("registering timeout at %ld.%ld callback %p %p",
     when ? (long)when->tv_sec : 0, when ? (long)when->tv_usec : 0,
     (void *)callback, u));
  t = xmalloc(sizeof *t);
  if(when)
    t->when = *when;
  t->callback = callback;
  t->u = u;
  pp = &ev->timeouts;
  while((p = *pp) && gt(&t->when, &p->when))
    pp = &p->next;
  t->next = p;
  *pp = t;
  if(handlep)
    *handlep = t;
  return 0;
}

int ev_timeout_cancel(ev_source *ev,
		      ev_timeout_handle handle) {
  struct timeout *t = handle, *p, **pp;

  for(pp = &ev->timeouts; (p = *pp) && p != t; pp = &p->next)
    ;
  if(p) {
    *pp = p->next;
    return 0;
  } else
    return -1;
}

/* signals ********************************************************************/

static int sigfd[NSIG];

static void sighandler(int s) {
  unsigned char sc = s;
  static const char errmsg[] = "error writing to signal pipe";

  /* probably the reader has stopped listening for some reason */
  if(write(sigfd[s], &sc, 1) < 0) {
    write(2, errmsg, sizeof errmsg - 1);
    abort();
  }
}

static int signal_read(ev_source *ev,
		       int attribute((unused)) fd,
		       void attribute((unused)) *u) {
  unsigned char s;
  int n;
  int ret;

  if((n = read(ev->sigpipe[0], &s, 1)) == 1)
    if((ret = ev->signals[s].callback(ev, s, ev->signals[s].u)))
      return ret;
  assert(n != 0);
  if(n < 0 && (errno != EINTR && errno != EAGAIN)) {
    error(errno, "error reading from signal pipe %d", ev->sigpipe[0]);
    return -1;
  }
  return 0;
}

static void close_sigpipe(ev_source *ev) {
  int save_errno = errno;

  xclose(ev->sigpipe[0]);
  xclose(ev->sigpipe[1]);
  ev->sigpipe[0] = ev->sigpipe[1] = -1;
  errno = save_errno;
}

int ev_signal(ev_source *ev,
	      int sig,
	      ev_signal_callback *callback,
	      void *u) {
  int n;
  struct sigaction sa;

  D(("registering signal %d handler callback %p %p", sig, (void *)callback, u));
  assert(sig > 0);
  assert(sig < NSIG);
  assert(sig <= UCHAR_MAX);
  if(ev->sigpipe[0] == -1) {
    D(("creating signal pipe"));
    xpipe(ev->sigpipe);
    D(("signal pipe is %d, %d", ev->sigpipe[0], ev->sigpipe[1]));
    for(n = 0; n < 2; ++n) {
      nonblock(ev->sigpipe[n]);
      cloexec(ev->sigpipe[n]);
    }
    if(ev_fd(ev, ev_read, ev->sigpipe[0], signal_read, 0)) {
      close_sigpipe(ev);
      return -1;
    }
  }
  sigaddset(&ev->sigmask, sig);
  xsigprocmask(SIG_BLOCK, &ev->sigmask, 0);
  sigfd[sig] = ev->sigpipe[1];
  ev->signals[sig].callback = callback;
  ev->signals[sig].u = u;
  sa.sa_handler = sighandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  xsigaction(sig, &sa, &ev->signals[sig].oldsa);
  ev->escape = 1;
  return 0;
}

int ev_signal_cancel(ev_source *ev,
		     int sig) {
  sigset_t ss;

  xsigaction(sig, &ev->signals[sig].oldsa, 0);
  ev->signals[sig].callback = 0;
  ev->escape = 1;
  sigdelset(&ev->sigmask, sig);
  sigemptyset(&ss);
  sigaddset(&ss, sig);
  xsigprocmask(SIG_UNBLOCK, &ss, 0);
  return 0;
}

void ev_signal_atfork(ev_source *ev) {
  int sig;

  if(ev->sigpipe[0] != -1) {
    /* revert any handled signals to their original state */
    for(sig = 1; sig < NSIG; ++sig) {
      if(ev->signals[sig].callback != 0)
	xsigaction(sig, &ev->signals[sig].oldsa, 0);
    }
    /* and then unblock them */
    xsigprocmask(SIG_UNBLOCK, &ev->sigmask, 0);
    /* don't want a copy of the signal pipe open inside the fork */
    xclose(ev->sigpipe[0]);
    xclose(ev->sigpipe[1]);
  }
}

/* child processes ************************************************************/

static int sigchld_callback(ev_source *ev,
			    int attribute((unused)) sig,
			    void attribute((unused)) *u) {
  struct rusage ru;
  pid_t r;
  int status, n, ret, revisit;

  do {
    revisit = 0;
    for(n = 0; n < ev->nchildren; ++n) {
      r = wait4(ev->children[n].pid,
		&status,
		ev->children[n].options | WNOHANG,
		&ru);
      if(r > 0) {
	ev_child_callback *c = ev->children[n].callback;
	void *cu = ev->children[n].u;

	if(WIFEXITED(status) || WIFSIGNALED(status))
	  ev_child_cancel(ev, r);
	revisit = 1;
	if((ret = c(ev, r, status, &ru, cu)))
	  return ret;
      } else if(r < 0) {
	/* We should "never" get an ECHILD but it can in fact happen.  For
	 * instance on Linux 2.4.31, and probably other versions, if someone
	 * straces a child process and then a different child process
	 * terminates, when we wait4() the trace process we will get ECHILD
	 * because it has been reparented to strace.  Obviously this is a
	 * hopeless design flaw in the tracing infrastructure, but we don't
	 * want the disorder server to bomb out because of it.  So we just log
	 * the problem and ignore it.
	 */
	error(errno, "error calling wait4 for PID %lu (broken ptrace?)",
	      (unsigned long)ev->children[n].pid);
	if(errno != ECHILD)
	  return -1;
      }
    }
  } while(revisit);
  return 0;
}

int ev_child_setup(ev_source *ev) {
  D(("installing SIGCHLD handler"));
  return ev_signal(ev, SIGCHLD, sigchld_callback, 0);
}

int ev_child(ev_source *ev,
	     pid_t pid,
	     int options,
	     ev_child_callback *callback,
	     void *u) {
  int n;

  D(("registering child handling %ld options %d callback %p %p",
     (long)pid, options, (void *)callback, u));
  assert(ev->signals[SIGCHLD].callback == sigchld_callback);
  if(ev->nchildren >= ev->nchildslots) {
    ev->nchildslots = ev->nchildslots ? 2 * ev->nchildslots : 16;
    ev->children = xrealloc(ev->children,
			    ev->nchildslots * sizeof (struct child));
  }
  n = ev->nchildren++;
  ev->children[n].pid = pid;
  ev->children[n].options = options;
  ev->children[n].callback = callback;
  ev->children[n].u = u;
  return 0;
}

int ev_child_cancel(ev_source *ev,
		    pid_t pid) {
  int n;

  for(n = 0; n < ev->nchildren && ev->children[n].pid != pid; ++n)
    ;
  assert(n < ev->nchildren);
  if(n != ev->nchildren - 1)
    ev->children[n] = ev->children[ev->nchildren - 1];
  --ev->nchildren;
  return 0;
}

/* socket listeners ***********************************************************/

struct listen_state {
  ev_listen_callback *callback;
  void *u;
};

static int listen_callback(ev_source *ev, int fd, void *u) {
  const struct listen_state *l = u;
  int newfd;
  union {
    struct sockaddr_in in;
#if HAVE_STRUCT_SOCKADDR_IN6
    struct sockaddr_in6 in6;
#endif
    struct sockaddr_un un;
    struct sockaddr sa;
  } addr;
  socklen_t addrlen;
  int ret;

  D(("callback for listener fd %d", fd));
  while((addrlen = sizeof addr),
	(newfd = accept(fd, &addr.sa, &addrlen)) >= 0) {
    if((ret = l->callback(ev, newfd, &addr.sa, addrlen, l->u)))
      return ret;
  }
  switch(errno) {
  case EINTR:
  case EAGAIN:
    break;
#ifdef ECONNABORTED
  case ECONNABORTED:
    error(errno, "error calling accept");
    break;
#endif
#ifdef EPROTO
  case EPROTO:
    /* XXX on some systems EPROTO should be fatal, but we don't know if
     * we're running on one of them */
    error(errno, "error calling accept");
    break;
#endif
  default:
    fatal(errno, "error calling accept");
    break;
  }
  if(errno != EINTR && errno != EAGAIN)
    error(errno, "error calling accept");
  return 0;
}

int ev_listen(ev_source *ev,
	      int fd,
	      ev_listen_callback *callback,
	      void *u) {
  struct listen_state *l = xmalloc(sizeof *l);

  D(("registering listener fd %d callback %p %p", fd, (void *)callback, u));
  l->callback = callback;
  l->u = u;
  return ev_fd(ev, ev_read, fd, listen_callback, l);
}

int ev_listen_cancel(ev_source *ev, int fd) {
  D(("cancelling listener fd %d", fd));
  return ev_fd_cancel(ev, ev_read, fd);
}

/* buffer *********************************************************************/

struct buffer {
  char *base, *start, *end, *top;
};

/* make sure there is @bytes@ available at @b->end@ */
static void buffer_space(struct buffer *b, size_t bytes) {
  D(("buffer_space %p %p %p %p want %lu",
     (void *)b->base, (void *)b->start, (void *)b->end, (void *)b->top,
     (unsigned long)bytes));
  if(b->start == b->end)
    b->start = b->end = b->base;
  if((size_t)(b->top - b->end) < bytes) {
    if((size_t)((b->top - b->end) + (b->start - b->base)) < bytes) {
      size_t newspace = b->end - b->start + bytes, n;
      char *newbase;

      for(n = 16; n < newspace; n *= 2)
	;
      newbase = xmalloc_noptr(n);
      memcpy(newbase, b->start, b->end - b->start);
      b->base = newbase;
      b->end = newbase + (b->end - b->start);
      b->top = newbase + n;
      b->start = newbase;		/* must be last */
    } else {
      memmove(b->base, b->start, b->end - b->start);
      b->end = b->base + (b->end - b->start);
      b->start = b->base;
    }
  }
  D(("result %p %p %p %p",
     (void *)b->base, (void *)b->start, (void *)b->end, (void *)b->top));
}

/* buffered writer ************************************************************/

struct ev_writer {
  struct sink s;
  struct buffer b;
  int fd;
  int eof;
  ev_error_callback *callback;
  void *u;
  ev_source *ev;
};

static int writer_callback(ev_source *ev, int fd, void *u) {
  ev_writer *w = u;
  int n;

  n = write(fd, w->b.start, w->b.end - w->b.start);
  D(("callback for writer fd %d, %ld bytes, n=%d, errno=%d",
     fd, (long)(w->b.end - w->b.start), n, errno));
  if(n >= 0) {
    w->b.start += n;
    if(w->b.start == w->b.end) {
      if(w->eof) {
	ev_fd_cancel(ev, ev_write, fd);
	return w->callback(ev, fd, 0, w->u);
      } else
	ev_fd_disable(ev, ev_write, fd);
    }
  } else {
    switch(errno) {
    case EINTR:
    case EAGAIN:
      break;
    default:
      ev_fd_cancel(ev, ev_write, fd);
      return w->callback(ev, fd, errno, w->u);
    }
  }
  return 0;
}

static int ev_writer_write(struct sink *sk, const void *s, int n) {
  ev_writer *w = (ev_writer *)sk;
  
  buffer_space(&w->b, n);
  if(w->b.start == w->b.end)
    ev_fd_enable(w->ev, ev_write, w->fd);
  memcpy(w->b.end, s, n);
  w->b.end += n;
  return 0;
}

ev_writer *ev_writer_new(ev_source *ev,
			 int fd,
			 ev_error_callback *callback,
			 void *u) {
  ev_writer *w = xmalloc(sizeof *w);

  D(("registering writer fd %d callback %p %p", fd, (void *)callback, u));
  w->s.write = ev_writer_write;
  w->fd = fd;
  w->callback = callback;
  w->u = u;
  w->ev = ev;
  if(ev_fd(ev, ev_write, fd, writer_callback, w))
    return 0;
  ev_fd_disable(ev, ev_write, fd);
  return w;
}

struct sink *ev_writer_sink(ev_writer *w) {
  return &w->s;
}

static int writer_shutdown(ev_source *ev,
			   const attribute((unused)) struct timeval *now,
			   void *u) {
  ev_writer *w = u;

  return w->callback(ev, w->fd, 0, w->u);
}

int ev_writer_close(ev_writer *w) {
  D(("close writer fd %d", w->fd));
  w->eof = 1;
  if(w->b.start == w->b.end) {
    /* we're already finished */
    ev_fd_cancel(w->ev, ev_write, w->fd);
    return ev_timeout(w->ev, 0, 0, writer_shutdown, w);
  }
  return 0;
}

int ev_writer_cancel(ev_writer *w) {
  D(("cancel writer fd %d", w->fd));
  return ev_fd_cancel(w->ev, ev_write, w->fd);
}

int ev_writer_flush(ev_writer *w) {
  return writer_callback(w->ev, w->fd, w);
}

/* buffered reader ************************************************************/

struct ev_reader {
  struct buffer b;
  int fd;
  ev_reader_callback *callback;
  ev_error_callback *error_callback;
  void *u;
  ev_source *ev;
  int eof;
};

static int reader_callback(ev_source *ev, int fd, void *u) {
  ev_reader *r = u;
  int n;

  buffer_space(&r->b, 1);
  n = read(fd, r->b.end, r->b.top - r->b.end);
  D(("read fd %d buffer %d returned %d errno %d",
     fd, (int)(r->b.top - r->b.end), n, errno));
  if(n > 0) {
    r->b.end += n;
    return r->callback(ev, r, fd, r->b.start, r->b.end - r->b.start, 0, r->u);
  } else if(n == 0) {
    r->eof = 1;
    ev_fd_cancel(ev, ev_read, fd);
    return r->callback(ev, r, fd, r->b.start, r->b.end - r->b.start, 1, r->u);
  } else {
    switch(errno) {
    case EINTR:
    case EAGAIN:
      break;
    default:
      ev_fd_cancel(ev, ev_read, fd);
      return r->error_callback(ev, fd, errno, r->u);
    }
  }
  return 0;
}

ev_reader *ev_reader_new(ev_source *ev,
			 int fd,
			 ev_reader_callback *callback,
			 ev_error_callback *error_callback,
			 void *u) {
  ev_reader *r = xmalloc(sizeof *r);

  D(("registering reader fd %d callback %p %p %p",
     fd, (void *)callback, (void *)error_callback, u));
  r->fd = fd;
  r->callback = callback;
  r->error_callback = error_callback;
  r->u = u;
  r->ev = ev;
  if(ev_fd(ev, ev_read, fd, reader_callback, r))
    return 0;
  return r;
}

void ev_reader_buffer(ev_reader *r, size_t nbytes) {
  buffer_space(&r->b, nbytes - (r->b.end - r->b.start));
}

void ev_reader_consume(ev_reader *r, size_t n) {
  r->b.start += n;
}

int ev_reader_cancel(ev_reader *r) {
  D(("cancel reader fd %d", r->fd));
  return ev_fd_cancel(r->ev, ev_read, r->fd);
}

int ev_reader_disable(ev_reader *r) {
  D(("disable reader fd %d", r->fd));
  return r->eof ? 0 : ev_fd_disable(r->ev, ev_read, r->fd);
}

static int reader_continuation(ev_source attribute((unused)) *ev,
			       const attribute((unused)) struct timeval *now,
			       void *u) {
  ev_reader *r = u;

  D(("reader continuation callback fd %d", r->fd));
  if(ev_fd_enable(r->ev, ev_read, r->fd)) return -1;
  return r->callback(ev, r, r->fd, r->b.start, r->b.end - r->b.start, r->eof, r->u);
}

int ev_reader_incomplete(ev_reader *r) {
  if(ev_fd_disable(r->ev, ev_read, r->fd)) return -1;
  return ev_timeout(r->ev, 0, 0, reader_continuation, r);
}

static int reader_enabled(ev_source *ev,
			  const attribute((unused)) struct timeval *now,
			  void *u) {
  ev_reader *r = u;

  D(("reader enabled callback fd %d", r->fd));
  return r->callback(ev, r, r->fd, r->b.start, r->b.end - r->b.start, r->eof, r->u);
}

int ev_reader_enable(ev_reader *r) {
  D(("enable reader fd %d", r->fd));
  return ((r->eof ? 0 : ev_fd_enable(r->ev, ev_read, r->fd))
	  || ev_timeout(r->ev, 0, 0, reader_enabled, r)) ? -1 : 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
