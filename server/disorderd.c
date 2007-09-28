/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#include <stdio.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <time.h>
#include <locale.h>
#include <syslog.h>
#include <sys/time.h>
#include <pcre.h>
#include <fcntl.h>

#include "daemonize.h"
#include "event.h"
#include "log.h"
#include "configuration.h"
#include "trackdb.h"
#include "queue.h"
#include "mem.h"
#include "play.h"
#include "server.h"
#include "state.h"
#include "syscalls.h"
#include "defs.h"
#include "user.h"
#include "mixer.h"
#include "eventlog.h"
#include "printf.h"

static ev_source *ev;

static void rescan_after(long offset);
static void dbgc_after(long offset);
static void volumecheck_after(long offset);

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "foreground", no_argument, 0, 'f' },
  { "log", required_argument, 0, 'l' },
  { "pidfile", required_argument, 0, 'P' },
  { "no-initial-rescan", no_argument, 0, 'N' },
  { "wide-open", no_argument, 0, 'w' },
  { "syslog", no_argument, 0, 's' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorderd [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h               Display usage message\n"
	  "  --version, -V            Display version number\n"
	  "  --config PATH, -c PATH   Set configuration file\n"
	  "  --debug, -d              Turn on debugging\n"
	  "  --foreground, -f         Do not become a daemon\n"
	  "  --syslog, -s             Log to syslog even with -f\n"
	  "  --pidfile PATH, -P PATH  Leave a pidfile\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorderd version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

/* SIGHUP callback */
static int handle_sighup(ev_source attribute((unused)) *ev_,
			 int attribute((unused)) sig,
			 void attribute((unused)) *u) {
  info("received SIGHUP");
  reconfigure(ev, 1);
  return 0;
}

/* fatal signals */

static int handle_sigint(ev_source attribute((unused)) *ev_,
			 int attribute((unused)) sig,
			 void attribute((unused)) *u) {
  info("received SIGINT");
  quit(ev);
}

static int handle_sigterm(ev_source attribute((unused)) *ev_,
			  int attribute((unused)) sig,
			  void attribute((unused)) *u) {
  info("received SIGTERM");
  quit(ev);
}

static int rescan_again(ev_source *ev_,
			const struct timeval attribute((unused)) *now,
			void attribute((unused)) *u) {
  trackdb_rescan(ev_);
  rescan_after(86400);
  return 0;
}

static void rescan_after(long offset) {
  struct timeval w;

  gettimeofday(&w, 0);
  w.tv_sec += offset;
  ev_timeout(ev, 0, &w, rescan_again, 0);
}

static int dbgc_again(ev_source attribute((unused)) *ev_,
		      const struct timeval attribute((unused)) *now,
		      void attribute((unused)) *u) {
  trackdb_gc();
  dbgc_after(60);
  return 0;
}

static void dbgc_after(long offset) {
  struct timeval w;

  gettimeofday(&w, 0);
  w.tv_sec += offset;
  ev_timeout(ev, 0, &w, dbgc_again, 0);
}

static int volumecheck_again(ev_source attribute((unused)) *ev_,
			     const struct timeval attribute((unused)) *now,
			     void attribute((unused)) *u) {
  int l, r;
  char lb[32], rb[32];

  if(!mixer_control(&l, &r, 0)) {
    if(l != volume_left || r != volume_right) {
      volume_left = l;
      volume_right = r;
      snprintf(lb, sizeof lb, "%d", l);
      snprintf(rb, sizeof rb, "%d", r);
      eventlog("volume", lb, rb, (char *)0);
    }
  }
  volumecheck_after(60);
  return 0;
}

static void volumecheck_after(long offset) {
  struct timeval w;

  gettimeofday(&w, 0);
  w.tv_sec += offset;
  ev_timeout(ev, 0, &w, volumecheck_again, 0);
}

/* We fix the path to include the bindir and sbindir we were installed into */
static void fix_path(void) {
  char *path = getenv("PATH");
  char *newpath;

  if(!path)
    error(0, "PATH is not set at all!");

  if(*finkbindir)
    /* We appear to be a finkized mac; include fink on the path in case the
     * tools we need are there. */
    byte_xasprintf(&newpath, "PATH=%s:%s:%s:%s", 
		   path, bindir, sbindir, finkbindir);
  else
    byte_xasprintf(&newpath, "PATH=%s:%s:%s", path, bindir, sbindir);
  putenv(newpath);
  info("%s", newpath); 
}

int main(int argc, char **argv) {
  int n, background = 1, logsyslog = 0;
  const char *pidfile = 0;
  int initial_rescan = 1;

  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
  while((n = getopt_long(argc, argv, "hVc:dfP:Ns", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'f': background = 0; break;
    case 'P': pidfile = optarg; break;
    case 'N': initial_rescan = 0; break;
    case 's': logsyslog = 1; break;
    case 'w': wideopen = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  /* go into background if necessary */
  if(background)
    daemonize(progname, LOG_DAEMON, pidfile);
  else if(logsyslog) {
    /* If we're running under some kind of daemon supervisor then we may want
     * to log to syslog but not to go into background */
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  info("process ID %lu", (unsigned long)getpid());
  fix_path();
  srand(time(0));			/* don't start the same every time */
  /* create event loop */
  ev = ev_new();
  if(ev_child_setup(ev)) fatal(0, "ev_child_setup failed");
  /* read config */
  if(config_read())
    fatal(0, "cannot read configuration");
  /* Start the speaker process (as root! - so it can choose its nice value) */
  speaker_setup(ev);
  /* set server nice value _after_ starting the speaker, so that they
   * are independently niceable */
  xnice(config->nice_server);
  /* change user */
  become_mortal();
  /* make sure we're not root, whatever the config says */
  if(getuid() == 0 || geteuid() == 0) fatal(0, "do not run as root");
  /* open a lockfile - we only want one copy of the server to run at once. */
  if(config->lock) {
    const char *lockfile;
    int lockfd;
   struct flock lock;
    
    lockfile = config_get_file("lock");
    if((lockfd = open(lockfile, O_RDWR|O_CREAT, 0600)) < 0)
      fatal(errno, "error opening %s", lockfile);
    cloexec(lockfd);
    memset(&lock, 0, sizeof lock);
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if(fcntl(lockfd, F_SETLK, &lock) < 0)
      fatal(errno, "error locking %s", lockfile);
  }
  /* initialize database environment */
  trackdb_init(TRACKDB_NORMAL_RECOVER);
  trackdb_master(ev);
  /* install new config */
  reconfigure(ev, 0);
  /* re-read config if we receive a SIGHUP */
  if(ev_signal(ev, SIGHUP, handle_sighup, 0)) fatal(0, "ev_signal failed");
  /* exit on SIGINT/SIGTERM */
  if(ev_signal(ev, SIGINT, handle_sigint, 0)) fatal(0, "ev_signal failed");
  if(ev_signal(ev, SIGTERM, handle_sigterm, 0)) fatal(0, "ev_signal failed");
  /* ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
  /* start a rescan straight away */
  if(initial_rescan)
    trackdb_rescan(ev);
  rescan_after(86400);
  /* periodically tidy up the database */
  dbgc_after(60);
  /* periodically check the volume */
  volumecheck_after(60);
  /* set initial state */
  add_random_track();
  play(ev);
  /* enter the event loop */
  n = ev_run(ev);
  /* if we exit the event loop, something must have gone wrong */
  fatal(errno, "ev_run returned %d", n);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
