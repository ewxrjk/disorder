/*
 * This file is part of DisOrder 
 * Copyright (C) 2005-2008 Richard Kettlewell
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

#include "disorder-server.h"

static DB_TXN *global_tid;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { "syslog", no_argument, 0, 's' },
  { "no-syslog", no_argument, 0, 'S' },
  { "check", no_argument, 0, 'K' },
  { "no-check", no_argument, 0, 'C' },
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
          "  --[no-]syslog           Enable/disable logging to syslog\n"
          "  --[no-]check            Enable/disable track length check\n"
          "\n"
          "Rescanner for DisOrder.  Not intended to be run\n"
          "directly.\n");
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
    if(config->dbversion > 1) {
      /* We use NFC track names */
      if(!(track = utf8_compose_canon(track, strlen(track), 0))) {
        error(0, "cannot convert track path to NFC: %s", path);
        continue;
      }
    }
    D(("track %s", track));
    /* only tracks with a known player are admitted */
    for(n = 0; (n < config->player.n
		&& fnmatch(config->player.s[n].s[0], track, 0) != 0); ++n)
      ;
    if(n < config->player.n) {
      nnew += !!trackdb_notice(track, path);
      ++ntracks;
      if(ntracks % 1000 == 0)
        info("rescanning %s, %ld tracks so far", c->root, ntracks);
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
  long nobsolete, nnocollection, nlength;
  struct recheck_track *tracks;
};

struct recheck_track {
  struct recheck_track *next;
  const char *track;
};

/* called for each non-alias track */
static int recheck_list_callback(const char *track,
                                 struct kvp attribute((unused)) *data,
                                 struct kvp attribute((unused)) *prefs,
                                 void *u,
                                 DB_TXN attribute((unused)) *tid) {
  struct recheck_state *cs = u;
  struct recheck_track *t = xmalloc(sizeof *t);

  t->next = cs->tracks;
  t->track = track;
  cs->tracks = t;
  return 0;
}

static int recheck_track_tid(struct recheck_state *cs,
                             const struct recheck_track *t,
                             DB_TXN *tid) {
  const struct collection *c = cs->c;
  const char *path;
  char buffer[20];
  int err, n;
  long length;
  struct kvp *data;

  if((err = trackdb_getdata(trackdb_tracksdb, t->track, &data, tid)))
    return err;
  path = kvp_get(data, "_path");
  D(("rechecking %s", t->track));
  /* if we're not checking a specific collection, find the right collection */
  if(!c) {
    if(!(c = find_track_collection(t->track))) {
      D(("obsoleting %s", t->track));
      if((err = trackdb_obsolete(t->track, tid)))
        return err;
      ++cs->nnocollection;
      return 0;
    }
  }
  /* see if the track has evaporated */
  if(check(c->module, c->root, path) == 0) {
    D(("obsoleting %s", t->track));
    if((err = trackdb_obsolete(t->track, tid)))
      return err;
    ++cs->nobsolete;
    return 0;
  }
  /* make sure we know the length */
  if(!kvp_get(data, "_length")) {
    D(("recalculating length of %s", t->track));
    for(n = 0; n < config->tracklength.n; ++n)
      if(fnmatch(config->tracklength.s[n].s[0], t->track, 0) == 0)
        break;
    if(n >= config->tracklength.n)
      error(0, "no tracklength plugin found for %s", t->track);
    else {
      length = tracklength(config->tracklength.s[n].s[1], t->track, path);
      if(length > 0) {
        byte_snprintf(buffer, sizeof buffer, "%ld", length);
        kvp_set(&data, "_length", buffer);
        if((err = trackdb_putdata(trackdb_tracksdb, t->track, data, tid, 0)))
          return err;
        ++cs->nlength;
      }
    }
  }
  return 0;
}

static int recheck_track(struct recheck_state *cs,
                         const struct recheck_track *t) {
  int e;

  WITH_TRANSACTION(recheck_track_tid(cs, t, tid));
  return e;
}

/* recheck a collection */
static void recheck_collection(const struct collection *c) {
  struct recheck_state cs;
  const struct recheck_track *t;
  long nrc;

  if(c)
    info("rechecking %s", c->root);
  else
    info("rechecking all tracks");
  /* Doing the checking inside a transaction locks up the server for much too
   * long (because it spends lots of time thinking about each track).  So we
   * pull the full track list into memory and work from that.
   *
   * 100,000 tracks at, say, 80 bytes per track name, gives 8MB, which is quite
   * reasonable.
   */
  for(;;) {
    checkabort();
    info("getting track list");
    global_tid = trackdb_begin_transaction();
    memset(&cs, 0, sizeof cs);
    cs.c = c;
    if(trackdb_scan(c ? c->root : 0, recheck_list_callback, &cs, global_tid))
      goto fail;
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
    if(c)
      info("resuming recheck of %s", c->root);
    else
      info("resuming global recheck");
  }
  trackdb_commit_transaction(global_tid);
  global_tid = 0;
  nrc = 0;
  for(t = cs.tracks; t; t = t->next) {
    if(aborted())
      return;
    recheck_track(&cs, t);
    ++nrc;
    if(nrc % 100 == 0) {
      if(c)
        info("rechecking %s, %ld tracks so far", c->root, nrc);
      else
        info("rechecking all tracks, %ld tracks so far", nrc);
    }
  }
  if(c)
    info("rechecked %s, %ld obsoleted, %ld lengths calculated",
         c->root, cs.nobsolete, cs.nlength);
  else
    info("rechecked all tracks, %ld no collection, %ld obsoleted, %ld lengths calculated",
         cs.nnocollection, cs.nobsolete, cs.nlength);
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
  /* TODO: we need to tidy up tracks from collections now removed.  We could do
   * this two ways: either remember collections we think there are and spot
   * their disappearance, or iterate over all tracks and gc any that don't fit
   * into some collection.
   *
   * Having a way to rename collections would be rather convenient too but
   * that's another kettle of monkeys.
   */
}

/** @brief Expire noticed.db */
static void expire_noticed(void) {
  time_t now;

  time(&now);
  trackdb_expire_noticed(now - config->noticed_history * 86400);
}

int main(int argc, char **argv) {
  int n, logsyslog = !isatty(2);
  struct sigaction sa;
  int do_check = 1;
  
  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSsKC", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-rescan");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    case 'K': do_check = 1; break;
    case 'C': do_check = 0; break;
    default: fatal(0, "invalid option");
    }
  }
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read(0)) fatal(0, "cannot read configuration");
  xnice(config->nice_rescan);
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  xsigaction(SIGTERM, &sa, 0);
  xsigaction(SIGINT, &sa, 0);
  info("started");
  trackdb_init(TRACKDB_NO_RECOVER);
  trackdb_open(TRACKDB_NO_UPGRADE);
  if(optind == argc) {
    /* Rescan all collections */
    do_all(rescan_collection);
    /* Check that every track still exists */
    if(do_check)
      recheck_collection(0);
    /* Expire noticed.db */
    expire_noticed();
  }
  else {
    /* Rescan specified collections */
    for(n = optind; n < argc; ++n)
      do_directory(argv[n], rescan_collection);
    /* Check specified collections for tracks that have gone */
    if(do_check)
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
