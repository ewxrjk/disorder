/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
 * Portions (C) 2007 Mark Wooding
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
/** @file server/speaker.c
 * @brief Speaker process
 *
 * This program is responsible for transmitting a single coherent audio stream
 * to its destination (over the network, to some sound API, to some
 * subprocess).  It receives connections from decoders (or rather from the
 * process that is about to become disorder-normalize) and plays them in the
 * right order.
 *
 * @b Encodings.  For the <a href="http://www.alsa-project.org/">ALSA</a> API,
 * 8- and 16- bit stereo and mono are supported, with any sample rate (within
 * the limits that ALSA can deal with.)
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

#include <config.h>
#include "types.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <ao/ao.h>
#include <string.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "speaker-protocol.h"
#include "user.h"
#include "speaker.h"
#include "printf.h"

/** @brief Linked list of all prepared tracks */
struct track *tracks;

/** @brief Playing track, or NULL */
struct track *playing;

/** @brief Number of bytes pre frame */
size_t bpf;

/** @brief Array of file descriptors for poll() */
struct pollfd fds[NFDS];

/** @brief Next free slot in @ref fds */
int fdno;

/** @brief Listen socket */
static int listenfd;

static time_t last_report;              /* when we last reported */
static int paused;                      /* pause status */

/** @brief The current device state */
enum device_states device_state;

/** @brief Set when idled
 *
 * This is set when the sound device is deliberately closed by idle().
 */
int idled;

/** @brief Selected backend */
static const struct speaker_backend *backend;

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

