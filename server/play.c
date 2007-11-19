/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include <ao/ao.h>
#include <sys/wait.h>
#include <sys/un.h>

#include "event.h"
#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "queue.h"
#include "server-queue.h"
#include "trackdb.h"
#include "play.h"
#include "plugin.h"
#include "wstat.h"
#include "eventlog.h"
#include "logfd.h"
#include "syscalls.h"
#include "speaker-protocol.h"
#include "disorder.h"
#include "signame.h"
#include "hash.h"

#define SPEAKER "disorder-speaker"

struct queue_entry *playing;
int paused;

static void finished(ev_source *ev);

static int speaker_fd = -1;
static hash *player_pids;
static int shutting_down;

static void store_player_pid(const char *id, pid_t pid) {
  if(!player_pids) player_pids = hash_new(sizeof (pid_t));
  hash_add(player_pids, id, &pid, HASH_INSERT_OR_REPLACE);
}

static pid_t find_player_pid(const char *id) {
  pid_t *pidp;

  if(player_pids && (pidp = hash_find(player_pids, id))) return *pidp;
  return -1;
}

static void forget_player_pid(const char *id) {
  if(player_pids) hash_remove(player_pids, id);
}

/* called when speaker process terminates */
static int speaker_terminated(ev_source attribute((unused)) *ev,
			      pid_t attribute((unused)) pid,
			      int attribute((unused)) status,
			      const struct rusage attribute((unused)) *rusage,
			      void attribute((unused)) *u) {
  fatal(0, "speaker subprocess %s",
	wstat(status));
}

/* called when speaker process has something to say */
static int speaker_readable(ev_source *ev, int fd,
			    void attribute((unused)) *u) {
  struct speaker_message sm;
  int ret = speaker_recv(fd, &sm);
  
  if(ret < 0) return 0;			/* EAGAIN */
  if(!ret) {				/* EOF */
    ev_fd_cancel(ev, ev_read, fd);
    return 0;
  }
  switch(sm.type) {
  case SM_PAUSED:
    /* track ID is paused, DATA seconds played */
    D(("SM_PAUSED %s %ld", sm.id, sm.data));
    playing->sofar = sm.data;
    break;
  case SM_FINISHED:
    /* the playing track finished */
    D(("SM_FINISHED %s", sm.id));
    finished(ev);
    break;
  case SM_PLAYING:
    /* track ID is playing, DATA seconds played */
    D(("SM_PLAYING %s %ld", sm.id, sm.data));
    playing->sofar = sm.data;
    break;
  default:
    error(0, "unknown message type %d", sm.type);
  }
  return 0;
}

void speaker_setup(ev_source *ev) {
  int sp[2];
  pid_t pid;
  struct speaker_message sm;

  if(socketpair(PF_UNIX, SOCK_DGRAM, 0, sp) < 0)
    fatal(errno, "error calling socketpair");
  if(!(pid = xfork())) {
    exitfn = _exit;
    ev_signal_atfork(ev);
    xdup2(sp[0], 0);
    xdup2(sp[0], 1);
    xclose(sp[0]);
    xclose(sp[1]);
    signal(SIGPIPE, SIG_DFL);
#if 0
    execlp("valgrind", "valgrind", SPEAKER, "--config", configfile,
	   debugging ? "--debug" : "--no-debug",
	   log_default == &log_syslog ? "--syslog" : "--no-syslog",
	   (char *)0);
#else
    execlp(SPEAKER, SPEAKER, "--config", configfile,
	   debugging ? "--debug" : "--no-debug",
	   log_default == &log_syslog ? "--syslog" : "--no-syslog",
	   (char *)0);
#endif
    fatal(errno, "error invoking %s", SPEAKER);
  }
  ev_child(ev, pid, 0, speaker_terminated, 0);
  speaker_fd = sp[1];
  xclose(sp[0]);
  cloexec(speaker_fd);
  /* Wait for the speaker to be ready */
  speaker_recv(speaker_fd, &sm);
  nonblock(speaker_fd);
  ev_fd(ev, ev_read, speaker_fd, speaker_readable, 0, "speaker read");
}

