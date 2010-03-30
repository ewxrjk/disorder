/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2010 Richard Kettlewell
 * Portions (C) 2007 Mark Wooding
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
/** @file server/speaker.c
 * @brief Speaker process
 *
 * This program is responsible for transmitting a single coherent audio stream
 * to its destination (over the network, to some sound API, to some
 * subprocess).  It receives connections from decoders (or rather from the
 * process that is about to become disorder-normalize) and plays them in the
 * right order.
 *
 * @b Model.  mainloop() implements a select loop awaiting commands from the
 * main server, new connections to the speaker socket, and audio data on those
 * connections.  Each connection starts with a queue ID (with a 32-bit
 * native-endian length word), allowing it to be referred to in commands from
 * the server.
 *
 * Data read on connections is buffered, up to a limit (currently 1Mbyte per
 * track).  No attempt is made here to limit the number of tracks, it is
 * assumed that the main server won't start outrageously many decoders.
 *
 * Audio is supplied from this buffer to the uaudio play callback.  Playback is
 * enabled when a track is to be played and disabled when the its last bytes
 * have been return by the callback; pause and resume is implemneted the
 * obvious way.  If the callback finds itself required to play when there is no
 * playing track it returns dead air.
 *
 * To implement gapless playback, the server is notified that a track has
 * finished slightly early.  @ref SM_PLAY is therefore allowed to arrive while
 * the previous track is still playing provided an early @ref SM_FINISHED has
 * been sent for it.
 *
 * @b Encodings.  The encodings supported depend entirely on the uaudio backend
 * chosen.  See @ref uaudio.h, etc.
 *
 * Inbound data is expected to match @c config->sample_format.  In normal use
 * this is arranged by the @c disorder-normalize program (see @ref
 * server/normalize.c).
 *
 * @b Garbage @b Collection.  This program deliberately does not use the
 * garbage collector even though it might be convenient to do so.  This is for
 * two reasons.  Firstly some sound APIs use thread threads and we do not want
 * to have to deal with potential interactions between threading and garbage
 * collection.  Secondly this process needs to be able to respond quickly and
 * this is not compatible with the collector hanging the program even
 * relatively briefly.
 *
 * @b Units.  This program thinks at various times in three different units.
 * Bytes are obvious.  A sample is a single sample on a single channel.  A
 * frame is several samples on different channels at the same point in time.
 * So (for instance) a 16-bit stereo frame is 4 bytes and consists of a pair of
 * 2-byte samples.
 */

#include "common.h"

#include <getopt.h>
#include <locale.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <ao/ao.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/resource.h>
#include <gcrypt.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "speaker-protocol.h"
#include "user.h"
#include "printf.h"
#include "version.h"
#include "uaudio.h"

/** @brief Maximum number of FDs to poll for */
#define NFDS 1024

/** @brief Number of bytes before end of track to send SM_FINISHED
 *
 * Generally set to 1 second.
 */
static size_t early_finish;

/** @brief Track structure
 *
 * Known tracks are kept in a linked list.  Usually there will be at most two
 * of these but rearranging the queue can cause there to be more.
 */
struct track {
  /** @brief Next track */
  struct track *next;

  /** @brief Input file descriptor */
  int fd;                               /* input FD */

  /** @brief Track ID */
  char id[24];

  /** @brief Start position of data in buffer */
  size_t start;

  /** @brief Number of bytes of data in buffer */
  size_t used;

  /** @brief Set @c fd is at EOF */
  int eof;

  /** @brief Total number of samples played */
  unsigned long long played;

  /** @brief Slot in @ref fds */
  int slot;

  /** @brief Set when playable
   *
   * A track becomes playable whenever it fills its buffer or reaches EOF; it
   * stops being playable when it entirely empties its buffer.  Tracks start
   * out life not playable.
   */
  int playable;

  /** @brief Set when finished
   *
   * This is set when we've notified the server that the track is finished.
   * Once this has happened (typically very late in the track's lifetime) the
   * track cannot be paused or cancelled.
   */
  int finished;
  