/* Display version number and terminate. */
static void version(void) {
  xprintf("%s", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

/** @brief Return the number of bytes per frame in @p format */
static size_t bytes_per_frame(const struct stream_header *format) {
  return format->channels * format->bits / 8;
}

/** @brief Find track @p id, maybe creating it if not found */
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

/** @brief Remove track @p id (but do not destroy it) */
static struct track *removetrack(const char *id) {
  struct track *t, **tt;

  D(("removetrack %s", id));
  for(tt = &tracks; (t = *tt) && strcmp(id, t->id); tt = &t->next)
    ;
  if(t)
    *tt = t->next;
  return t;
}

/** @brief Destroy a track */
static void destroy(struct track *t) {
  D(("destroy %s", t->id));
  if(t->fd != -1) xclose(t->fd);
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
  int n;

  D(("fill %s: eof=%d used=%zu",
     t->id, t->eof, t->used));
  if(t->eof) return -1;
  if(t->used < sizeof t->buffer) {
    /* there is room left in the buffer */
    where = (t->start + t->used) % sizeof t->buffer;
    /* Get as much data as we can */
    if(where >= t->start) left = (sizeof t->buffer) - where;
    else left = t->start - where;
    do {
      n = read(t->fd, t->buffer + where, left);
    } while(n < 0 && errno == EINTR);
    if(n < 0) {
      if(errno != EAGAIN) fatal(errno, "error reading sample stream");
      return 0;
    }
    if(n == 0) {
      D(("fill %s: eof detected", t->id));
      t->eof = 1;
      t->playable = 1;
      return -1;
    }
    t->used += n;
    if(t->used == sizeof t->buffer)
      t->playable = 1;
  }
  return 0;
}

/** @brief Close the sound device
 *
 * This is called to deactivate the output device when pausing, and also by the
 * ALSA backend when changing encoding (in which case the sound device will be
 * immediately reactivated).
 */
static void idle(void) {
  D(("idle"));
  if(backend->deactivate) 
    backend->deactivate();
  else
    device_state = device_closed;
  idled = 1;
}

/** @brief Abandon the current track */
void abandon(void) {
  struct speaker_message sm;

  D(("abandon"));
  memset(&sm, 0, sizeof sm);
  sm.type = SM_FINISHED;
  strcpy(sm.id, playing->id);
  speaker_send(1, &sm);
  removetrack(playing->id);
  destroy(playing);
  playing = 0;
}

/** @brief Enable sound output
 *
 * Makes sure the sound device is open and has the right sample format.  Return
 * 0 on success and -1 on error.
 */
static void activate(void) {
  if(backend->activate)
    backend->activate();
  else
    device_state = device_open;
}

/** @brief Check whether the current track has finished
 *
 * The current track is determined to have finished either if the input stream
 * eded before the format could be determined (i.e. it is malformed) or the
 * input is at end of file and there is less than a frame left unplayed.  (So
 * it copes with decoders that crash mid-frame.)
 */
static void maybe_finished(void) {
  if(playing
     && playing->eof
     && playing->used < bytes_per_frame(&config->sample_format))
    abandon();
}

/** @brief Return nonzero if we want to play some audio
 *
 * We want to play audio if there is a current track; and it is not paused; and
 * it is playable according to the rules for @ref track::playable.
 */
static int playable(void) {
  return playing
         && !paused
         && playing->playable;
}

/** @brief Play up to @p frames frames of audio
 *
 * It is always safe to call this function.
 * - If @ref playing is 0 then it will just return
 * - If @ref paused is non-0 then it will just return
 * - If @ref device_state != @ref device_open then it will call activate() and
 * return if it it fails.
 * - If there is not enough audio to play then it play what is available.
 *
 * If there are not enough frames to play then whatever is available is played
 * instead.  It is up to mainloop() to ensure that speaker_play() is not called
 * when unreasonably only an small amounts of data is available to play.
 */
static void speaker_play(size_t frames) {
  size_t avail_frames, avail_bytes, written_frames;
  ssize_t written_bytes;

  /* Make sure there's a track to play and it is not paused */
  if(!playable())
    return;
  /* Make sure the output device is open */
  if(device_state != device_open) {
    activate(); 
    if(device_state != device_open)
      return;
  }
  D(("play: play %zu/%zu%s %dHz %db %dc",  frames, playing->used / bpf,
     playing->eof ? " EOF" : "",
     config->sample_format.rate,
     config->sample_format.bits,
     config->sample_format.channels));
  /* Figure out how many frames there are available to write */
  if(playing->start + playing->used > sizeof playing->buffer)
    /* The ring buffer is currently wrapped, only play up to the wrap point */
    avail_bytes = (sizeof playing->buffer) - playing->start;
  else
    /* The ring buffer is not wrapped, can play the lot */
    avail_bytes = playing->used;
  avail_frames = avail_bytes / bpf;
  /* Only play up to the requested amount */
  if(avail_frames > frames)
    avail_frames = frames;
  if(!avail_frames)
    return;
  /* Play it, Sam */
  written_frames = backend->play(avail_frames);
  written_bytes = written_frames * bpf;
  /* written_bytes and written_frames had better both be set and correct by
   * this point */
  playing->start += written_bytes;
  playing->used -= written_bytes;
  playing->played += written_frames;
  /* If the pointer is at the end of the buffer (or the buffer is completely
   * empty) wrap it back to the start. */
  if(!playing->used || playing->start == (sizeof playing->buffer))
    playing->start = 0;
  /* If the buffer emptied out mark the track as unplayably */
  if(!playing->used && !playing->eof) {
    error(0, "track buffer emptied");
    playing->playable = 0;
  }
  frames -= written_frames;
  return;
}

/* Notify the server what we're up to. */
static void report(void) {
  struct speaker_message sm;

  if(playing) {
    memset(&sm, 0, sizeof sm);
    sm.type = paused ? SM_PAUSED : SM_PLAYING;
    strcpy(sm.id, playing->id);
    sm.data = playing->played / config->sample_format.rate;
    speaker_send(1, &sm);
  }
  time(&last_report);
}

static void reap(int __attribute__((unused)) sig) {
  pid_t cmdpid;
  int st;

  do
    cmdpid = waitpid(-1, &st, WNOHANG);
  while(cmdpid > 0);
  signal(SIGCHLD, reap);
}

int addfd(int fd, int events) {
  if(fdno < NFDS) {
    fds[fdno].fd = fd;
    fds[fdno].events = events;
    return fdno++;
  } else
    return -1;
}

/** @brief Table of speaker backends */
static const struct speaker_backend *backends[] = {
#if HAVE_ALSA_ASOUNDLIB_H
  &alsa_backend,
#endif
  &command_backend,
  &network_backend,
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
  &coreaudio_backend,
#endif
#if HAVE_SYS_SOUNDCARD_H
  &oss_backend,
#endif
  0
};

/** @brief Main event loop */
static void mainloop(void) {
  struct track *t;
  struct speaker_message sm;
  int n, fd, stdin_slot, timeout, listen_slot;

  while(getppid() != 1) {
    fdno = 0;
    /* By default we will wait up to a second before thinking about current
     * state. */
    timeout = 1000;
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
    if(playable()) {
      /* We want to play some audio.  If the device is closed then we attempt
       * to open it. */
      if(device_state == device_closed)
        activate();
      /* If the device is (now) open then we will wait up until it is ready for
       * more.  If something went wrong then we should have device_error
       * instead, but the post-poll code will cope even if it's
       * device_closed. */
      if(device_state == device_open)
        backend->beforepoll(&timeout);
    }
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
    /* Wait for something interesting to happen */
    n = poll(fds, fdno, timeout);
    if(n < 0) {
      if(errno == EINTR) continue;
      fatal(errno, "error calling poll");
    }
    /* Play some sound before doing anything else */
    if(playable()) {
      /* We want to play some audio */
      if(device_state == device_open) {
        if(backend->ready())
          speaker_play(3 * FRAMES);
      } else {
        /* We must be in _closed or _error, and it should be the latter, but we
         * cope with either.
         *
         * We most likely timed out, so now is a good time to retry.
         * speaker_play() knows to re-activate the device if necessary.
         */
        speaker_play(3 * FRAMES);
      }
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
          error(errno, "reading length from inbound connection");
          xclose(fd);
        } else if(l >= sizeof id) {
          error(0, "id length too long");
          xclose(fd);
        } else if(read(fd, id, l) < (ssize_t)l) {
          error(errno, "reading id from inbound connection");
          xclose(fd);
        } else {
          id[l] = 0;
          D(("id %s fd %d", id, fd));
          t = findtrack(id, 1/*create*/);
          write(fd, "", 1);             /* write an ack */
          if(t->fd != -1) {
            error(0, "%s: already got a connection", id);
            xclose(fd);
          } else {
            nonblock(fd);
            t->fd = fd;               /* yay */
          }
        }
      } else
        error(errno, "accept");
    }
    /* Perhaps we have a command to process */
    if(fds[stdin_slot].revents & POLLIN) {
      /* There might (in theory) be several commands queued up, but in general
       * this won't be the case, so we don't bother looping around to pick them
       * all up. */ 
      n = speaker_recv(0, &sm);
      /* TODO */
      if(n > 0)
	switch(sm.type) {
	case SM_PLAY:
          if(playing) fatal(0, "got SM_PLAY but already playing something");
	  t = findtrack(sm.id, 1);
          D(("SM_PLAY %s fd %d", t->id, t->fd));
          if(t->fd == -1)
            error(0, "cannot play track because no connection arrived");
          playing = t;
          /* We attempt to play straight away rather than going round the loop.
           * speaker_play() is clever enough to perform any activation that is
           * required. */
          speaker_play(3 * FRAMES);
          report();
	  break;
	case SM_PAUSE:
          D(("SM_PAUSE"));
	  paused = 1;
          report();
          break;
	case SM_RESUME:
          D(("SM_RESUME"));
          if(paused) {
            paused = 0;
            /* As for SM_PLAY we attempt to play straight away. */
            if(playing)
              speaker_play(3 * FRAMES);
          }
          report();
	  break;
	case SM_CANCEL:
          D(("SM_CANCEL %s",  sm.id));
	  t = removetrack(sm.id);
	  if(t) {
	    if(t == playing) {
              sm.type = SM_FINISHED;
              strcpy(sm.id, playing->id);
              speaker_send(1, &sm);
	      playing = 0;
            }
	    destroy(t);
	  } else
	    error(0, "SM_CANCEL for unknown track %s", sm.id);
          report();
	  break;
	case SM_RELOAD:
          D(("SM_RELOAD"));
	  if(config_read(1)) error(0, "cannot read configuration");
          info("reloaded configuration");
	  break;
	default:
	  error(0, "unknown message type %d", sm.type);
        }
    }
    /* Read in any buffered data */
    for(t = tracks; t; t = t->next)
      if(t->fd != -1
         && t->slot != -1
         && (fds[t->slot].revents & (POLLIN | POLLHUP)))
         speaker_fill(t);
    /* Maybe we finished playing a track somewhere in the above */
    maybe_finished();
    /* If we don't need the sound device for now then close it for the benefit
     * of anyone else who wants it. */
    if((!playing || paused) && device_state == device_open)
      idle();
    /* If we've not reported out state for a second do so now. */
    if(time(0) > last_report)
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

  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  if((d = getenv("DISORDER_DEBUG_SPEAKER"))) debugging = atoi(d);
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read(1)) fatal(0, "cannot read configuration");
  bpf = bytes_per_frame(&config->sample_format);
  /* ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
  /* reap kids */
  signal(SIGCHLD, reap);
  /* set nice value */
  xnice(config->nice_speaker);
  /* change user */
  become_mortal();
  /* make sure we're not root, whatever the config says */
  if(getuid() == 0 || geteuid() == 0) fatal(0, "do not run as root");
  /* identify the backend used to play */
  for(n = 0; backends[n]; ++n)
    if(backends[n]->backend == config->api)
      break;
  if(!backends[n])
    fatal(0, "unsupported api %d", config->api);
  backend = backends[n];
  /* backend-specific initialization */
  backend->init();
  /* create the socket directory */
  byte_xasprintf(&dir, "%s/speaker", config->home);
  unlink(dir);                          /* might be a leftover socket */
  if(mkdir(dir, 0700) < 0 && errno != EEXIST)
    fatal(errno, "error creating %s", dir);
  /* set up the listen socket */
  listenfd = xsocket(PF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof addr.sun_path, "%s/speaker/socket",
           config->home);
  if(unlink(addr.sun_path) < 0 && errno != ENOENT)
    error(errno, "removing %s", addr.sun_path);
  xsetsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if(bind(listenfd, (const struct sockaddr *)&addr, sizeof addr) < 0)
    fatal(errno, "error binding socket to %s", addr.sun_path);
  xlisten(listenfd, 128);
  nonblock(listenfd);
  info("listening on %s", addr.sun_path);
  memset(&sm, 0, sizeof sm);
  sm.type = SM_READY;
  speaker_send(1, &sm);
  mainloop();
  info("stopped (parent terminated)");
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
