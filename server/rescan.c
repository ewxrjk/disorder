/*
 * This file is part of DisOrder 
 * Copyright (C) 2005, 2006 Richard Kettlewell
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <db.h>
#include <locale.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pcre.h>
#include <fnmatch.h>
#include <sys/wait.h>
#include <string.h>
#include <syslog.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "plugin.h"
#include "inputline.h"
#include "charset.h"
#include "wstat.h"
#include "kvp.h"
#include "printf.h"
#include "trackdb.h"
#include "trackdb-int.h"

static DB_TXN *global_tid;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-rescan [OPTIONS] [PATH...]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "\n"
          "Rescanner for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder-rescan version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

static volatile sig_atomic_t signalled;

static void signal_handler(int sig) {
  if(sig == 0) _exit(-1);               /* "Cannot happen" */
  signalled = sig;
}

static int aborted(void) {
  return signalled || getppid() == 1;
}

/* Exit if our parent has gone away or we have been told to stop. */
static void checkabort(void) {
  if(getppid() == 1) {
    info("parent has terminated");
    trackdb_abort_transaction(global_tid);
    exit(0);
  }
  if(signalled) {
    info("received signal %d", signalled);
    trackdb_abort_transaction(global_tid);
    exit(0);
  }
}

/* rescan a collection */
static void rescan_collection(const struct collection *c) {
  pid_t pid, r;
  int p[2], n, w;
  FILE *fp = 0;
  char *path, *track;
  long ntracks = 0, nnew = 0;
  
  checkabort();
  info("rescanning %s with %s", c->root, c->module);
  /* plugin runs in a subprocess */
  xpipe(p);
  if(!(pid = xfork())) {
    exitfn = _exit;
    xclose(p[0]);
    xdup2(p[1], 1);
    xclose(p[1]);
    scan(c->module, c->root);
    if(fflush(stdout) < 0)
      fatal(errno, "error writing to scanner pipe");
    _exit(0);
  }
  xclose(p[1]);
  if(!(fp = fdopen(p[0], "r")))
    fatal(errno, "error calling fdopen");
  /* read tracks from the plugin */
  while(!inputline("rescanner", fp, &path, 0)) {
    checkabort();
    /* actually we can cope relatively well within the server, but they'll go
     * wrong in track listings */
    if(strchr(path, '\n')) {
      error(0, "cannot cope with tracks with newlines in the name");
      continue;
    }
    if(!(track = any2utf8(c->encoding, path))) {
      error(0, "cannot convert track path to UTF-8: %s", path);
      continue;
    }
    D(("track %s", track));
    /* only tracks with a known player are admitted */
    for(n = 0; (n < config->player.n
		&& fnmatch(config->player.s[n].s[0], track, 0) != 0); ++n)
      ;
    if(n < config->player.n) {
      nnew += !!trackdb_notice(track, path);
      ++ntracks;
    }
  }
  /* tidy up */
  if(ferror(fp)) {
    error(errno, "error reading from scanner pipe");
    goto done;
  }
  xfclose(fp);
  fp = 0;
  while((r = waitpid(pid, &w, 0)) == -1 && errno == EINTR)
    ;
  if(r < 0) fatal(errno, "error calling waitpid");
  pid = 0;
  if(w) {
    error(0, "scanner subprocess: %s", wstat(w));
    goto done;
  }
  info("rescanned %s, %ld tracks, %ld new", c->root, ntracks, nnew);
done:
  if(fp)
    xfclose(fp);
  if(pid)
    while((r = waitpid(pid, &w, 0)) == -1 && errno == EINTR)
      ;
}

struct recheck_state {
  const struct collection *c;
  long nobsolete, nlength;
};

/* called for each non-alias track */
static int recheck_callback(const char *track,
                            struct kvp *data,
                            void *u,
                            DB_TXN *tid) {
  struct recheck_state *cs = u;
  const struct collection *c = cs->c;
  const char *path = kvp_get(data, "_path");
  char buffer[20];
  int err;
  long n;

  if(aborted()) return EINTR;
  D(("rechecking %s", track));
  /* see if the track has evaporated */
  if(check(c->module, c->root, path) == 0) {
    D(("obsoleting %s", track));
    if((err = trackdb_obsolete(track, tid))) return err;
    ++cs->nobsolete;
    return 0;
  }
  /* make sure we know the length */
  if(!kvp_get(data, "_length")) {
    D(("recalculating length of %s", track));
    n = tracklength(track, path);
    if(n > 0) {
      byte_snprintf(buffer, sizeof buffer, "%ld", n);
      kvp_set(&data, "_length", buffer);
      if((err = trackdb_putdata(trackdb_tracksdb, track, data, tid, 0)))
        return err;
      ++cs->nlength;
    }
  }
  return 0;
}

/* recheck a collection */
static void recheck_collection(const struct collection *c) {
  struct recheck_state cs;

  info("rechecking %s", c->root);
  for(;;) {
    checkabort();
    global_tid = trackdb_begin_transaction();
    memset(&cs, 0, sizeof cs);
    cs.c = c;
    if(trackdb_scan(c->root, recheck_callback, &cs, global_tid)) goto fail;
    break;
  fail:
    /* Maybe we need to shut down */
    checkabort();
    /* Abort the transaction and try again in a bit. */
    trackdb_abort_transaction(global_tid);
    global_tid = 0;
    /* Let anything else that is going on get out of the way. */
    sleep(10);
    checkabort();
    info("resuming recheck of %s", c->root);
  }
  trackdb_commit_transaction(global_tid);
  global_tid = 0;
  info("rechecked %s, %ld obsoleted, %ld lengths calculated",
       c->root, cs.nobsolete, cs.nlength);
}

/* rescan/recheck a collection by name */
static void do_directory(const char *s,
			 void (*fn)(const struct collection *c)) {
  int n;
  
  for(n = 0; (n < config->collection.n
	      && strcmp(config->collection.s[n].root, s)); ++n)
    ;
  if(n < config->collection.n)
    fn(&config->collection.s[n]);
  else
    error(0, "no collection has root '%s'", s);
}

/* rescan/recheck all collections */
static void do_all(void (*fn)(const struct collection *c)) {
  int n;

  for(n = 0; n < config->collection.n; ++n)
    fn(&config->collection.s[n]);
}

int main(int argc, char **argv) {
  int n;
  struct sigaction sa;
  
  set_progname(argv);
  mem_init(1);
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dD", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    default: fatal(0, "invalid option");
    }
  }
  /* If stderr is a TTY then log there, otherwise to syslog. */
  if(!isatty(2)) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read()) fatal(0, "cannot read configuration");
  xnice(config->nice_rescan);
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  xsigaction(SIGTERM, &sa, 0);
  xsigaction(SIGINT, &sa, 0);
  info("started");
  trackdb_init(0);
  trackdb_open();
  if(optind == argc) {
    do_all(rescan_collection);
    do_all(recheck_collection);
  }
  else {
    for(n = optind; n < argc; ++n)
      do_directory(argv[n], rescan_collection);
    for(n = optind; n < argc; ++n)
      do_directory(argv[n], recheck_collection);
  }
  trackdb_close();
  trackdb_deinit();
  info("completed");
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
/* arch-tag:yBYnk5wX3mDtVI1MI3LlWg */
