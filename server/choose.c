/*
 * This file is part of DisOrder 
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file choose.c
 * @brief Random track chooser
 *
 * Picks a track at random and writes it to standard output.  If for
 * any reason no track can be picked - even a trivial reason like a
 * deadlock - it just exits and expects the server to try again.
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
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>

#include "configuration.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "kvp.h"
#include "syscalls.h"
#include "printf.h"
#include "trackdb.h"
#include "trackdb-int.h"
#include "version.h"
#include "trackname.h"

static DB_TXN *global_tid;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { "syslog", no_argument, 0, 's' },
  { "no-syslog", no_argument, 0, 'S' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-choose [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Enable/disable logging to syslog\n"
          "\n"
          "Track choose for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/** @brief Weighted track record */
struct weighted_track {
  /** @brief Next track in the list */
  struct weighted_track *next;
  /** @brief Track name */
  const char *track;
  /** @brief Weight for this track (always positive) */
  unsigned long weight;
};

/** @brief List of tracks with nonzero weight */
static struct weighted_track *tracks;

/** @brief Sum of all weights */
static unsigned long long total_weight;

/** @brief Count of tracks */
static long ntracks;

static char **required_tags;
static char **prohibited_tags;

/** @brief Compute the weight of a track
 * @param track Track name (UTF-8)
 * @param data Track data
 * @param prefs Track preferences
 * @return Track weight (non-negative)
 *
 * Tracks to be excluded entirely are given a weight of 0.
 */
static unsigned long compute_weight(const char *track,
                                    struct kvp *data,
                                    struct kvp *prefs) {
  const char *s;
  char **track_tags;
  time_t last, now;

  /* Reject tracks not in any collection (race between edit config and
   * rescan) */
  if(!find_track_root(track)) {
    info("found track not in any collection: %s", track);
    return 0;
  }

  /* Reject aliases to avoid giving aliased tracks extra weight */
  if(kvp_get(data, "_alias_for"))
    return 0;
  
  /* Reject tracks with random play disabled */
  if((s = kvp_get(prefs, "pick_at_random"))
     && !strcmp(s, "0"))
    return 0;

  /* Reject tracks played within the last 8 hours */
  if((s = kvp_get(prefs, "played_time"))) {
    last = atoll(s);
    now = time(0);
    if(now < last + 8 * 3600)       /* TODO configurable */
      return 0;
  }

  /* We'll need tags for a number of things */
  track_tags = parsetags(kvp_get(prefs, "tags"));

  /* Reject tracks with prohibited tags */
  if(prohibited_tags && tag_intersection(track_tags, prohibited_tags))
    return 0;

  /* Reject tracks that lack required tags */
  if(*required_tags && !tag_intersection(track_tags, required_tags))
    return 0;

  return 90000;
}

/** @brief Called for each track */
static int collect_tracks_callback(const char *track,
				   struct kvp *data,
                                   struct kvp *prefs,
				   void attribute((unused)) *u,
				   DB_TXN attribute((unused)) *tid) {
  const unsigned long weight = compute_weight(track, data, prefs);

  if(weight) {
    struct weighted_track *const t = xmalloc(sizeof *t);

    t->next = tracks;
    t->track = track;
    t->weight = weight;
    tracks = t;
    total_weight += weight;
    ++ntracks;
  }
  return 0;
}

/** @brief Pick a random integer uniformly from [0, limit) */
static unsigned long long pick_weight(unsigned long long limit) {
  unsigned long long n;
  static int fd = -1;
  int r;

  if(fd < 0) {
    if((fd = open("/dev/urandom", O_RDONLY)) < 0)
      fatal(errno, "opening /dev/urandom");
  }
  if((r = read(fd, &n, sizeof n)) < 0)
    fatal(errno, "reading /dev/urandom");
  if((size_t)r < sizeof n)
    fatal(0, "short read from /dev/urandom");
  return n % limit;
}

/** @brief Pick a track at random and write it to stdout */
static void pick_track(void) {
  long long w;
  struct weighted_track *t;

  w = pick_weight(total_weight);
  t = tracks;
  while(t && w >= t->weight) {
    w -= t->weight;
    t = t->next;
  }
  if(!t)
    fatal(0, "ran out of tracks but %lld weighting left", w);
  xprintf("%s", t->track);
}

int main(int argc, char **argv) {
  int n, logsyslog = !isatty(2), err;
  const char *tags;
  
  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-choose");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read(0)) fatal(0, "cannot read configuration");
  /* Generate the candidate track list */
  trackdb_init(TRACKDB_NO_RECOVER);
  trackdb_open(TRACKDB_NO_UPGRADE|TRACKDB_READ_ONLY);
  global_tid = trackdb_begin_transaction();
  if((err = trackdb_get_global_tid("required-tags", global_tid, &tags)))
    fatal(0, "error getting required-tags: %s", db_strerror(err));
  required_tags = parsetags(tags);
  if((err = trackdb_get_global_tid("prohibited-tags", global_tid, &tags)))
    fatal(0, "error getting prohibited-tags: %s", db_strerror(err));
  prohibited_tags = parsetags(tags);
  if(trackdb_scan(0, collect_tracks_callback, 0, global_tid))
    exit(1);
  trackdb_commit_transaction(global_tid);
  trackdb_close();
  trackdb_deinit();
  //info("ntracks=%ld total_weight=%lld", ntracks, total_weight);
  if(!total_weight)
    fatal(0, "no tracks match random choice criteria");
  /* Pick a track */
  pick_track();
  xfclose(stdout);
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
