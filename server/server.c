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

#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <stddef.h>
#include <time.h>
#include <limits.h>
#include <pcre.h>
#include <netdb.h>
#include <netinet/in.h>

#include "event.h"
#include "server.h"
#include "syscalls.h"
#include "queue.h"
#include "play.h"
#include "log.h"
#include "mem.h"
#include "state.h"
#include "charset.h"
#include "split.h"
#include "configuration.h"
#include "hex.h"
#include "trackdb.h"
#include "table.h"
#include "kvp.h"
#include "mixer.h"
#include "sink.h"
#include "authhash.h"
#include "plugin.h"
#include "printf.h"
#include "trackname.h"
#include "eventlog.h"
#include "defs.h"
#include "cache.h"

#ifndef NONCE_SIZE
# define NONCE_SIZE 16
#endif

int volume_left, volume_right;		/* last known volume */

/** @brief Accept all well-formed login attempts
 *
 * Used in debugging.
 */
int wideopen;

struct listener {
  const char *name;
  int pf;
};

struct conn {
  ev_reader *r;
  ev_writer *w;
  int fd;
  unsigned tag;
  char *who;
  ev_source *ev;
  unsigned char nonce[NONCE_SIZE];
  ev_reader_callback *reader;
  struct eventlog_output *lo;
  const struct listener *l;
};

static int reader_callback(ev_source *ev,
			   ev_reader *reader,
			   int fd,
			   void *ptr,
			   size_t bytes,
			   int eof,
			   void *u);

static const char *noyes[] = { "no", "yes" };

static int writer_error(ev_source attribute((unused)) *ev,
			int fd,
			int errno_value,
			void *u) {
  struct conn *c = u;

  D(("server writer_error %d %d", fd, errno_value));
  if(errno_value == 0) {
    /* writer is done */
    c->w = 0;
    if(c->r == 0) {
      D(("server writer_error closes %d", fd));
      xclose(fd);		/* reader is done too, close */
    } else {
      D(("server writer_error shutdown %d SHUT_WR", fd));
      xshutdown(fd, SHUT_WR);	/* reader is not done yet */
    }
  } else {
    if(errno_value != EPIPE)
      error(errno_value, "S%x write error on socket", c->tag);
    if(c->r)
      ev_reader_cancel(c->r);
    xclose(fd);
  }
  return 0;
}

static int reader_error(ev_source attribute((unused)) *ev,
			int fd,
			int errno_value,
			void *u) {
  struct conn *c = u;

  D(("server reader_error %d %d", fd, errno_value));
  error(errno, "S%x read error on socket", c->tag);
  ev_writer_cancel(c->w);
  xclose(fd);
  return 0;
}

/* return true if we are talking to a trusted user */
static int trusted(struct conn *c) {
  int n;
  
  for(n = 0; (n < config->trust.n
	      && strcmp(config->trust.s[n], c->who)); ++n)
    ;
  return n < config->trust.n;
}

static int c_disable(struct conn *c, char **vec, int nvec) {
  if(nvec == 0)
    disable_playing(c->who);
  else if(nvec == 1 && !strcmp(vec[0], "now"))
    disable_playing(c->who);
  else {
    sink_writes(ev_writer_sink(c->w), "550 invalid argument\n");
    return 1;			/* completed */
  }
  sink_writes(ev_writer_sink(c->w), "250 OK\n");
  return 1;			/* completed */
}

static int c_enable(struct conn *c,
		    char attribute((unused)) **vec,
		    int attribute((unused)) nvec) {
  enable_playing(c->who, c->ev);
  /* Enable implicitly unpauses if there is nothing playing */
  if(paused && !playing) resume_playing(c->who);
  sink_writes(ev_writer_sink(c->w), "250 OK\n");
  return 1;			/* completed */
}

static int c_enabled(struct conn *c,
		     char attribute((unused)) **vec,
		     int attribute((unused)) nvec) {
  sink_printf(ev_writer_sink(c->w), "252 %s\n", noyes[playing_is_enabled()]);
  return 1;			/* completed */
}

static int c_play(struct conn *c, char **vec,
		  int attribute((unused)) nvec) {
  const char *track;
  struct queue_entry *q;
  
  if(!trackdb_exists(vec[0])) {
    sink_writes(ev_writer_sink(c->w), "550 track is not in database\n");
    return 1;
  }
  if(!(track = trackdb_resolve(vec[0]))) {
    sink_writes(ev_writer_sink(c->w), "550 cannot resolve track\n");
    return 1;
  }
  q = queue_add(track, c->who, WHERE_BEFORE_RANDOM);
  queue_write();
  /* If we added the first track, and something is playing, then prepare the
   * new track.  If nothing is playing then we don't bother as it wouldn't gain
   * anything. */
  if(q == qhead.next && playing)
    prepare(c->ev, q);
  sink_writes(ev_writer_sink(c->w), "250 queued\n");
  /* If the queue was empty but we are for some reason paused then
   * unpause. */
  if(!playing) resume_playing(0);
  play(c->ev);
  return 1;			/* completed */
}