void speaker_reload(void) {
  struct speaker_message sm;

  memset(&sm, 0, sizeof sm);
  sm.type = SM_RELOAD;
  speaker_send(speaker_fd, &sm);
}

/* timeout for play retry */
static int play_again(ev_source *ev,
		      const struct timeval attribute((unused)) *now,
		      void attribute((unused)) *u) {
  D(("play_again"));
  play(ev);
  return 0;
}

/* try calling play() again after @offset@ seconds */
static void retry_play(ev_source *ev, int offset) {
  struct timeval w;

  D(("retry_play(%d)", offset));
  gettimeofday(&w, 0);
  w.tv_sec += offset;
  ev_timeout(ev, 0, &w, play_again, 0);
}

/* Called when the currently playing track finishes playing.  This
 * might be because the player finished or because the speaker process
 * told us so. */
static void finished(ev_source *ev) {
  D(("finished playing=%p", (void *)playing));
  if(!playing)
    return;
  if(playing->state != playing_scratched)
    notify_not_scratched(playing->track, playing->submitter);
  switch(playing->state) {
  case playing_ok:
    eventlog("completed", playing->track, (char *)0);
    break;
  case playing_scratched:
    eventlog("scratched", playing->track, playing->scratched, (char *)0);
    break;
  case playing_failed:
    eventlog("failed", playing->track, wstat(playing->wstat), (char *)0);
    break;
  default:
    break;
  }
  queue_played(playing);
  recent_write();
  forget_player_pid(playing->id);
  playing = 0;
  if(ev) retry_play(ev, config->gap);
}

/* Called when a player terminates. */
static int player_finished(ev_source *ev,
			   pid_t pid,
			   int status,
			   const struct rusage attribute((unused)) *rusage,
			   void *u) {
  struct queue_entry *q = u;

  D(("player_finished pid=%lu status=%#x",
     (unsigned long)pid, (unsigned)status));
  /* Record that this PID is dead.  If we killed the track we might know this
   * already, but also it might have exited or crashed.  Either way we don't
   * want to end up signalling it. */
  if(pid == find_player_pid(q->id))
    forget_player_pid(q->id);
  switch(q->state) {
  case playing_unplayed:
  case playing_random:
    /* If this was a pre-prepared track then either it failed or we
     * deliberately stopped it because it was removed from the queue or moved
     * down it.  So leave it state alone for future use. */
    break;
  default:
    /* We actually started playing this track. */
    if(status) {
      if(q->state != playing_scratched)
	q->state = playing_failed;
    } else 
      q->state = playing_ok;
    break;
  }
  /* Regardless we always report and record the status and do cleanup for
   * prefork calls. */
  if(status)
    error(0, "player for %s %s", q->track, wstat(status));
  if(q->type & DISORDER_PLAYER_PREFORK)
    play_cleanup(q->pl, q->data);
  q->wstat = status;
  /* If this actually was the current track, and does not use the speaker
   * process, then it must have finished.  For raw-output players we will get a
   * separate notification from the speaker process. */
  if(q == playing
     && (q->type & DISORDER_PLAYER_TYPEMASK) != DISORDER_PLAYER_RAW)
    finished(ev);
  return 0;
}

/* Find the player for Q */
static int find_player(const struct queue_entry *q) {
  int n;
  
  for(n = 0; n < config->player.n; ++n)
    if(fnmatch(config->player.s[n].s[0], q->track, 0) == 0)
      break;
  if(n >= config->player.n)
    return -1;
  else
    return n;
}

/* Return values from start() */
#define START_OK 0			/**< @brief Succeeded. */
#define START_HARDFAIL 1		/**< @brief Track is broken. */
#define START_SOFTFAIL 2	   /**< @brief Track OK, system (temporarily?) broken */

/** @brief Play or prepare @p q
 * @param ev Event loop
 * @param q Track to play/prepare
 * @param prepare_only If true, only prepares track
 * @return @ref START_OK, @ref START_HARDFAIL or @ref START_SOFTFAIL
 */
