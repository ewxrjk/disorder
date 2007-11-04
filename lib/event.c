/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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
/** @file lib/event.c
 * @brief DisOrder event loop
 */

#include <config.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
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
#include "vector.h"

/** @brief A timeout */
struct timeout {
  struct timeout *next;
  struct timeval when;
  ev_timeout_callback *callback;
  void *u;
  int resolve;
};

/** @brief A file descriptor in one mode */
struct fd {
  int fd;
  ev_fd_callback *callback;
  void *u;
  const char *what;
};

/** @brief All the file descriptors in a given mode */
struct fdmode {
  /** @brief Mask of active file descriptors passed to @c select() */
  fd_set enabled;

  /** @brief File descriptor mask returned from @c select() */
  fd_set tripped;

  /** @brief Number of file descriptors in @p fds */
  int nfds;

  /** @brief Number of slots in @p fds */
  int fdslots;

  /** @brief Array of all active file descriptors */
  struct fd *fds;

  /** @brief Highest-numbered file descriptor or 0 */
  int maxfd;
};

/** @brief A signal handler */
struct signal {
  struct sigaction oldsa;
  ev_signal_callback *callback;
  void *u;
};

/** @brief A child process */
struct child {
  pid_t pid;
  int options;
  ev_child_callback *callback;
  void *u;
};

/** @brief An event loop */
struct ev_source {
  /** @brief File descriptors, per mode */
  struct fdmode mode[ev_nmodes];

  /** @brief Sorted linked list of timeouts
   *
   * We could use @ref HEAP_TYPE now, but there aren't many timeouts.
   */
  struct timeout *timeouts;

  /** @brief Array of handled signals */
  struct signal signals[NSIG];

  /** @brief Mask of handled signals */
  sigset_t sigmask;

  /** @brief Escape early from handling of @c select() results
   *
   * This is set if any of the file descriptor arrays are invalidated, since
   * it's then not safe for processing of them to continue.
   */
  int escape;

  /** @brief Signal handling pipe
   *
   * The signal handle writes signal numbers down this pipe.
   */
  int sigpipe[2];

  /** @brief Number of child processes in @p children */
  int nchildren;

  /** @brief Number of slots in @p children */
  int nchildslots;

  /** @brief Array of child processes */
  struct child *children;
};

/** @brief Names of file descriptor modes */
static const char *modenames[] = { "read", "write", "except" };

/* utilities ******************************************************************/

/** @brief Great-than comparison for timevals
 *
 * Ought to be in @file lib/timeval.h
 */
static inline int gt(const struct timeval *a, const struct timeval *b) {
  if(a->tv_sec > b->tv_sec)
    return 1;
  if(a->tv_sec == b->tv_sec
     && a->tv_usec > b->tv_usec)
    return 1;
  return 0;
}

/** @brief Greater-than-or-equal comparison for timevals
 *
 * Ought to be in @file lib/timeval.h
 */
static inline int ge(const struct timeval *a, const struct timeval *b) {
  return !gt(b, a);
}

/* creation *******************************************************************/

/** @brief Create a new event loop */
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

/** @brief Run the event loop
 * @return -1 on error, non-0 if any callback returned non-0
 */