static int c_remove(struct conn *c, char **vec,
		    int attribute((unused)) nvec) {
  struct queue_entry *q;

  if(!(q = queue_find(vec[0]))) {
    sink_writes(ev_writer_sink(c->w), "550 no such track on the queue\n");
    return 1;
  }
  if(config->restrictions & RESTRICT_REMOVE) {
    /* can only remove tracks that you submitted */
    if(!q->submitter || strcmp(q->submitter, c->who)) {
      sink_writes(ev_writer_sink(c->w), "550 you didn't submit that track!\n");
      return 1;
    }
  }
  queue_remove(q, c->who);
  /* De-prepare the track. */
  abandon(c->ev, q);
  /* If we removed the random track then add another one. */
  if(q->state == playing_random)
    add_random_track();
  /* Prepare whatever the next head track is. */
  if(qhead.next != &qhead)
    prepare(c->ev, qhead.next);
  queue_write();
  sink_writes(ev_writer_sink(c->w), "250 removed\n");
  return 1;			/* completed */
}

static int c_scratch(struct conn *c,
		     char **vec,
		     int nvec) {
  if(!playing) {
    sink_writes(ev_writer_sink(c->w), "250 nothing is playing\n");
    return 1;			/* completed */
  }
  if(config->restrictions & RESTRICT_SCRATCH) {
    /* can only scratch tracks you submitted and randomly selected ones */
    if(playing->submitter && strcmp(playing->submitter, c->who)) {
      sink_writes(ev_writer_sink(c->w), "550 you didn't submit that track!\n");
      return 1;
    }
  }
  scratch(c->who, nvec == 1 ? vec[0] : 0);
  /* If you scratch an unpaused track then it is automatically unpaused */
  resume_playing(0);
  sink_writes(ev_writer_sink(c->w), "250 scratched\n");
  return 1;			/* completed */
}

static int c_pause(struct conn *c,
		   char attribute((unused)) **vec,
		   int attribute((unused)) nvec) {
  if(!playing) {
    sink_writes(ev_writer_sink(c->w), "250 nothing is playing\n");
    return 1;			/* completed */
  }
  if(paused) {
    sink_writes(ev_writer_sink(c->w), "250 already paused\n");
    return 1;			/* completed */
  }
  if(pause_playing(c->who) < 0)
    sink_writes(ev_writer_sink(c->w), "550 cannot pause this track\n");
  else
    sink_writes(ev_writer_sink(c->w), "250 paused\n");
  return 1;
}

static int c_resume(struct conn *c,
		   char attribute((unused)) **vec,
		   int attribute((unused)) nvec) {
  if(!paused) {
    sink_writes(ev_writer_sink(c->w), "250 not paused\n");
    return 1;			/* completed */
  }
  resume_playing(c->who);
  sink_writes(ev_writer_sink(c->w), "250 paused\n");
  return 1;
}

static int c_shutdown(struct conn *c,
		      char attribute((unused)) **vec,
		      int attribute((unused)) nvec) {
  info("S%x shut down by %s", c->tag, c->who);
  sink_writes(ev_writer_sink(c->w), "250 shutting down\n");
  ev_writer_flush(c->w);
  quit(c->ev);
}

static int c_reconfigure(struct conn *c,
			 char attribute((unused)) **vec,
			 int attribute((unused)) nvec) {
  info("S%x reconfigure by %s", c->tag, c->who);
  if(reconfigure(c->ev, 1))
    sink_writes(ev_writer_sink(c->w), "550 error reading new config\n");
  else
    sink_writes(ev_writer_sink(c->w), "250 installed new config\n");
  return 1;				/* completed */
}

static int c_rescan(struct conn *c,
		    char attribute((unused)) **vec,
		    int attribute((unused)) nvec) {
  info("S%x rescan by %s", c->tag, c->who);
  trackdb_rescan(c->ev);
  sink_writes(ev_writer_sink(c->w), "250 initiated rescan\n");
  return 1;				/* completed */
}