static int start(ev_source *ev,
		 struct queue_entry *q,
		 int prepare_only) {
  int n, lfd;
  const char *p;
  int np[2], sfd;
  struct speaker_message sm;
  char buffer[64];
  int optc;
  ao_sample_format format;
  ao_device *device;
  int retries;
  struct timespec ts;
  const char *waitdevice = 0;
  const char *const *optv;
  pid_t pid, npid;
  struct sockaddr_un addr;
  uint32_t l;

  memset(&sm, 0, sizeof sm);
  D(("start %s %d", q->id, prepare_only));
  if(find_player_pid(q->id) > 0) {
    if(prepare_only) return START_OK;
    /* We have already prepared this track so we just need to tell the speaker
     * process to start actually playing the queued up audio data */
    strcpy(sm.id, q->id);
    sm.type = SM_PLAY;
    speaker_send(speaker_fd, &sm);
    D(("sent SM_PLAY for %s", sm.id));
    return START_OK;
  }
  /* Find the player plugin. */
  if((n = find_player(q)) < 0) return START_HARDFAIL;
  if(!(q->pl = open_plugin(config->player.s[n].s[1], 0)))
    return START_HARDFAIL;
  q->type = play_get_type(q->pl);
  /* Can't prepare non-raw tracks. */
  if(prepare_only
     && (q->type & DISORDER_PLAYER_TYPEMASK) != DISORDER_PLAYER_RAW)
    return START_OK;
  /* Call the prefork function. */
  p = trackdb_rawpath(q->track);
  if(q->type & DISORDER_PLAYER_PREFORK)
    if(!(q->data = play_prefork(q->pl, p))) {
      error(0, "prefork function for %s failed", q->track);
      return START_HARDFAIL;
    }
  /* Use the second arg as the tag if available (it's probably a command name),
   * otherwise the module name. */
  if(!isatty(2))
    lfd = logfd(ev, (config->player.s[n].s[2]
		     ? config->player.s[n].s[2] : config->player.s[n].s[1]));
  else
    lfd = -1;
  optc = config->player.s[n].n - 2;
  optv = (void *)&config->player.s[n].s[2];
  while(optc > 0 && optv[0][0] == '-') {
    if(!strcmp(optv[0], "--")) {
      ++optv;
      --optc;
      break;
    }
    if(!strcmp(optv[0], "--wait-for-device")
       || !strncmp(optv[0], "--wait-for-device=", 18)) {
      if((waitdevice = strchr(optv[0], '='))) {
	++waitdevice;
      } else
	waitdevice = "";		/* use default */
      ++optv;
      --optc;
    } else {
      error(0, "unknown option %s", optv[0]);
      return START_HARDFAIL;
    }
  }
  switch(pid = fork()) {
  case 0:			/* child */
    exitfn = _exit;
    ev_signal_atfork(ev);
    signal(SIGPIPE, SIG_DFL);
    if(lfd != -1) {
      xdup2(lfd, 1);
      xdup2(lfd, 2);
      xclose(lfd);			/* tidy up */
    }
    setpgid(0, 0);
    if((q->type & DISORDER_PLAYER_TYPEMASK) == DISORDER_PLAYER_RAW) {
      /* "Raw" format players always have their output send down a pipe
       * to the disorder-normalize process.  This will connect to the
       * speaker process to actually play the audio data.
       */
      /* np will be the pipe to disorder-normalize */
      if(socketpair(PF_UNIX, SOCK_STREAM, 0, np) < 0)
	fatal(errno, "error calling socketpair");
      xshutdown(np[0], SHUT_WR);	/* normalize reads from np[0] */
      xshutdown(np[1], SHUT_RD);	/* decoder writes to np[1] */
      blocking(np[0]);
      blocking(np[1]);
      /* Start disorder-normalize */
      if(!(npid = xfork())) {
	if(!xfork()) {
	  /* Connect to the speaker process */
	  memset(&addr, 0, sizeof addr);
	  addr.sun_family = AF_UNIX;
	  snprintf(addr.sun_path, sizeof addr.sun_path,
		   "%s/speaker", config->home);
	  sfd = xsocket(PF_UNIX, SOCK_STREAM, 0);
	  if(connect(sfd, (const struct sockaddr *)&addr, sizeof addr) < 0)
	    fatal(errno, "connecting to %s", addr.sun_path);
	  l = strlen(q->id);
	  if(write(sfd, &l, sizeof l) < 0
	     || write(sfd, q->id, l) < 0)
	    fatal(errno, "writing to %s", addr.sun_path);
	  /* Await the ack */
	  read(sfd, &l, 1);
	  /* Plumbing */
	  xdup2(np[0], 0);
	  xdup2(sfd, 1);
	  xclose(np[0]);
	  xclose(np[1]);
	  xclose(sfd);
	  /* Ask the speaker to actually start playing the track; we do it here
	   * so it's definitely after ack. */
	  if(!prepare_only) {
	    strcpy(sm.id, q->id);
	    sm.type = SM_PLAY;
	    speaker_send(speaker_fd, &sm);
	    D(("sent SM_PLAY for %s", sm.id));
	  }
	  /* TODO stderr shouldn't be redirected for disorder-normalize
	   * (but it should be for play_track() */
	  execlp("disorder-normalize", "disorder-normalize",
		 log_default == &log_syslog ? "--syslog" : "--no-syslog",
		 (char *)0);
	  fatal(errno, "executing disorder-normalize");
	  /* end of the innermost fork */
	}
	_exit(0);
	/* end of the middle fork */
      }
      /* Wait for the middle fork to finish */
      while(waitpid(npid, &n, 0) < 0 && errno == EINTR)
	;
      /* Pass the file descriptor to the driver in an environment
       * variable. */
      snprintf(buffer, sizeof buffer, "DISORDER_RAW_FD=%d", np[1]);
      if(putenv(buffer) < 0)
	fatal(errno, "error calling putenv");
      /* Close all the FDs we don't need */
      xclose(np[0]);
    }
    if(waitdevice) {
      ao_initialize();
      if(*waitdevice) {
	n = ao_driver_id(waitdevice);
	if(n == -1)
	  fatal(0, "invalid libao driver: %s", optv[0]);
	} else
	  n = ao_default_driver_id();
      /* Make up a format. */
      memset(&format, 0, sizeof format);
      format.bits = 8;
      format.rate = 44100;
      format.channels = 1;
      format.byte_format = AO_FMT_NATIVE;
      retries = 20;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;	/* 0.1s */
      while((device = ao_open_live(n, &format, 0)) == 0 && retries-- > 0)
	  nanosleep(&ts, 0);
      if(device)
	ao_close(device);
    }
    play_track(q->pl,
	       optv, optc,
	       p,
	       q->track);
    _exit(0);
  case -1:			/* error */
    error(errno, "error calling fork");
    if(q->type & DISORDER_PLAYER_PREFORK)
      play_cleanup(q->pl, q->data);	/* else would leak */
    if(lfd != -1)
      xclose(lfd);
    return START_SOFTFAIL;
  }
  store_player_pid(q->id, pid);
  if(lfd != -1)
    xclose(lfd);
  setpgid(pid, pid);
  ev_child(ev, pid, 0, player_finished, q);
  D(("player subprocess ID %lu", (unsigned long)pid));
  return START_OK;
}