  /** @brief Input buffer
   *
   * 1Mbyte is enough for nearly 6s of 44100Hz 16-bit stereo
   */
  char buffer[1048576];
};

/** @brief Lock protecting data structures
 *
 * This lock protects values shared between the main thread and the callback.
 *
 * It is held 'all' the time by the main thread, the exceptions being when
 * called activate/deactivate callbacks and when calling (potentially) slow
 * system calls (in particular poll(), where in fact the main thread will spend
 * most of its time blocked).
 *
 * The callback holds it when it's running.
 */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Linked list of all prepared tracks */
static struct track *tracks;

/** @brief Playing track, or NULL
 *
 * This means the track the speaker process intends to play.  It does not
 * reflect any other state (e.g. activation of uaudio backend).
 */
static struct track *playing;

/** @brief Pending playing track, or NULL
 *
 * This means the track the server wants the speaker to play.
 */
static struct track *pending_playing;

/** @brief Array of file descriptors for poll() */
static struct pollfd fds[NFDS];

/** @brief Next free slot in @ref fds */
static int fdno;

/** @brief Listen socket */
static int listenfd;

/** @brief Timestamp of last potential report to server */
static time_t last_report;

/** @brief Set when paused */
static int paused;

/** @brief Set when back end activated */
static int activated;

/** @brief Signal pipe back into the poll() loop */
static int sigpipe[2];

/** @brief Selected backend */
static const struct uaudio *backend;

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