static int c_version(struct conn *c,
		     char attribute((unused)) **vec,
		     int attribute((unused)) nvec) {
  /* VERSION had better only use the basic character set */
  sink_printf(ev_writer_sink(c->w), "251 %s\n", disorder_version_string);
  return 1;			/* completed */
}

static int c_playing(struct conn *c,
		     char attribute((unused)) **vec,
		     int attribute((unused)) nvec) {
  if(playing) {
    queue_fix_sofar(playing);
    playing->expected = 0;
    sink_printf(ev_writer_sink(c->w), "252 %s\n", queue_marshall(playing));
  } else
    sink_printf(ev_writer_sink(c->w), "259 nothing playing\n");
  return 1;				/* completed */
}

static int c_become(struct conn *c,
		  char **vec,
		  int attribute((unused)) nvec) {
  c->who = vec[0];
  sink_writes(ev_writer_sink(c->w), "230 OK\n");
  return 1;
}

static int c_user(struct conn *c,
		  char **vec,
		  int attribute((unused)) nvec) {
  int n;
  const char *res;
  union {
    struct sockaddr sa;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
  } u;
  socklen_t l;
  char host[1024];

  if(c->who) {
    sink_writes(ev_writer_sink(c->w), "530 already authenticated\n");
    return 1;
  }
  /* get connection data */
  l = sizeof u;
  if(getpeername(c->fd, &u.sa, &l) < 0) {
    error(errno, "S%x error calling getpeername", c->tag);
    sink_writes(ev_writer_sink(c->w), "530 authentication failure\n");
    return 1;
  }
  if(c->l->pf != PF_UNIX) {
    if((n = getnameinfo(&u.sa, l,
			host, sizeof host, 0, 0, NI_NUMERICHOST))) {
      error(0, "S%x error calling getnameinfo: %s", c->tag, gai_strerror(n));
      sink_writes(ev_writer_sink(c->w), "530 authentication failure\n");
      return 1;
    }
  } else
    strcpy(host, "local");
  /* find the user */
  for(n = 0; n < config->allow.n
	&& strcmp(config->allow.s[n].s[0], vec[0]); ++n)
    ;
  /* if it's a real user check whether the response is right */
  if(n >= config->allow.n) {
    info("S%x unknown user '%s' from %s", c->tag, vec[0], host);
    sink_writes(ev_writer_sink(c->w), "530 authentication failed\n");
    return 1;
  }
  res = authhash(c->nonce, sizeof c->nonce, config->allow.s[n].s[1]);
  if(wideopen || (res && !strcmp(res, vec[1]))) {
    c->who = vec[0];
    /* currently we only bother logging remote connections */
    if(c->l->pf != PF_UNIX)
      info("S%x %s connected from %s", c->tag, vec[0], host);
    sink_writes(ev_writer_sink(c->w), "230 OK\n");
    return 1;
  }
  /* oops, response was wrong */
  info("S%x authentication failure for %s from %s", c->tag, vec[0], host);
  sink_writes(ev_writer_sink(c->w), "530 authentication failed\n");
  return 1;
}

static int c_recent(struct conn *c,
		    char attribute((unused)) **vec,
		    int attribute((unused)) nvec) {
  const struct queue_entry *q;

  sink_writes(ev_writer_sink(c->w), "253 Tracks follow\n");
  for(q = phead.next; q != &phead; q = q->next)
    sink_printf(ev_writer_sink(c->w), " %s\n", queue_marshall(q));
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;				/* completed */
}

static int c_queue(struct conn *c,
		   char attribute((unused)) **vec,
		   int attribute((unused)) nvec) {
  struct queue_entry *q;
  time_t when = 0;
  const char *l;
  long length;

  sink_writes(ev_writer_sink(c->w), "253 Tracks follow\n");
  if(playing_is_enabled() && !paused) {
    if(playing) {
      queue_fix_sofar(playing);
      if((l = trackdb_get(playing->track, "_length"))
	 && (length = atol(l))) {
	time(&when);
	when += length - playing->sofar + config->gap;
      }
    } else
      /* Nothing is playing but playing is enabled, so whatever is
       * first in the queue can be expected to start immediately. */
      time(&when);
  }
  for(q = qhead.next; q != &qhead; q = q->next) {
    /* fill in estimated start time */
    q->expected = when;
    sink_printf(ev_writer_sink(c->w), " %s\n", queue_marshall(q));
    /* update for next track */
    if(when) {
      if((l = trackdb_get(q->track, "_length"))
	 && (length = atol(l)))
	when += length + config->gap;
      else
	when = 0;
    }
  }
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;				/* completed */
}