int prepare(ev_source *ev,
	    struct queue_entry *q) {
  int n;

  /* Find the player plugin */
  if(find_player_pid(q->id) > 0) return 0; /* Already going. */
  if((n = find_player(q)) < 0) return -1; /* No player */
  q->pl = open_plugin(config->player.s[n].s[1], 0); /* No player */
  q->type = play_get_type(q->pl);
  if((q->type & DISORDER_PLAYER_TYPEMASK) != DISORDER_PLAYER_RAW)
    return 0;				/* Not a raw player */
  return start(ev, q, 1/*prepare_only*/); /* Prepare it */
}

void abandon(ev_source attribute((unused)) *ev,
	     struct queue_entry *q) {
  struct speaker_message sm;
  pid_t pid = find_player_pid(q->id);

  if(pid < 0) return;			/* Not prepared. */
  if((q->type & DISORDER_PLAYER_TYPEMASK) != DISORDER_PLAYER_RAW)
    return;				/* Not a raw player. */
  /* Terminate the player. */
  kill(-pid, config->signal);
  forget_player_pid(q->id);
  /* Cancel the track. */
  memset(&sm, 0, sizeof sm);
  sm.type = SM_CANCEL;
  strcpy(sm.id, q->id);
  speaker_send(speaker_fd, &sm);
}