int ev_run(ev_source *ev) {
  for(;;) {
    struct timeval now;
    struct timeval delta;
    int n, mode;
    int ret;
    int maxfd;
    struct timeout *t, **tt;
    struct stat sb;

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
      if(errno == EBADF) {
	/* If there's a bad FD in the mix then check them all and log what we
	 * find, to ease debugging */
	for(mode = 0; mode < ev_nmodes; ++mode) {
	  for(n = 0; n < ev->mode[mode].nfds; ++n) {
	    const int fd = ev->mode[mode].fds[n].fd;

	    if(FD_ISSET(fd, &ev->mode[mode].enabled)
	       && fstat(fd, &sb) < 0)
	      error(errno, "mode %s fstat %d (%s)",
		    modenames[mode], fd, ev->mode[mode].fds[n].what);
	  }
	  for(n = 0; n <= maxfd; ++n)
	    if(FD_ISSET(n, &ev->mode[mode].enabled)
	       && fstat(n, &sb) < 0)
	      error(errno, "mode %s fstat %d", modenames[mode], n);
	}
      }
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

/** @brief Register a file descriptor
 * @param ev Event loop
 * @param mode @c ev_read or @c ev_write
 * @param fd File descriptor
 * @param callback Called when @p is readable/writable
 * @param u Passed to @p callback
 * @param what Text description
 * @return 0 on success, non-0 on error
 *
 * Sets @ref ev_source::escape, so no further processing of file descriptors
 * will occur this time round the event loop.
 */
int ev_fd(ev_source *ev,
	  ev_fdmode mode,
	  int fd,
	  ev_fd_callback *callback,
	  void *u,
	  const char *what) {
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
  ev->mode[mode].fds[n].what = what;
  if(fd > ev->mode[mode].maxfd)
    ev->mode[mode].maxfd = fd;
  ev->escape = 1;
  return 0;
}

/** @brief Cancel a file descriptor
 * @param ev Event loop
 * @param mode @c ev_read or @c ev_write
 * @param fd File descriptor
 * @return 0 on success, non-0 on error
 *
 * Sets @ref ev_source::escape, so no further processing of file descriptors
 * will occur this time round the event loop.
 */
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

/** @brief Re-enable a file descriptor
 * @param ev Event loop
 * @param mode @c ev_read or @c ev_write
 * @param fd File descriptor
 * @return 0 on success, non-0 on error
 *
 * It is harmless if @p fd is currently disabled, but it must not have been
 * cancelled.
 */
int ev_fd_enable(ev_source *ev, ev_fdmode mode, int fd) {
  D(("enabling mode %s fd %d", modenames[mode], fd));
  FD_SET(fd, &ev->mode[mode].enabled);
  return 0;
}

/** @brief Temporarily disable a file descriptor
 * @param ev Event loop
 * @param mode @c ev_read or @c ev_write
 * @param fd File descriptor
 * @return 0 on success, non-0 on error
 *
 * Re-enable with ev_fd_enable().  It is harmless if @p fd is already disabled,
 * but it must not have been cancelled.
 */
int ev_fd_disable(ev_source *ev, ev_fdmode mode, int fd) {
  D(("disabling mode %s fd %d", modenames[mode], fd));
  FD_CLR(fd, &ev->mode[mode].enabled);
  FD_CLR(fd, &ev->mode[mode].tripped);
  return 0;
}

/** @brief Log a report of file descriptor state */
void ev_report(ev_source *ev) {
  int n, fd;
  ev_fdmode mode;
  struct dynstr d[1];
  char b[4096];

  dynstr_init(d);
  for(mode = 0; mode < ev_nmodes; ++mode) {
    info("mode %s maxfd %d", modenames[mode], ev->mode[mode].maxfd);
    for(n = 0; n < ev->mode[mode].nfds; ++n) {
      fd = ev->mode[mode].fds[n].fd;
      info("fd %s %d%s%s (%s)", modenames[mode], fd,
	   FD_ISSET(fd, &ev->mode[mode].enabled) ? " enabled" : "",
	   FD_ISSET(fd, &ev->mode[mode].tripped) ? " tripped" : "",
	   ev->mode[mode].fds[n].what);
    }
    d->nvec = 0;
    for(fd = 0; fd <= ev->mode[mode].maxfd; ++fd) {
      if(!FD_ISSET(fd, &ev->mode[mode].enabled))
	continue;
      for(n = 0; n < ev->mode[mode].nfds; ++n) {
	if(ev->mode[mode].fds[n].fd == fd)
	  break;
      }
      if(n < ev->mode[mode].nfds)
	snprintf(b, sizeof b, "%d(%s)", fd, ev->mode[mode].fds[n].what);
      else
	snprintf(b, sizeof b, "%d", fd);
      dynstr_append(d, ' ');
      dynstr_append_string(d, b);
    }
    dynstr_terminate(d);
    info("%s enabled:%s", modenames[mode], d->vec);
  }
}

/* timeouts *******************************************************************/

/** @brief Register a timeout
 * @param ev Event source
 * @param handle Where to store timeout handle, or @c NULL
 * @param when Earliest time to call @p callback, or @c NULL
 * @param callback Function to call at or after @p when
 * @param u Passed to @p callback
 * @return 0 on success, non-0 on error
 *
 * If @p when is a null pointer then a time of 0 is assumed.  The effect is to
 * call the timeout handler from ev_run() next time around the event loop.
 * This is used internally to schedule various operations if it is not
 * convenient to call them from the current place in the call stack, or
 * externally to ensure that other clients of the event loop get a look in when
 * performing some lengthy operation.
 */
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

/** @brief Cancel a timeout
 * @param ev Event loop
 * @param handle Handle returned from ev_timeout(), or 0
 * @return 0 on success, non-0 on error
 *
 * If @p handle is 0 then this is a no-op.
 */
int ev_timeout_cancel(ev_source *ev,
		      ev_timeout_handle handle) {
  struct timeout *t = handle, *p, **pp;

  if(!t)
    return 0;
  for(pp = &ev->timeouts; (p = *pp) && p != t; pp = &p->next)
    ;
  if(p) {
    *pp = p->next;
    return 0;
  } else
    return -1;
}

/* signals ********************************************************************/

/** @brief Mapping of signals to pipe write ends
 *
 * The pipes are per-event loop, it's possible in theory for there to be
 * multiple event loops (e.g. in different threads), although in fact DisOrder
 * does not do this.
 */
static int sigfd[NSIG];

/** @brief The signal handler
 * @param s Signal number
 *
 * Writes to @c sigfd[s].
 */
static void sighandler(int s) {
  unsigned char sc = s;
  static const char errmsg[] = "error writing to signal pipe";

  /* probably the reader has stopped listening for some reason */
  if(write(sigfd[s], &sc, 1) < 0) {
    write(2, errmsg, sizeof errmsg - 1);
    abort();
  }
}

/** @brief Read callback for signals */
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

/** @brief Close the signal pipe */
static void close_sigpipe(ev_source *ev) {
  int save_errno = errno;

  xclose(ev->sigpipe[0]);
  xclose(ev->sigpipe[1]);
  ev->sigpipe[0] = ev->sigpipe[1] = -1;
  errno = save_errno;
}

/** @brief Register a signal handler
 * @param ev Event loop
 * @param sig Signal to handle
 * @param callback Called when signal is delivered
 * @param u Passed to @p callback
 * @return 0 on success, non-0 on error
 *
 * Note that @p callback is called from inside ev_run(), not from inside the
 * signal handler, so the usual restrictions on signal handlers do not apply.
 */
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
    if(ev_fd(ev, ev_read, ev->sigpipe[0], signal_read, 0, "sigpipe read")) {
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

/** @brief Cancel a signal handler
 * @param ev Event loop
 * @param sig Signal to cancel
 * @return 0 on success, non-0 on error
 */
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

/** @brief Clean up signal handling
 * @param ev Event loop
 *
 * This function can be called from inside a fork.  It restores signal
 * handlers, unblocks the signals, and closes the signal pipe for @p ev.
 */
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

/** @brief Called on SIGCHLD */
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

/** @brief Configure event loop for child process handling
 * @return 0 on success, non-0 on error
 *
 * Currently at most one event loop can handle child processes and it must be
 * distinguished from others by calling this function on it.  This could be
 * fixed but since no process ever makes use of more than one event loop there
 * is no need.
 */
int ev_child_setup(ev_source *ev) {
  D(("installing SIGCHLD handler"));
  return ev_signal(ev, SIGCHLD, sigchld_callback, 0);
}

/** @brief Wait for a child process to terminate
 * @param ev Event loop
 * @param pid Process ID of child
 * @param options Options to pass to @c wait4()
 * @param callback Called when child terminates (or possibly when it stops)
 * @param u Passed to @p callback
 * @return 0 on success, non-0 on error
 *
 * You must have called ev_child_setup() on @p ev once first.
 */
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

/** @brief Stop waiting for a child process
 * @param ev Event loop
 * @param pid Child process ID
 * @return 0 on success, non-0 on error
 */ 
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

/** @brief State for a socket listener */
struct listen_state {
  ev_listen_callback *callback;
  void *u;
};

/** @brief Called when a listenign socket is readable */
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

/** @brief Listen on a socket for inbound stream connections
 * @param ev Event source
 * @param fd File descriptor of socket
 * @param callback Called when a new connection arrives
 * @param u Passed to @p callback
 * @param what Text description of socket
 * @return 0 on success, non-0 on error
 */
int ev_listen(ev_source *ev,
	      int fd,
	      ev_listen_callback *callback,
	      void *u,
	      const char *what) {
  struct listen_state *l = xmalloc(sizeof *l);

  D(("registering listener fd %d callback %p %p", fd, (void *)callback, u));
  l->callback = callback;
  l->u = u;
  return ev_fd(ev, ev_read, fd, listen_callback, l, what);
}

/** @brief Stop listening on a socket
 * @param ev Event loop
 * @param fd File descriptor of socket
 * @return 0 on success, non-0 on error
 */ 
int ev_listen_cancel(ev_source *ev, int fd) {
  D(("cancelling listener fd %d", fd));
  return ev_fd_cancel(ev, ev_read, fd);
}

/* buffer *********************************************************************/

/** @brief Buffer structure */
struct buffer {
  char *base, *start, *end, *top;
};

/* @brief Make sure there is @p bytes available at @c b->end */
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

/** @brief State structure for a buffered writer */
struct ev_writer {
  struct sink s;
  struct buffer b;
  int fd;
  int eof;
  ev_error_callback *callback;
  void *u;
  ev_source *ev;

  /** @brief Maximum amount of time between succesful writes, 0 = don't care */
  int timebound;
  /** @brief Maximum amount of data to buffer, 0 = don't care */
  int spacebound;
  /** @brief Synthesized error code */
  int syntherror;
  /** @brief Timeout handle for @p timebound (or 0) */
  ev_timeout_handle timeout;

  const char *what;
};

/** @brief Synthesized error callback
 *
 * Calls @p callback with @p w->syntherr as the error code (which might be 0).
 */
static int writer_shutdown(ev_source *ev,
			   const attribute((unused)) struct timeval *now,
			   void *u) {
  ev_writer *w = u;

  ev_timeout_cancel(ev, w->timeout);
  w->timeout = 0;
  return w->callback(ev, w->fd, w->syntherror, w->u);
}

/** @brief Called when a writer's @p timebound expires */
static int writer_timebound_exceeded(ev_source *ev,
				     const struct timeval attribute((unused)) *now,
				     void *u) {
  ev_writer *const w = u;

  error(0, "abandoning writer %s because no writes within %ds",
	w->what, w->timebound);
  return w->callback(ev, w->fd, ETIMEDOUT, w->u);
}

/** @brief Set the time bound callback (if not set already) */
static void writer_set_timebound(ev_writer *w) {
  if(w->timebound && !w->timeout) {
    struct timeval when;
    ev_source *const ev = w->ev;
    
    xgettimeofday(&when, 0);
    when.tv_sec += w->timebound;
    ev_timeout(ev, &w->timeout, &when, writer_timebound_exceeded, w);
  }
}

/** @brief Called when a writer's file descriptor is writable */
static int writer_callback(ev_source *ev, int fd, void *u) {
  ev_writer *const w = u;
  int n;

  n = write(fd, w->b.start, w->b.end - w->b.start);
  D(("callback for writer fd %d, %ld bytes, n=%d, errno=%d",
     fd, (long)(w->b.end - w->b.start), n, errno));
  if(n >= 0) {
    w->b.start += n;
    ev_timeout_cancel(ev, w->timeout);
    w->timeout = 0;
    if(w->b.start == w->b.end) {
      if(w->eof) {
	ev_fd_cancel(ev, ev_write, fd);
	return w->callback(ev, fd, 0, w->u);
      } else
	ev_fd_disable(ev, ev_write, fd);
    } else
      writer_set_timebound(w);
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

/** @brief Write bytes to a writer's buffer
 *
 * This is the sink write callback.
 *
 * Calls ev_fd_enable() if necessary (i.e. if the buffer was empty but
 * now is not).
 */
static int ev_writer_write(struct sink *sk, const void *s, int n) {
  ev_writer *w = (ev_writer *)sk;

  if(!n)
    return 0;				/* avoid silliness */
  buffer_space(&w->b, n);
  if(w->b.start == w->b.end)
    ev_fd_enable(w->ev, ev_write, w->fd);
  memcpy(w->b.end, s, n);
  w->b.end += n;
  if(w->spacebound && w->b.end - w->b.start > w->spacebound) {
    /* Buffer contents have exceeded the space bound.  We assume that the
     * remote client has gone away and TCP hasn't noticed yet, or that it's got
     * hopelessly stuck. */
    error(0, "abandoning writer %s because buffer has reached %td bytes",
	  w->what, w->b.end - w->b.start);
    w->syntherror = EPIPE;
    ev_fd_cancel(w->ev, ev_write, w->fd);
    return ev_timeout(w->ev, 0, 0, writer_shutdown, w);
  }
  writer_set_timebound(w);
  return 0;
}

/** @brief Create a new buffered writer
 * @param ev Event loop
 * @param fd File descriptor to write to
 * @param callback Called if an error occurs and when finished
 * @param u Passed to @p callback
 * @param what Text description
 * @return New writer or @c NULL
 */ 
ev_writer *ev_writer_new(ev_source *ev,
			 int fd,
			 ev_error_callback *callback,
			 void *u,
			 const char *what) {
  ev_writer *w = xmalloc(sizeof *w);

  D(("registering writer fd %d callback %p %p", fd, (void *)callback, u));
  w->s.write = ev_writer_write;
  w->fd = fd;
  w->callback = callback;
  w->u = u;
  w->ev = ev;
  w->timebound = 10 * 60;
  w->spacebound = 512 * 1024;
  w->what = what;
  if(ev_fd(ev, ev_write, fd, writer_callback, w, what))
    return 0;
  ev_fd_disable(ev, ev_write, fd);
  return w;
}

/** @brief Get/set the time bound
 * @param w Writer
 * @param new_time_bound New bound or -1 for no change
 * @return Latest time bound
 *
 * If @p new_time_bound is negative then the current time bound is returned.
 * Otherwise it is set and the new value returned.
 *
 * The time bound is the number of seconds allowed between writes.  If it takes
 * longer than this to flush a buffer then the peer will be assumed to be dead
 * and an error will be synthesized.  0 means "don't care".  The default time
 * bound is 10 minutes.
 *
 * Note that this value does not take into account kernel buffering and
 * timeouts.
 */
int ev_writer_time_bound(ev_writer *w,
			 int new_time_bound) {
  if(new_time_bound >= 0)
    w->timebound = new_time_bound;
  return w->timebound;
}

/** @brief Get/set the space bound
 * @param w Writer
 * @param new_space_bound New bound or -1 for no change
 * @return Latest space bound
 *
 * If @p new_space_bound is negative then the current space bound is returned.
 * Otherwise it is set and the new value returned.
 *
 * The space bound is the number of bytes allowed between in the buffer.  If
 * the buffer exceeds this size an error will be synthesized.  0 means "don't
 * care".  The default space bound is 512Kbyte.
 *
 * Note that this value does not take into account kernel buffering.
 */
int ev_writer_space_bound(ev_writer *w,
			  int new_space_bound) {
  if(new_space_bound >= 0)
    w->spacebound = new_space_bound;
  return w->spacebound;
}

/** @brief Return the sink associated with a writer
 * @param w Writer
 * @return Pointer to sink
 *
 * Writing to the sink will arrange for those bytes to be written to the file
 * descriptor as and when it is writable.
 */
struct sink *ev_writer_sink(ev_writer *w) {
  if(!w)
    fatal(0, "ev_write_sink called with null writer");
  return &w->s;
}

/** @brief Close a writer
 * @param w Writer to close
 * @return 0 on success, non-0 on error
 *
 * Close a writer.  No more bytes should be written to its sink.
 *
 * When the last byte has been written the callback will be called with an
 * error code of 0.  It is guaranteed that this will NOT happen before
 * ev_writer_close() returns (although the file descriptor for the writer might
 * be cancelled by the time it returns).
 */
int ev_writer_close(ev_writer *w) {
  D(("close writer fd %d", w->fd));
  w->eof = 1;
  if(w->b.start == w->b.end) {
    /* we're already finished */
    w->syntherror = 0;			/* no error */
    ev_fd_cancel(w->ev, ev_write, w->fd);
    return ev_timeout(w->ev, 0, 0, writer_shutdown, w);
  }
  return 0;
}

/** @brief Cancel a writer discarding any buffered data
 * @param w Writer to close
 * @return 0 on success, non-0 on error
 *
 * This cancels a writer immediately.  Any unwritten buffered data is discarded
 * and the error callback is never called.  This is appropriate to call if (for
 * instance) the read half of a TCP connection is known to have failed and the
 * writer is therefore obsolete.
 */
int ev_writer_cancel(ev_writer *w) {
  ev_source *const ev = w->ev;
  D(("cancel writer fd %d", w->fd));
  ev_timeout_cancel(ev, w->timeout);
  w->timeout = 0;
  return ev_fd_cancel(w->ev, ev_write, w->fd);
}

/** @brief Attempt to flush a writer
 * @param w Writer to flush
 * @return 0 on success, non-0 on error
 *
 * Does a speculative write of any buffered data.  Does not block if it cannot
 * be written.
 */
int ev_writer_flush(ev_writer *w) {
  return writer_callback(w->ev, w->fd, w);
}

/* buffered reader ************************************************************/

/** @brief State structure for a buffered reader */
struct ev_reader {
  struct buffer b;
  int fd;
  ev_reader_callback *callback;
  ev_error_callback *error_callback;
  void *u;
  ev_source *ev;
  int eof;
};

/** @brief Called when a reader's @p fd is readable */
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

/** @brief Create a new buffered reader
 * @param ev Event loop
 * @param fd File descriptor to read from
 * @param callback Called when new data is available
 * @param error_callback Called if an error occurs
 * @param u Passed to callbacks
 * @param what Text description
 * @return New reader or @c NULL
 */
ev_reader *ev_reader_new(ev_source *ev,
			 int fd,
			 ev_reader_callback *callback,
			 ev_error_callback *error_callback,
			 void *u,
			 const char *what) {
  ev_reader *r = xmalloc(sizeof *r);

  D(("registering reader fd %d callback %p %p %p",
     fd, (void *)callback, (void *)error_callback, u));
  r->fd = fd;
  r->callback = callback;
  r->error_callback = error_callback;
  r->u = u;
  r->ev = ev;
  if(ev_fd(ev, ev_read, fd, reader_callback, r, what))
    return 0;
  return r;
}

void ev_reader_buffer(ev_reader *r, size_t nbytes) {
  buffer_space(&r->b, nbytes - (r->b.end - r->b.start));
}

/** @brief Consume @p n bytes from the reader's buffer
 * @param r Reader
 * @param n Number of bytes to consume
 *
 * Tells the reader than the next @p n bytes have been dealt with and can now
 * be discarded.
 */
void ev_reader_consume(ev_reader *r, size_t n) {
  r->b.start += n;
}

/** @brief Cancel a reader
 * @param r Reader
 * @return 0 on success, non-0 on error
 */
int ev_reader_cancel(ev_reader *r) {
  D(("cancel reader fd %d", r->fd));
  return ev_fd_cancel(r->ev, ev_read, r->fd);
}

/** @brief Temporarily disable a reader
 * @param r Reader
 * @return 0 on success, non-0 on error
 *
 * No further callbacks for this reader will be made.  Re-enable with
 * ev_reader_enable().
 */
int ev_reader_disable(ev_reader *r) {
  D(("disable reader fd %d", r->fd));
  return r->eof ? 0 : ev_fd_disable(r->ev, ev_read, r->fd);
}

/** @brief Called from ev_run() for ev_reader_incomplete() */
static int reader_continuation(ev_source attribute((unused)) *ev,
			       const attribute((unused)) struct timeval *now,
			       void *u) {
  ev_reader *r = u;

  D(("reader continuation callback fd %d", r->fd));
  if(ev_fd_enable(r->ev, ev_read, r->fd)) return -1;
  return r->callback(ev, r, r->fd, r->b.start, r->b.end - r->b.start, r->eof, r->u);
}

/** @brief Arrange another callback
 * @param r reader
 * @return 0 on success, non-0 on error
 *
 * Indicates that the reader can process more input but would like to yield to
 * other clients of the event loop.  Input will be disabled but it will be
 * re-enabled on the next iteration of the event loop and the read callback
 * will be called again (even if no further bytes are available).
 */
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

/** @brief Re-enable reading
 * @param r reader
 * @return 0 on success, non-0 on error
 *
 * If there is unconsumed data then you get a callback next time round the
 * event loop even if nothing new has been read.
 *
 * The idea is in your read callback you come across a line (or whatever) that
 * can't be processed immediately.  So you set up processing and disable
 * reading with ev_reader_disable().  Later when you finish processing you
 * re-enable.  You'll automatically get another callback directly from the
 * event loop (i.e. not from inside ev_reader_enable()) so you can handle the
 * next line (or whatever) if the whole thing has in fact already arrived.
 */
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