static int output_list(struct conn *c, char **vec) {
  while(*vec)
    sink_printf(ev_writer_sink(c->w), "%s\n", *vec++);
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;
}

static int files_dirs(struct conn *c,
		      char **vec,
		      int nvec,
		      enum trackdb_listable what) {
  const char *dir, *re, *errstr;
  int erroffset;
  pcre *rec;
  char **fvec, *key;
  
  switch(nvec) {
  case 0: dir = 0; re = 0; break;
  case 1: dir = vec[0]; re = 0; break;
  case 2: dir = vec[0]; re = vec[1]; break;
  default: abort();
  }
  /* A bit of a bodge to make sure the args don't trample on cache keys */
  if(dir && strchr(dir, '\n')) {
    sink_writes(ev_writer_sink(c->w), "550 invalid directory name\n");
    return 1;
  }
  if(re && strchr(re, '\n')) {
    sink_writes(ev_writer_sink(c->w), "550 invalid regexp\n");
    return 1;
  }
  /* We bother eliminating "" because the web interface is relatively
   * likely to send it */
  if(re && *re) {
    byte_xasprintf(&key, "%d\n%s\n%s", (int)what, dir ? dir : "", re);
    fvec = (char **)cache_get(&cache_files_type, key);
    if(fvec) {
      /* Got a cache hit, don't store the answer in the cache */
      key = 0;
      ++cache_files_hits;
      rec = 0;				/* quieten compiler */
    } else {
      /* Cache miss, we'll do the lookup and key != 0 so we'll store the answer
       * in the cache. */
      if(!(rec = pcre_compile(re, PCRE_CASELESS|PCRE_UTF8,
			      &errstr, &erroffset, 0))) {
	sink_printf(ev_writer_sink(c->w), "550 Error compiling regexp: %s\n",
		    errstr);
	return 1;
      }
      /* It only counts as a miss if the regexp was valid. */
      ++cache_files_misses;
    }
  } else {
    /* No regexp, don't bother caching the result */
    rec = 0;
    key = 0;
    fvec = 0;
  }
  if(!fvec) {
    /* No cache hit (either because a miss, or because we did not look) so do
     * the lookup */
    if(dir && *dir)
      fvec = trackdb_list(dir, 0, what, rec);
    else
      fvec = trackdb_list(0, 0, what, rec);
  }
  if(key)
    /* Put the answer in the cache */
    cache_put(&cache_files_type, key, fvec);
  sink_writes(ev_writer_sink(c->w), "253 Listing follow\n");
  return output_list(c, fvec);
}

static int c_files(struct conn *c,
		  char **vec,
		  int nvec) {
  return files_dirs(c, vec, nvec, trackdb_files);
}

static int c_dirs(struct conn *c,
		  char **vec,
		  int nvec) {
  return files_dirs(c, vec, nvec, trackdb_directories);
}

static int c_allfiles(struct conn *c,
		      char **vec,
		      int nvec) {
  return files_dirs(c, vec, nvec, trackdb_directories|trackdb_files);
}

static int c_get(struct conn *c,
		 char **vec,
		 int attribute((unused)) nvec) {
  const char *v;

  if(vec[1][0] != '_' && (v = trackdb_get(vec[0], vec[1])))
    sink_printf(ev_writer_sink(c->w), "252 %s\n", v);
  else
    sink_writes(ev_writer_sink(c->w), "550 not found\n");
  return 1;
}

static int c_length(struct conn *c,
		 char **vec,
		 int attribute((unused)) nvec) {
  const char *track, *v;

  if(!(track = trackdb_resolve(vec[0]))) {
    sink_writes(ev_writer_sink(c->w), "550 cannot resolve track\n");
    return 1;
  }
  if((v = trackdb_get(track, "_length")))
    sink_printf(ev_writer_sink(c->w), "252 %s\n", v);
  else
    sink_writes(ev_writer_sink(c->w), "550 not found\n");
  return 1;
}

static int c_set(struct conn *c,
		 char **vec,
		 int attribute((unused)) nvec) {
  if(vec[1][0] != '_' && !trackdb_set(vec[0], vec[1], vec[2]))
    sink_writes(ev_writer_sink(c->w), "250 OK\n");
  else
    sink_writes(ev_writer_sink(c->w), "550 not found\n");
  return 1;
}

static int c_prefs(struct conn *c,
		   char **vec,
		   int attribute((unused)) nvec) {
  struct kvp *k;

  k = trackdb_get_all(vec[0]);
  sink_writes(ev_writer_sink(c->w), "253 prefs follow\n");
  for(; k; k = k->next)
    if(k->name[0] != '_')		/* omit internal values */
      sink_printf(ev_writer_sink(c->w),
		  " %s %s\n", quoteutf8(k->name), quoteutf8(k->value));
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;
}