int add_random_track(void) {
  struct queue_entry *q;
  const char *p;
  long qlen = 0;
  int rc = 0;

  /* If random play is not enabled then do nothing. */
  if(shutting_down || !random_is_enabled())
    return 0;
  /* Count how big the queue is */
  for(q = qhead.next; q != &qhead; q = q->next)
    ++qlen;
  /* Add random tracks until the queue is at the right size */
  while(qlen < config->queue_pad) {
    /* Try to pick a random track */
    if(!(p = trackdb_random(16))) {
      rc = -1;
      break;
    }
    /* Add it to the end of the queue. */
    q = queue_add(p, 0, WHERE_END);
    q->state = playing_random;
    D(("picked %p (%s) at random", (void *)q, q->track));
    ++qlen;
  }
  /* Commit the queue */
  queue_write();
  return rc;
}

/* try to play a track */
void play(ev_source *ev) {
  struct queue_entry *q;
  int random_enabled = random_is_enabled();

  D(("play playing=%p", (void *)playing));
  if(shutting_down || playing || !playing_is_enabled()) return;
  /* If the queue is empty then add a random track. */
  if(qhead.next == &qhead) {
    if(!random_enabled)
      return;
    if(add_random_track()) {
      /* On error, try again in 10s. */
      retry_play(ev, 10);
      return;
    }
    /* Now there must be at least one track in the queue. */
  }
  q = qhead.next;
  /* If random play is disabled but the track is a random one then don't play
   * it.  play() will be called again when random play is re-enabled. */
  if(!random_enabled && q->state == playing_random)
    return;
  D(("taken %p (%s) from queue", (void *)q, q->track));
  /* Try to start playing. */
  switch(start(ev, q, 0/*!prepare_only*/)) {
  case START_HARDFAIL:
    if(q == qhead.next) {
      queue_remove(q, 0);		/* Abandon this track. */
      queue_played(q);
      recent_write();
    }
    if(qhead.next == &qhead)
      /* Queue is empty, wait a bit before trying something else (so we don't
       * sit there looping madly in the presence of persistent problem).  Note
       * that we might not reliably get a random track lookahead in this case,
       * but if we get here then really there are bigger problems. */
      retry_play(ev, 1);
    else
      /* More in queue, try again now. */
      play(ev);
    break;
  case START_SOFTFAIL:
    /* Try same track again in a bit. */
    retry_play(ev, 10);
    break;
  case START_OK:
    if(q == qhead.next) {
      queue_remove(q, 0);
      queue_write();
    }
    playing = q;
    time(&playing->played);
    playing->state = playing_started;
    notify_play(playing->track, playing->submitter);
    eventlog("playing", playing->track,
	     playing->submitter ? playing->submitter : (const char *)0,
	     (const char *)0);
    /* Maybe add a random track. */
    add_random_track();
    /* If there is another track in the queue prepare it now.  This could
     * potentially be a just-added random track. */
    if(qhead.next != &qhead)
      prepare(ev, qhead.next);
    break;
  }
}

int playing_is_enabled(void) {
  const char *s = trackdb_get_global("playing");

  return !s || !strcmp(s, "yes");
}

void enable_playing(const char *who, ev_source *ev) {
  trackdb_set_global("playing", "yes", who);
  /* Add a random track if necessary. */
  add_random_track();
  play(ev);
}

void disable_playing(const char *who) {
  trackdb_set_global("playing", "no", who);
}

int random_is_enabled(void) {
  const char *s = trackdb_get_global("random-play");

  return !s || !strcmp(s, "yes");
}

void enable_random(const char *who, ev_source *ev) {
  trackdb_set_global("random-play", "yes", who);
  add_random_track();
  play(ev);
}

void disable_random(const char *who) {
  trackdb_set_global("random-play", "no", who);
}

