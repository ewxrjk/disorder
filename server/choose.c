/*
 * This file is part of DisOrder 
 * Copyright (C) 2008, 2009, 2011 Richard Kettlewell
 * Copyright (C) 2008 Mark Wooding
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
/** @file server/choose.c
 * @brief Random track chooser
 *
 * Picks a track at random and writes it to standard output.  If for
 * any reason no track can be picked - even a trivial reason like a
 * deadlock - it just exits and expects the server to try again.
 */

#include "disorder-server.h"

#define BASE_WEIGHT 90000

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
static void attribute((noreturn)) help(void) {
  xprintf("Usage:\n"
	  "  disorder-choose [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Enable/disable logging to syslog\n"
          "\n"
          "Track chooser for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}
/** @brief Sum of all weights */
static unsigned long long total_weight;

/** @brief The winning track */
static const char *winning = 0;

/** @brief Count of tracks */
static long ntracks;

static char **required_tags;
static char **prohibited_tags;

static int queue_contains(const struct queue_entry *head,
                          const char *track) {
  const struct queue_entry *q;

  for(q = head->next; q != head; q = q->next)
    if(!strcmp(q->track, track))
      return 1;
  return 0;
}

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
  time_t last, now = xtime(0);

  /* Reject tracks not in any collection (race between edit config and
   * rescan) */
  if(!find_track_root(track)) {
    disorder_info("found track not in any collection: %s", track);
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
    if(now < last + config->replay_min)
      return 0;
  }

  /* Reject tracks currently in the queue or in the recent list */
  if(queue_contains(&qhead, track)
     || queue_contains(&phead, track))
    return 0;

  /* We'll need tags for a number of things */
  track_tags = parsetags(kvp_get(prefs, "tags"));

  /* Reject tracks with prohibited tags */
  if(prohibited_tags && tag_intersection(track_tags, prohibited_tags))
    return 0;

  /* Reject tracks that lack required tags */
  if(*required_tags && !tag_intersection(track_tags, required_tags))
    return 0;

  /* Use the configured weight if available */
  if((s = kvp_get(prefs, "weight"))) {
    long n;
    errno = 0;

    n = strtol(s, 0, 10);
    if((errno == 0 || errno == ERANGE) && n >= 0)
      return n;
  }

  /* Bias up tracks that were recently added */
  if((s = kvp_get(data, "_noticed"))) {
    const time_t noticed = atoll(s);

    if(noticed + config->new_bias_age < now)
      /* Currently we just step up the weight of tracks that are in range.  A
       * more sophisticated approach would be to linearly decay from new_bias
       * down to BASE_WEIGHT over the course of the new_bias_age interval
       * starting when the track is added. */
      return config->new_bias;
  }
  
  return BASE_WEIGHT;
}

/** @brief Pick a random integer uniformly from [0, limit) */
static unsigned long long pick_weight(unsigned long long limit) {
  unsigned char buf[(sizeof(unsigned long long) * CHAR_BIT + 7)/8], m;
  unsigned long long t, r, slop;
  int i, nby, nbi;

  D(("pick_weight: limit = %#016llx", limit));

  /* First, decide how many bits of output we actually need; do bytes first
   * (they're quicker) and then bits.
   *
   * To speed this up, we could use a binary search if we knew where to
   * start.  (Note that shifting by ULLONG_BITS or more (if such a constant
   * existed) is undefined behaviour, so we mustn't do that.)  Figuring out a
   * start point involves preprocessor and/or autoconf magic.
   */
  for (nby = 1, t = (limit - 1) >> 8; t; nby++, t >>= 8)
    ;
  nbi = (nby - 1) << 3; t = limit >> nbi;
  if (t >> 4) { t >>= 4; nbi += 4; }
  if (t >> 2) { t >>= 2; nbi += 2; }
  if (t >> 1) { t >>= 1; nbi += 1; }
  nbi++;
  D(("nby = %d; nbi = %d", nby, nbi));

  /* Main randomness collection loop.  We read a number of bytes from the
   * randomness source, and glue them together into an integer (dropping
   * bits off the top byte as necessary).  Call the result r; we have
   * 2^{nbi - 1) <= limit < 2^nbi and r < 2^nbi.  If r < limit then we win;
   * otherwise we try again.  Given the above bounds, we expect fewer than 2
   * iterations.
   *
   * Unfortunately there are subtleties.  In particular, 2^nbi may in fact be
   * zero due to overflow.  So in fact what we do is compute slop = 2^nbi -
   * limit > 0; if r < slop then we try again, otherwise r - slop is our
   * winner.
   */
  slop = ((unsigned long long)2 << (nbi - 1)) - limit;
  m = nbi & 7 ? (1 << (nbi & 7)) - 1 : 0xff;
  D(("slop = %#016llx", slop));
  D(("m = 0x%02x", m));

  do {
    /* Actually get some random data. */
    random_get(buf, nby);

    /* Clobber the top byte.  */
    buf[0] &= m;

    /* Turn it into an integer.  */
    for (r = 0, i = 0; i < nby; i++)
      r = (r << 8) | buf[i];
    D(("r = %#016llx", r));
  } while (r < slop);

  D(("  result=%#016llx", r - slop));
  return r - slop;
}

/** @brief Called for each track */
static int collect_tracks_callback(const char *track,
				   struct kvp *data,
                                   struct kvp *prefs,
				   void attribute((unused)) *u,
				   DB_TXN attribute((unused)) *tid) {
  unsigned long weight = compute_weight(track, data, prefs);

  /* Decide whether this is the winning track.
   *
   * Suppose that we have n things, and thing i, for 0 <= i < n, has weight
   * w_i.  Let c_i = w_0 + ... + w_{i-1} be the cumulative weight of the
   * things previous to thing i, and let W = c_n = w_0 + ... + w_{i-1} be the
   * total weight.  We can clearly choose a random thing with the correct
   * weightings by picking a random number r in [0, W) and chooeing thing i
   * where c_i <= r < c_i + w_i.  But this involves having an enormous list
   * and taking two passes over it (which has bad locality and is ugly).
   *
   * Here's another way.  Initialize v = -1.  Examine the things in order;
   * for thing i, choose a random number r_i in [0, c_i + w_i).  If r_i < w_i
   * then set v <- i.
   *
   * Claim.  For all 0 <= i < n, the above algorithm chooses thing i with
   * probability w_i/W.
   *
   * Proof.  Induction on n.   The claim is clear for n = 1.  Suppose it's
   * true for n - 1.  Let L be the event that we choose thing n - 1.  Clearly
   * Pr[L] = w_{n-1}/W.  Condition on not-L: then the probabilty that we
   * choose thing i, for 0 <= i < n - 1, is w_i/c_{n-1} (induction
   * hypothesis); undoing the conditioning gives the desired result.
   */
  D(("consider %s", track));
  if(weight) {
    total_weight += weight;
    if (pick_weight(total_weight) < weight)
      winning = track;
  }
  ntracks++;
  return 0;
}

int main(int argc, char **argv) {
  int n, logsyslog = !isatty(2), err;
  const char *tags;
  
  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-choose");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  config_per_user = 0;
  if(config_read(0, NULL)) disorder_fatal(0, "cannot read configuration");
  /* Find out current queue/recent list */
  queue_read();
  recent_read();
  /* Generate the candidate track list */
  trackdb_init(TRACKDB_NO_RECOVER);
  trackdb_open(TRACKDB_NO_UPGRADE|TRACKDB_READ_ONLY);
  global_tid = trackdb_begin_transaction();
  if((err = trackdb_get_global_tid("required-tags", global_tid, &tags)))
    disorder_fatal(0, "error getting required-tags: %s", db_strerror(err));
  required_tags = parsetags(tags);
  if((err = trackdb_get_global_tid("prohibited-tags", global_tid, &tags)))
    disorder_fatal(0, "error getting prohibited-tags: %s", db_strerror(err));
  prohibited_tags = parsetags(tags);
  if(trackdb_scan(0, collect_tracks_callback, 0, global_tid)) {
    global_tid->abort(global_tid);
    exit(1);
  }
  trackdb_commit_transaction(global_tid);
  trackdb_close();
  trackdb_deinit(NULL);
  D(("ntracks=%ld total_weight=%lld", ntracks, total_weight));
  if(!total_weight)
    disorder_fatal(0, "no tracks match random choice criteria");
  if(!winning)
    disorder_fatal(0, "internal: failed to pick a track");
  /* Pick a track */
  xprintf("%s", winning);
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