static int c_exists(struct conn *c,
		    char **vec,
		    int attribute((unused)) nvec) {
  sink_printf(ev_writer_sink(c->w), "252 %s\n", noyes[trackdb_exists(vec[0])]);
  return 1;
}

static void search_parse_error(const char *msg, void *u) {
  *(const char **)u = msg;
}

static int c_search(struct conn *c,
			  char **vec,
			  int attribute((unused)) nvec) {
  char **terms, **results;
  int nterms, nresults, n;
  const char *e = "unknown error";

  /* This is a bit of a bodge.  Initially it's there to make the eclient
   * interface a bit more convenient to add searching to, but it has the more
   * compelling advantage that if everything uses it, then interpretation of
   * user-supplied search strings will be the same everywhere. */
  if(!(terms = split(vec[0], &nterms, SPLIT_QUOTES, search_parse_error, &e))) {
    sink_printf(ev_writer_sink(c->w), "550 %s\n", e);
  } else {
    results = trackdb_search(terms, nterms, &nresults);
    sink_printf(ev_writer_sink(c->w), "253 %d matches\n", nresults);
    for(n = 0; n < nresults; ++n)
      sink_printf(ev_writer_sink(c->w), "%s\n", results[n]);
    sink_writes(ev_writer_sink(c->w), ".\n");
  }
  return 1;
}

static int c_random_enable(struct conn *c,
			   char attribute((unused)) **vec,
			   int attribute((unused)) nvec) {
  enable_random(c->who, c->ev);
  /* Enable implicitly unpauses if there is nothing playing */
  if(paused && !playing) resume_playing(c->who);
  sink_writes(ev_writer_sink(c->w), "250 OK\n");
  return 1;			/* completed */
}

static int c_random_disable(struct conn *c,
			    char attribute((unused)) **vec,
			    int attribute((unused)) nvec) {
  disable_random(c->who);
  sink_writes(ev_writer_sink(c->w), "250 OK\n");
  return 1;			/* completed */
}

static int c_random_enabled(struct conn *c,
			    char attribute((unused)) **vec,
			    int attribute((unused)) nvec) {
  sink_printf(ev_writer_sink(c->w), "252 %s\n", noyes[random_is_enabled()]);
  return 1;			/* completed */
}

static int c_stats(struct conn *c,
		   char attribute((unused)) **vec,
		   int attribute((unused)) nvec) {
  char **v;
  int nv, n;

  v = trackdb_stats(&nv);
  sink_printf(ev_writer_sink(c->w), "253 stats\n");
  for(n = 0; n < nv; ++n) {
    if(v[n][0] == '.')
      sink_writes(ev_writer_sink(c->w), ".");
    sink_printf(ev_writer_sink(c->w), "%s\n", v[n]);
  }
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;
}

static int c_volume(struct conn *c,
		    char **vec,
		    int nvec) {
  int l, r, set;
  char lb[32], rb[32];

  switch(nvec) {
  case 0:
    set = 0;
    break;
  case 1:
    l = r = atoi(vec[0]);
    set = 1;
    break;
  case 2:
    l = atoi(vec[0]);
    r = atoi(vec[1]);
    set = 1;
    break;
  default:
    abort();
  }
  if(mixer_control(&l, &r, set))
    sink_writes(ev_writer_sink(c->w), "550 error accessing mixer\n");
  else {
    sink_printf(ev_writer_sink(c->w), "252 %d %d\n", l, r);
    if(l != volume_left || r != volume_right) {
      volume_left = l;
      volume_right = r;
      snprintf(lb, sizeof lb, "%d", l);
      snprintf(rb, sizeof rb, "%d", r);
      eventlog("volume", lb, rb, (char *)0);
    }
  }
  return 1;
}

/* we are logging, and some data is available to read */
static int logging_reader_callback(ev_source *ev,
				   ev_reader *reader,
				   int fd,
				   void *ptr,
				   size_t bytes,
				   int eof,
				   void *u) {
  struct conn *c = u;

  /* don't log to this conn any more */
  eventlog_remove(c->lo);
  /* terminate the log output */
  sink_writes(ev_writer_sink(c->w), ".\n");
  /* restore the reader callback */
  c->reader = reader_callback;
  /* ...and exit via it */
  return c->reader(ev, reader, fd, ptr, bytes, eof, u);
}

static void logclient(const char *msg, void *user) {
  struct conn *c = user;

  sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" %s\n",
	      (uintmax_t)time(0), msg);
}