void scratch(const char *who, const char *id) {
  struct queue_entry *q;
  struct speaker_message sm;
  pid_t pid;

  D(("scratch playing=%p state=%d id=%s playing->id=%s",
     (void *)playing,
     playing ? playing->state : 0,
     id ? id : "(none)",
     playing ? playing->id : "(none)"));
  if(playing
     && (playing->state == playing_started
	 || playing->state == playing_paused)
     && (!id
	 || !strcmp(id, playing->id))) {
    playing->state = playing_scratched;
    playing->scratched = who ? xstrdup(who) : 0;
    if((pid = find_player_pid(playing->id)) > 0) {
      D(("kill -%d %lu", config->signal, (unsigned long)pid));
      kill(-pid, config->signal);
      forget_player_pid(playing->id);
    } else
      error(0, "could not find PID for %s", playing->id);
    if((playing->type & DISORDER_PLAYER_TYPEMASK) == DISORDER_PLAYER_RAW) {
      memset(&sm, 0, sizeof sm);
      sm.type = SM_CANCEL;
      strcpy(sm.id, playing->id);
      speaker_send(speaker_fd, &sm);
      D(("sending SM_CANCEL for %s", playing->id));
    }
    /* put a scratch track onto the front of the queue (but don't
     * bother if playing is disabled) */
    if(playing_is_enabled() && config->scratch.n) {
      int r = rand() * (double)config->scratch.n / (RAND_MAX + 1.0);
      q = queue_add(config->scratch.s[r], who, WHERE_START);
      q->state = playing_isscratch;
    }
    notify_scratch(playing->track, playing->submitter, who,
		   time(0) - playing->played);
  }
}

void quitting(ev_source *ev) {
  struct queue_entry *q;
  pid_t pid;

  /* Don't start anything new */
  shutting_down = 1;
  /* Shut down the current player */
  if(playing) {
    if((pid = find_player_pid(playing->id)) > 0) {
      kill(-pid, config->signal);
      forget_player_pid(playing->id);
    } else
      error(0, "could not find PID for %s", playing->id);
    playing->state = playing_quitting;
    finished(0);
  }
  /* Zap any other players */
  for(q = qhead.next; q != &qhead; q = q->next)
    if((pid = find_player_pid(q->id)) > 0) {
      D(("kill -%d %lu", config->signal, (unsigned long)pid));
      kill(-pid, config->signal);
      forget_player_pid(q->id);
    } else
      error(0, "could not find PID for %s", q->id);
  /* Don't need the speaker any more */
  ev_fd_cancel(ev, ev_read, speaker_fd);
  xclose(speaker_fd);
}

int pause_playing(const char *who) {
  struct speaker_message sm;
  long played;
  
  /* Can't pause if already paused or if nothing playing. */
  if(!playing || paused) return 0;
  switch(playing->type & DISORDER_PLAYER_TYPEMASK) {
  case DISORDER_PLAYER_STANDALONE:
    if(!(playing->type & DISORDER_PLAYER_PAUSES)) {
    default:
      error(0,  "cannot pause because player is not powerful enough");
      return -1;
    }
    if(play_pause(playing->pl, &played, playing->data)) {
      error(0, "player indicates it cannot pause");
      return -1;
    }
    time(&playing->lastpaused);
    playing->uptopause = played;
    playing->lastresumed = 0;
    break;
  case DISORDER_PLAYER_RAW:
    memset(&sm, 0, sizeof sm);
    sm.type = SM_PAUSE;
    speaker_send(speaker_fd, &sm);
    break;
  }
  if(who) info("paused by %s", who);
  notify_pause(playing->track, who);
  paused = 1;
  if(playing->state == playing_started)
    playing->state = playing_paused;
  eventlog("state", "pause", (char *)0);
  return 0;
}

void resume_playing(const char *who) {
  struct speaker_message sm;

  if(!paused) return;
  paused = 0;
  if(!playing) return;
  switch(playing->type & DISORDER_PLAYER_TYPEMASK) {
  case DISORDER_PLAYER_STANDALONE:
    if(!playing->type & DISORDER_PLAYER_PAUSES) {
    default:
      /* Shouldn't happen */
      return;
    }
    play_resume(playing->pl, playing->data);
    time(&playing->lastresumed);
    break;
  case DISORDER_PLAYER_RAW:
    memset(&sm, 0, sizeof sm);
    sm.type = SM_RESUME;
    speaker_send(speaker_fd, &sm);
    break;
  }
  if(who) info("resumed by %s", who);
  notify_resume(playing->track, who);
  if(playing->state == playing_paused)
    playing->state = playing_started;
  eventlog("state", "resume", (char *)0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