/* Display usage message and terminate. */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-speaker [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Force logging\n"
          "\n"
          "Speaker process for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/** @brief Find track @p id, maybe creating it if not found
 * @param id Track ID to find
 * @param create If nonzero, create track structure of @p id not found
 * @return Pointer to track structure or NULL
 */
static struct track *findtrack(const char *id, int create) {
  struct track *t;

  D(("findtrack %s %d", id, create));
  for(t = tracks; t && strcmp(id, t->id); t = t->next)
    ;
  if(!t && create) {
    t = xmalloc(sizeof *t);
    t->next = tracks;
    strcpy(t->id, id);
    t->fd = -1;
    tracks = t;
  }
  return t;
}

/** @brief Remove track @p id (but do not destroy it)
 * @param id Track ID to remove
 * @return Track structure or NULL if not found
 */
static struct track *removetrack(const char *id) {
  struct track *t, **tt;

  D(("removetrack %s", id));
  for(tt = &tracks; (t = *tt) && strcmp(id, t->id); tt = &t->next)
    ;
  if(t)
    *tt = t->next;
  return t;
}

/** @brief Destroy a track
 * @param t Track structure
 */
static void destroy(struct track *t) {
  D(("destroy %s", t->id));
  if(t->fd != -1)
    xclose(t->fd);
  free(t);
}

/** @brief Read data into a sample buffer
 * @param t Pointer to track
 * @return 0 on success, -1 on EOF
 *
 * This is effectively the read callback on @c t->fd.  It is called from the
 * main loop whenever the track's file descriptor is readable, assuming the
 * buffer has not reached the maximum allowed occupancy.
 */
static int speaker_fill(struct track *t) {
  size_t where, left;
  int n, rc;

  D(("fill %s: eof=%d used=%zu",
     t->id, t->eof, t->used));
  if(t->eof)
    return -1;
  if(t->used < sizeof t->buffer) {
    /* there is room left in the buffer */
    where = (t->start + t->used) % sizeof t->buffer;
    /* Get as much data as we can */
    if(where >= t->start)
      left = (sizeof t->buffer) - where;
    else
      left = t->start - where;
    pthread_mutex_unlock(&lock);
    do {
      n = read(t->fd, t->buffer + where, left);
    } while(n < 0 && errno == EINTR);
    pthread_mutex_lock(&lock);
    if(n < 0 && errno == EAGAIN) {
      /* EAGAIN means more later */
      rc = 0;
    } else if(n <= 0) {
      /* n=0 means EOF.  n<0 means some error occurred.  We log the error but
       * otherwise treat it as identical to EOF. */
      if(n < 0)
        disorder_error(errno, "error reading sample stream for %s", t->id);
      else
        D(("fill %s: eof detected", t->id));
      t->eof = 1;
      /* A track always becomes playable at EOF; we're not going to see any
       * more data. */
      t->playable = 1;
      rc = -1;
    } else {
      t->used += n;
      /* A track becomes playable when it (first) fills its buffer.  For
       * 44.1KHz 16-bit stereo this is ~6s of audio data.  The latency will
       * depend how long that takes to decode (hopefuly not very!) */
      if(t->used == sizeof t->buffer)
        t->playable = 1;
      rc = 0;
    }
  }
  return rc;
}

/** @brief Return nonzero if we want to play some audio
 *
 * We want to play audio if there is a current track; and it is not paused; and
 * it is playable according to the rules for @ref track::playable.
 *
 * We don't allow tracks to be paused if we've already told the server we've
 * finished them; that would cause such tracks to survive much longer than the
 * few samples they're supposed to, with report() remaining silent for the
 * duration.
 */
static int playable(void) {
  return playing
         && (!paused || playing->finished)
         && playing->playable;
}

/** @brief Notify the server what we're up to */
static void report(void) {
  struct speaker_message sm;

  if(playing) {
    /* Had better not send a report for a track that the server thinks has
     * finished, that would be confusing. */
    if(playing->finished)
      return;
    memset(&sm, 0, sizeof sm);
    sm.type = paused ? SM_PAUSED : SM_PLAYING;
    strcpy(sm.id, playing->id);
    sm.data = playing->played / (uaudio_rate * uaudio_channels);
    speaker_send(1, &sm);
    xtime(&last_report);
  }
}

/** @brief Add a file descriptor to the set to poll() for
 * @param fd File descriptor
 * @param events Events to wait for e.g. @c POLLIN
 * @return Slot number
 */
static int addfd(int fd, int events) {
  if(fdno < NFDS) {
    fds[fdno].fd = fd;
    fds[fdno].events = events;
    return fdno++;
  } else
    return -1;
}

/** @brief Callback to return some sampled data
 * @param buffer Where to put sample data
 * @param max_samples How many samples to return
 * @param userdata User data
 * @return Number of samples written
 *
 * See uaudio_callback().
 */
static size_t speaker_callback(void *buffer,
                               size_t max_samples,
                               void attribute((unused)) *userdata) {
  const size_t max_bytes = max_samples * uaudio_sample_size;
  size_t provided_samples = 0;

  pthread_mutex_lock(&lock);
  /* TODO perhaps we should immediately go silent if we've been asked to pause
   * or cancel the playing track (maybe block in the cancel case and see what
   * else turns up?) */
  if(playing) {
    if(playing->used > 0) {
      size_t bytes;
      /* Compute size of largest contiguous chunk.  We get called as often as
       * necessary so there's no need for cleverness here. */
      if(playing->start + playing->used > sizeof playing->buffer)
        bytes = sizeof playing->buffer - playing->start;
      else
        bytes = playing->used;
      /* Limit to what we were asked for */
      if(bytes > max_bytes)
        bytes = max_bytes;
      /* Provide it */
      memcpy(buffer, playing->buffer + playing->start, bytes);
      playing->start += bytes;
      playing->used -= bytes;
      /* Wrap around to start of buffer */
      if(playing->start == sizeof playing->buffer)
        playing->start = 0;
      /* See if we've reached the end of the track */
      if(playing->used == 0 && playing->eof) {
        int ignored = write(sigpipe[1], "", 1);
        (void) ignored;
      }
      provided_samples = bytes / uaudio_sample_size;
      playing->played += provided_samples;
    }
  }
  /* If we couldn't provide anything at all, play dead air */
  /* TODO maybe it would be better to block, in some cases? */
  if(!provided_samples) {
    memset(buffer, 0, max_bytes);
    provided_samples = max_samples;
    if(playing)
      disorder_info("%zu samples silence, playing->used=%zu",
                    provided_samples, playing->used);
    else
      disorder_info("%zu samples silence, playing=NULL", provided_samples);
  }
  pthread_mutex_unlock(&lock);
  return provided_samples;
}

/** @brief Main event loop */
static void mainloop(void) {
  struct track *t;
  struct speaker_message sm;
  int n, fd, stdin_slot, timeout, listen_slot, sigpipe_slot;

  /* Keep going while our parent process is alive */
  pthread_mutex_lock(&lock);
  while(getppid() != 1) {
    int force_report = 0;

    fdno = 0;
    /* By default we will wait up to half a second before thinking about
     * current state. */
    timeout = 500;
    /* Always ready for commands from the main server. */
    stdin_slot = addfd(0, POLLIN);
    /* Also always ready for inbound connections */
    listen_slot = addfd(listenfd, POLLIN);
    /* Try to read sample data for the currently playing track if there is
     * buffer space. */
    if(playing
       && playing->fd >= 0
       && !playing->eof
       && playing->used < (sizeof playing->buffer))
      playing->slot = addfd(playing->fd, POLLIN);
    else if(playing)
      playing->slot = -1;
    /* If any other tracks don't have a full buffer, try to read sample data
     * from them.  We do this last of all, so that if we run out of slots,
     * nothing important can't be monitored. */
    for(t = tracks; t; t = t->next)
      if(t != playing) {
        if(t->fd >= 0
           && !t->eof
           && t->used < sizeof t->buffer) {
          t->slot = addfd(t->fd,  POLLIN | POLLHUP);
        } else
          t->slot = -1;
      }
    sigpipe_slot = addfd(sigpipe[0], POLLIN);
    /* Wait for something interesting to happen */
    pthread_mutex_unlock(&lock);
    n = poll(fds, fdno, timeout);
    pthread_mutex_lock(&lock);
    if(n < 0) {
      if(errno == EINTR) continue;
      disorder_fatal(errno, "error calling poll");
    }
    /* Perhaps a connection has arrived */
    if(fds[listen_slot].revents & POLLIN) {
      struct sockaddr_un addr;
      socklen_t addrlen = sizeof addr;
      uint32_t l;
      char id[24];

      if((fd = accept(listenfd, (struct sockaddr *)&addr, &addrlen)) >= 0) {
        blocking(fd);
        if(read(fd, &l, sizeof l) < 4) {
          disorder_error(errno, "reading length from inbound connection");
          xclose(fd);
        } else if(l >= sizeof id) {
          disorder_error(0, "id length too long");
          xclose(fd);
        } else if(read(fd, id, l) < (ssize_t)l) {
          disorder_error(errno, "reading id from inbound connection");
          xclose(fd);
        } else {
          id[l] = 0;
          D(("id %s fd %d", id, fd));
          t = findtrack(id, 1/*create*/);
          if (write(fd, "", 1) < 0)             /* write an ack */
            disorder_error(errno, "writing ack to inbound connection for %s",
                           id);
          if(t->fd != -1) {
            disorder_error(0, "%s: already got a connection", id);
            xclose(fd);
          } else {
            nonblock(fd);
            t->fd = fd;               /* yay */
          }
          /* Notify the server that the connection arrived */
          sm.type = SM_ARRIVED;
          strcpy(sm.id, id);
          speaker_send(1, &sm);
        }
      } else
        disorder_error(errno, "accept");
    }
    /* Perhaps we have a command to process */
    if(fds[stdin_slot].revents & POLLIN) {
      /* There might (in theory) be several commands queued up, but in general
       * this won't be the case, so we don't bother looping around to pick them
       * all up. */ 
      n = speaker_recv(0, &sm);
      if(n > 0)
        /* As a rule we don't send success replies to most commands - we just
         * force the regular status update to be sent immediately rather than
         * on schedule. */
	switch(sm.type) {
	case SM_PLAY:
          /* SM_PLAY is only allowed if the server reasonably believes that
           * nothing is playing */
          if(playing) {
            /* If finished isn't set then the server can't believe that this
             * track has finished */
            if(!playing->finished)
              disorder_fatal(0, "got SM_PLAY but already playing something");
            /* If pending_playing is set then the server must believe that that
             * is playing */
            if(pending_playing)
              disorder_fatal(0, "got SM_PLAY but have a pending playing track");
          }
	  t = findtrack(sm.id, 1);
          D(("SM_PLAY %s fd %d", t->id, t->fd));
          if(t->fd == -1)
            disorder_error(0,
                           "cannot play track because no connection arrived");
          /* TODO as things stand we often report this error message but then
           * appear to proceed successfully.  Understanding why requires a look
           * at play.c: we call prepare() which makes the connection in a child
           * process, and then sends the SM_PLAY in the parent process.  The
           * latter may well be faster.  As it happens this is harmless; we'll
           * just sit around sending silence until the decoder connects and
           * starts sending some sample data.  But is is annoying and ought to
           * be fixed. */
          pending_playing = t;
          /* If nothing is currently playing then we'll switch to the pending
           * track below so there's no point distinguishing the situations
           * here. */
	  break;
	case SM_PAUSE:
          D(("SM_PAUSE"));
	  paused = 1;
          force_report = 1;
          break;
	case SM_RESUME:
          D(("SM_RESUME"));
          paused = 0;
          force_report = 1;
	  break;
	case SM_CANCEL:
          D(("SM_CANCEL %s", sm.id));
	  t = removetrack(sm.id);
	  if(t) {
	    if(t == playing || t == pending_playing) {
              /* Scratching the track that the server believes is playing,
               * which might either be the actual playing track or a pending
               * playing track */
              sm.type = SM_FINISHED;
              if(t == playing)
                playing = 0;
              else
                pending_playing = 0;
            } else {
              /* Could be scratching the playing track before it's quite got
               * going, or could be just removing a track from the queue.  We
               * log more because there's been a bug here recently than because
               * it's particularly interesting; the log message will be removed
               * if no further problems show up. */
              disorder_info("SM_CANCEL for nonplaying track %s", sm.id);
              sm.type = SM_STILLBORN;
            }
            strcpy(sm.id, t->id);
	    destroy(t);
	  } else {
            /* Probably scratching the playing track well before it's got
             * going, but could indicate a bug, so we log this as an error. */
            sm.type = SM_UNKNOWN;
	    disorder_error(0, "SM_CANCEL for unknown track %s", sm.id);
          }
          speaker_send(1, &sm);
          force_report = 1;
	  break;
	case SM_RELOAD:
          D(("SM_RELOAD"));
	  if(config_read(1, NULL))
            disorder_error(0, "cannot read configuration");
          disorder_info("reloaded configuration");
	  break;
	default:
	  disorder_error(0, "unknown message type %d", sm.type);
        }
    }
    /* Read in any buffered data */
    for(t = tracks; t; t = t->next)
      if(t->fd != -1
         && t->slot != -1
         && (fds[t->slot].revents & (POLLIN | POLLHUP)))
         speaker_fill(t);
    /* Drain the signal pipe.  We don't care about its contents, merely that it
     * interrupted poll(). */
    if(fds[sigpipe_slot].revents & POLLIN) {
      char buffer[64];
      int ignored; (void)ignored;

      ignored = read(sigpipe[0], buffer, sizeof buffer);
    }
    /* Send SM_FINISHED when we're near the end of the track.
     *
     * This is how we implement gapless play; we hope that the SM_PLAY from the
     * server arrives before the remaining bytes of the track play out.
     */
    if(playing
       && playing->eof
       && !playing->finished
       && playing->used <= early_finish) {
      memset(&sm, 0, sizeof sm);
      sm.type = SM_FINISHED;
      strcpy(sm.id, playing->id);
      speaker_send(1, &sm);
      playing->finished = 1;
    }
    /* When the track is actually finished, deconfigure it */
    if(playing && playing->eof && !playing->used) {
      removetrack(playing->id);
      destroy(playing);
      playing = 0;
    }
    /* Act on the pending SM_PLAY */
    if(!playing && pending_playing) {
      playing = pending_playing;
      pending_playing = 0;
      force_report = 1;
    }
    /* Impose any state change required by the above */
    if(playable()) {
      if(!activated) {
        activated = 1;
        pthread_mutex_unlock(&lock);
        backend->activate();
        pthread_mutex_lock(&lock);
      }
    } else {
      if(activated) {
        activated = 0;
        pthread_mutex_unlock(&lock);
        backend->deactivate();
        pthread_mutex_lock(&lock);
      }
    }
    /* If we've not reported our state for a second do so now. */
    if(force_report || xtime(0) > last_report)
      report();
  }
}

int main(int argc, char **argv) {
  int n, logsyslog = !isatty(2);
  struct sockaddr_un addr;
  static const int one = 1;
  struct speaker_message sm;
  const char *d;
  char *dir;
  struct rlimit rl[1];

  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-speaker");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if((d = getenv("DISORDER_DEBUG_SPEAKER"))) debugging = atoi(d);
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  config_uaudio_apis = uaudio_apis;
  if(config_read(1, NULL)) disorder_fatal(0, "cannot read configuration");
  /* ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
  /* set nice value */
  xnice(config->nice_speaker);
  /* change user */
  become_mortal();
  /* make sure we're not root, whatever the config says */
  if(getuid() == 0 || geteuid() == 0)
    disorder_fatal(0, "do not run as root");
  /* Make sure we can't have more than NFDS files open (it would bust our
   * poll() array) */
  if(getrlimit(RLIMIT_NOFILE, rl) < 0)
    disorder_fatal(errno, "getrlimit RLIMIT_NOFILE");
  if(rl->rlim_cur > NFDS) {
    rl->rlim_cur = NFDS;
    if(setrlimit(RLIMIT_NOFILE, rl) < 0)
      disorder_fatal(errno, "setrlimit to reduce RLIMIT_NOFILE to %lu",
            (unsigned long)rl->rlim_cur);
    disorder_info("set RLIM_NOFILE to %lu", (unsigned long)rl->rlim_cur);
  } else
    disorder_info("RLIM_NOFILE is %lu", (unsigned long)rl->rlim_cur);
  /* gcrypt initialization */
  if(!gcry_check_version(NULL))
    disorder_fatal(0, "gcry_check_version failed");
  gcry_control(GCRYCTL_INIT_SECMEM, 0);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
  /* create a pipe between the backend callback and the poll() loop */
  xpipe(sigpipe);
  nonblock(sigpipe[0]);
  /* set up audio backend */
  uaudio_set_format(config->sample_format.rate,
                    config->sample_format.channels,
                    config->sample_format.bits,
                    config->sample_format.bits != 8);
  early_finish = uaudio_sample_size * uaudio_channels * uaudio_rate;
  /* TODO other parameters! */
  backend = uaudio_find(config->api);
  /* backend-specific initialization */
  if(backend->configure)
    backend->configure();
  backend->start(speaker_callback, NULL);
  /* create the socket directory */
  byte_xasprintf(&dir, "%s/speaker", config->home);
  unlink(dir);                          /* might be a leftover socket */
  if(mkdir(dir, 0700) < 0 && errno != EEXIST)
    disorder_fatal(errno, "error creating %s", dir);
  /* set up the listen socket */
  listenfd = xsocket(PF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof addr.sun_path, "%s/speaker/socket",
           config->home);
  if(unlink(addr.sun_path) < 0 && errno != ENOENT)
    disorder_error(errno, "removing %s", addr.sun_path);
  xsetsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if(bind(listenfd, (const struct sockaddr *)&addr, sizeof addr) < 0)
    disorder_fatal(errno, "error binding socket to %s", addr.sun_path);
  xlisten(listenfd, 128);
  nonblock(listenfd);
  disorder_info("listening on %s", addr.sun_path);
  memset(&sm, 0, sizeof sm);
  sm.type = SM_READY;
  speaker_send(1, &sm);
  mainloop();
  disorder_info("stopped (parent terminated)");
  exit(0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