static int c_log(struct conn *c,
		 char attribute((unused)) **vec,
		 int attribute((unused)) nvec) {
  time_t now;

  sink_writes(ev_writer_sink(c->w), "254 OK\n");
  /* pump out initial state */
  time(&now);
  sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" state %s\n",
	      (uintmax_t)now, 
	      playing_is_enabled() ? "enable_play" : "disable_play");
  sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" state %s\n",
	      (uintmax_t)now, 
	      random_is_enabled() ? "enable_random" : "disable_random");
  sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" state %s\n",
	      (uintmax_t)now, 
	      paused ? "pause" : "resume");
  if(playing)
    sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" state playing\n",
		(uintmax_t)now);
  /* Initial volume */
  sink_printf(ev_writer_sink(c->w), "%"PRIxMAX" volume %d %d\n",
	      (uintmax_t)now, volume_left, volume_right);
  c->lo = xmalloc(sizeof *c->lo);
  c->lo->fn = logclient;
  c->lo->user = c;
  eventlog_add(c->lo);
  c->reader = logging_reader_callback;
  return 0;
}

static void post_move_cleanup(void) {
  struct queue_entry *q;

  /* If we have caused the random track to not be at the end then we make it no
   * longer be random. */
  for(q = qhead.next; q != &qhead; q = q->next)
    if(q->state == playing_random && q->next != &qhead)
      q->state = playing_unplayed;
  /* That might mean we need to add a new random track. */
  add_random_track();
  queue_write();
}

static int c_move(struct conn *c,
		  char **vec,
		  int attribute((unused)) nvec) {
  struct queue_entry *q;
  int n;

  if(config->restrictions & RESTRICT_MOVE) {
    if(!trusted(c)) {
      sink_writes(ev_writer_sink(c->w),
		  "550 only trusted users can move tracks\n");
      return 1;
    }
  }
  if(!(q = queue_find(vec[0]))) {
    sink_writes(ev_writer_sink(c->w), "550 no such track on the queue\n");
    return 1;
  }
  n = queue_move(q, atoi(vec[1]), c->who);
  post_move_cleanup();
  sink_printf(ev_writer_sink(c->w), "252 %d\n", n);
  /* If we've moved to the head of the queue then prepare the track. */
  if(q == qhead.next)
    prepare(c->ev, q);
  return 1;
}

static int c_moveafter(struct conn *c,
		       char **vec,
		       int attribute((unused)) nvec) {
  struct queue_entry *q, **qs;
  int n;

  if(config->restrictions & RESTRICT_MOVE) {
    if(!trusted(c)) {
      sink_writes(ev_writer_sink(c->w),
		  "550 only trusted users can move tracks\n");
      return 1;
    }
  }
  if(vec[0][0]) {
    if(!(q = queue_find(vec[0]))) {
      sink_writes(ev_writer_sink(c->w), "550 no such track on the queue\n");
      return 1;
    }
  } else
    q = 0;
  ++vec;
  --nvec;
  qs = xcalloc(nvec, sizeof *qs);
  for(n = 0; n < nvec; ++n)
    if(!(qs[n] = queue_find(vec[n]))) {
      sink_writes(ev_writer_sink(c->w), "550 no such track on the queue\n");
      return 1;
    }
  queue_moveafter(q, nvec, qs, c->who);
  post_move_cleanup();
  sink_printf(ev_writer_sink(c->w), "250 Moved tracks\n");
  /* If we've moved to the head of the queue then prepare the track. */
  if(q == qhead.next)
    prepare(c->ev, q);
  return 1;
}

static int c_part(struct conn *c,
		  char **vec,
		  int attribute((unused)) nvec) {
  sink_printf(ev_writer_sink(c->w), "252 %s\n",
	      trackdb_getpart(vec[0], vec[1], vec[2]));
  return 1;
}

static int c_resolve(struct conn *c,
		     char **vec,
		     int attribute((unused)) nvec) {
  const char *track;

  if(!(track = trackdb_resolve(vec[0]))) {
    sink_writes(ev_writer_sink(c->w), "550 cannot resolve track\n");
    return 1;
  }
  sink_printf(ev_writer_sink(c->w), "252 %s\n", track);
  return 1;
}

static int c_tags(struct conn *c,
		  char attribute((unused)) **vec,
		  int attribute((unused)) nvec) {
  char **tags = trackdb_alltags();
  
  sink_printf(ev_writer_sink(c->w), "253 Tag list follows\n");
  while(*tags) {
    sink_printf(ev_writer_sink(c->w), "%s%s\n",
		**tags == '.' ? "." : "", *tags);
    ++tags;
  }
  sink_writes(ev_writer_sink(c->w), ".\n");
  return 1;				/* completed */

}

static int c_set_global(struct conn *c,
			char **vec,
			int attribute((unused)) nvec) {
  trackdb_set_global(vec[0], vec[1], c->who);
  sink_printf(ev_writer_sink(c->w), "250 OK\n");
  return 1;
}

static int c_get_global(struct conn *c,
			char **vec,
			int attribute((unused)) nvec) {
  const char *s = trackdb_get_global(vec[0]);

  if(s)
    sink_printf(ev_writer_sink(c->w), "252 %s\n", s);
  else
    sink_writes(ev_writer_sink(c->w), "550 not found\n");
  return 1;
}

static int c_nop(struct conn *c,
		 char attribute((unused)) **vec,
		 int attribute((unused)) nvec) {
  sink_printf(ev_writer_sink(c->w), "250 Quack\n");
  return 1;
}

#define C_AUTH		0001		/* must be authenticated */
#define C_TRUSTED	0002		/* must be trusted user */

static const struct command {
  const char *name;
  int minargs, maxargs;
  int (*fn)(struct conn *, char **, int);
  unsigned flags;
} commands[] = {
  { "allfiles",       0, 2,       c_allfiles,       C_AUTH },
  { "become",         1, 1,       c_become,         C_AUTH|C_TRUSTED },
  { "dirs",           0, 2,       c_dirs,           C_AUTH },
  { "disable",        0, 1,       c_disable,        C_AUTH },
  { "enable",         0, 0,       c_enable,         C_AUTH },
  { "enabled",        0, 0,       c_enabled,        C_AUTH },
  { "exists",         1, 1,       c_exists,         C_AUTH },
  { "files",          0, 2,       c_files,          C_AUTH },
  { "get",            2, 2,       c_get,            C_AUTH },
  { "get-global",     1, 1,       c_get_global,     C_AUTH },
  { "length",         1, 1,       c_length,         C_AUTH },
  { "log",            0, 0,       c_log,            C_AUTH },
  { "move",           2, 2,       c_move,           C_AUTH },
  { "moveafter",      1, INT_MAX, c_moveafter,      C_AUTH },
  { "nop",            0, 0,       c_nop,            C_AUTH },
  { "part",           3, 3,       c_part,           C_AUTH },
  { "pause",          0, 0,       c_pause,          C_AUTH },
  { "play",           1, 1,       c_play,           C_AUTH },
  { "playing",        0, 0,       c_playing,        C_AUTH },
  { "prefs",          1, 1,       c_prefs,          C_AUTH },
  { "queue",          0, 0,       c_queue,          C_AUTH },
  { "random-disable", 0, 0,       c_random_disable, C_AUTH },
  { "random-enable",  0, 0,       c_random_enable,  C_AUTH },
  { "random-enabled", 0, 0,       c_random_enabled, C_AUTH },
  { "recent",         0, 0,       c_recent,         C_AUTH },
  { "reconfigure",    0, 0,       c_reconfigure,    C_AUTH|C_TRUSTED },
  { "remove",         1, 1,       c_remove,         C_AUTH },
  { "rescan",         0, 0,       c_rescan,         C_AUTH|C_TRUSTED },
  { "resolve",        1, 1,       c_resolve,        C_AUTH },
  { "resume",         0, 0,       c_resume,         C_AUTH },
  { "scratch",        0, 1,       c_scratch,        C_AUTH },
  { "search",         1, 1,       c_search,         C_AUTH },
  { "set",            3, 3,       c_set,            C_AUTH, },
  { "set-global",     2, 2,       c_set_global,     C_AUTH },
  { "shutdown",       0, 0,       c_shutdown,       C_AUTH|C_TRUSTED },
  { "stats",          0, 0,       c_stats,          C_AUTH },
  { "tags",           0, 0,       c_tags,           C_AUTH },
  { "unset",          2, 2,       c_set,            C_AUTH },
  { "unset-global",   1, 1,       c_set_global,      C_AUTH },
  { "user",           2, 2,       c_user,           0 },
  { "version",        0, 0,       c_version,        C_AUTH },
  { "volume",         0, 2,       c_volume,         C_AUTH }
};

static void command_error(const char *msg, void *u) {
  struct conn *c = u;

  sink_printf(ev_writer_sink(c->w), "500 parse error: %s\n", msg);
}

/* process a command.  Return 1 if complete, 0 if incomplete. */
static int command(struct conn *c, char *line) {
  char **vec;
  int nvec, n;

  D(("server command %s", line));
  if(!(vec = split(line, &nvec, SPLIT_QUOTES, command_error, c))) {
    sink_writes(ev_writer_sink(c->w), "500 cannot parse command\n");
    return 1;
  }
  if(nvec == 0) {
    sink_writes(ev_writer_sink(c->w), "500 do what?\n");
    return 1;
  }
  if((n = TABLE_FIND(commands, struct command, name, vec[0])) < 0)
    sink_writes(ev_writer_sink(c->w), "500 unknown command\n");
  else {
    if((commands[n].flags & C_AUTH) && !c->who) {
      sink_writes(ev_writer_sink(c->w), "530 not authenticated\n");
      return 1;
    }
    if((commands[n].flags & C_TRUSTED) && !trusted(c)) {
      sink_writes(ev_writer_sink(c->w), "530 insufficient privilege\n");
      return 1;
    }
    ++vec;
    --nvec;
    if(nvec < commands[n].minargs) {
      sink_writes(ev_writer_sink(c->w), "500 missing argument(s)\n");
      return 1;
    }
    if(nvec > commands[n].maxargs) {
      sink_writes(ev_writer_sink(c->w), "500 too many arguments\n");
      return 1;
    }
    return commands[n].fn(c, vec, nvec);
  }
  return 1;			/* completed */
}

/* redirect to the right reader callback for our current state */
static int redirect_reader_callback(ev_source *ev,
				    ev_reader *reader,
				    int fd,
				    void *ptr,
				    size_t bytes,
				    int eof,
				    void *u) {
  struct conn *c = u;

  return c->reader(ev, reader, fd, ptr, bytes, eof, u);
}

/* the main command reader */
static int reader_callback(ev_source attribute((unused)) *ev,
			   ev_reader *reader,
			   int attribute((unused)) fd,
			   void *ptr,
			   size_t bytes,
			   int eof,
			   void *u) {
  struct conn *c = u;
  char *eol;
  int complete;

  D(("server reader_callback"));
  while((eol = memchr(ptr, '\n', bytes))) {
    *eol++ = 0;
    ev_reader_consume(reader, eol - (char *)ptr);
    complete = command(c, ptr);
    bytes -= (eol - (char *)ptr);
    ptr = eol;
    if(!complete) {
      /* the command had better have set a new reader callback */
      if(bytes || eof)
	/* there are further bytes to read, or we are at eof; arrange for the
	 * command's reader callback to handle them */
	return ev_reader_incomplete(reader);
      /* nothing's going on right now */
      return 0;
    }
    /* command completed, we can go around and handle the next one */
  }
  if(eof) {
    if(bytes)
      error(0, "S%x unterminated line", c->tag);
    c->r = 0;
    return ev_writer_close(c->w);
  }
  return 0;
}

static int listen_callback(ev_source *ev,
			   int fd,
			   const struct sockaddr attribute((unused)) *remote,
			   socklen_t attribute((unused)) rlen,
			   void *u) {
  const struct listener *l = u;
  struct conn *c = xmalloc(sizeof *c);
  static unsigned tags;

  D(("server listen_callback fd %d (%s)", fd, l->name));
  nonblock(fd);
  cloexec(fd);
  c->tag = tags++;
  c->ev = ev;
  c->w = ev_writer_new(ev, fd, writer_error, c);
  c->r = ev_reader_new(ev, fd, redirect_reader_callback, reader_error, c);
  c->fd = fd;
  c->reader = reader_callback;
  c->l = l;
  gcry_randomize(c->nonce, sizeof c->nonce, GCRY_STRONG_RANDOM);
  sink_printf(ev_writer_sink(c->w), "231 %s\n", hex(c->nonce, sizeof c->nonce));
  return 0;
}

int server_start(ev_source *ev, int pf,
		 size_t socklen, const struct sockaddr *sa,
		 const char *name) {
  int fd;
  struct listener *l = xmalloc(sizeof *l);
  static const int one = 1;

  D(("server_init socket %s", name));
  fd = xsocket(pf, SOCK_STREAM, 0);
  xsetsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if(bind(fd, sa, socklen) < 0) {
    error(errno, "error binding to %s", name);
    return -1;
  }
  xlisten(fd, 128);
  nonblock(fd);
  cloexec(fd);
  l->name = name;
  l->pf = pf;
  if(ev_listen(ev, fd, listen_callback, l)) exit(EXIT_FAILURE);
  return fd;
}

int server_stop(ev_source *ev, int fd) {
  xclose(fd);
  return ev_listen_cancel(ev, fd);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
