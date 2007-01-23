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

/* This program deliberately does not use the garbage collector even though it
 * might be convenient to do so.  This is for two reasons.  Firstly some libao
 * drivers are implemented using threads and we do not want to have to deal
 * with potential interactions between threading and garbage collection.
 * Secondly this process needs to be able to respond quickly and this is not
 * compatible with the collector hanging the program even relatively
 * briefly. */

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
#include <time.h>
#include <alsa/asoundlib.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "speaker.h"
#include "user.h"

#define BUFFER_SECONDS 5                /* How many seconds of input to
                                         * buffer. */

#define FRAMES 4096                     /* Frame batch size */

#define NFDS 256                        /* Max FDs to poll for */

/* Known tracks are kept in a linked list.  We don't normally to have
 * more than two - maybe three at the outside. */
static struct track {
  struct track *next;                   /* next track */
  int fd;                               /* input FD */
  char id[24];                          /* ID */
  size_t start, used;                   /* start + bytes used */
  int eof;                              /* input is at EOF */
  int got_format;                       /* got format yet? */
  ao_sample_format format;              /* sample format */
  unsigned long long played;            /* number of frames played */
  char *buffer;                         /* sample buffer */
  size_t size;                          /* sample buffer size */
  int slot;                             /* poll array slot */
} *tracks, *playing;                    /* all tracks + playing track */

static time_t last_report;              /* when we last reported */
static int paused;                      /* pause status */
static snd_pcm_t *pcm;                  /* current pcm handle */
static ao_sample_format pcm_format;     /* current format if aodev != 0 */
static size_t bpf;                      /* bytes per frame */
static struct pollfd fds[NFDS];         /* if we need more than that */
static int fdno;                        /* fd number */
static snd_pcm_uframes_t pcm_bufsize;   /* buffer size */
static int forceplay;                   /* frames to force play */

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
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
          "\n"
          "Speaker process for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/* Display version number and terminate. */
static void version(void) {
  xprintf("disorder-speaker version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

/* Return the number of bytes per frame in FORMAT. */
static size_t bytes_per_frame(const ao_sample_format *format) {
  return format->channels * format->bits / 8;
}

/* Find track ID, maybe creating it if not found. */
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
    /* The initial input buffer will be the sample format. */
    t->buffer = (void *)&t->format;
    t->size = sizeof t->format;
  }
  return t;
}

/* Remove track ID (but do not destroy it). */
static struct track *removetrack(const char *id) {
  struct track *t, **tt;

  D(("removetrack %s", id));
  for(tt = &tracks; (t = *tt) && strcmp(id, t->id); tt = &t->next)
    ;
  if(t)
    *tt = t->next;
  return t;
}

/* Destroy a track. */
static void destroy(struct track *t) {
  D(("destroy %s", t->id));
  if(t->fd != -1) xclose(t->fd);
  if(t->buffer != (void *)&t->format) free(t->buffer);
  free(t);
}

/* Notice a new FD. */
static void acquire(struct track *t, int fd) {
  D(("acquire %s %d", t->id, fd));
  if(t->fd != -1)
    xclose(t->fd);
  t->fd = fd;
  nonblock(fd);
}

/* Read data into a sample buffer.  Return 0 on success, -1 on EOF. */
static int fill(struct track *t) {
  size_t where, left;
  int n;

  D(("fill %s: eof=%d used=%zu size=%zu  got_format=%d",
     t->id, t->eof, t->used, t->size, t->got_format));
  if(t->eof) return -1;
  if(t->used < t->size) {
    /* there is room left in the buffer */
    where = (t->start + t->used) % t->size;
    if(t->got_format) {
      /* We are reading audio data, get as much as we can */
      if(where >= t->start) left = t->size - where;
      else left = t->start - where;
    } else
      /* We are still waiting for the format, only get that */
      left = sizeof (ao_sample_format) - t->used;
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
      return -1;
    }
    t->used += n;
    if(!t->got_format && t->used >= sizeof (ao_sample_format)) {
      assert(t->used == sizeof (ao_sample_format));
      /* Check that our assumptions are met. */
      if(t->format.bits & 7)
        fatal(0, "bits per sample not a multiple of 8");
      /* Make a new buffer for audio data. */
      t->size = bytes_per_frame(&t->format) * t->format.rate * BUFFER_SECONDS;
      t->buffer = xmalloc(t->size);
      t->used = 0;
      t->got_format = 1;
      D(("got format for %s", t->id));
    }
  }
  return 0;
}

/* Return true if A and B denote identical libao formats, else false. */
static int formats_equal(const ao_sample_format *a,
                         const ao_sample_format *b) {
  return (a->bits == b->bits
          && a->rate == b->rate
          && a->channels == b->channels
          && a->byte_format == b->byte_format);
}

/* Close the sound device. */
static void idle(void) {
  int  err;

  D(("idle"));
  if(pcm) {
    if((err = snd_pcm_nonblock(pcm, 0)) < 0)
      fatal(0, "error calling snd_pcm_nonblock: %d", err);
    D(("draining pcm"));
    snd_pcm_drain(pcm);
    D(("closing pcm"));
    snd_pcm_close(pcm);
    pcm = 0;
    forceplay = 0;
    D(("released audio device"));
  }
}

/* Abandon the current track */
static void abandon(void) {
  struct speaker_message sm;

  D(("abandon"));
  memset(&sm, 0, sizeof sm);
  sm.type = SM_FINISHED;
  strcpy(sm.id, playing->id);
  speaker_send(1, &sm, 0);
  removetrack(playing->id);
  destroy(playing);
  playing = 0;
  forceplay = 0;
}

/* Make sure the sound device is open and has the right sample format.  Return
 * 0 on success and -1 on error. */
static int activate(void) {
  int err;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_sw_params_t *swparams;
  int sample_format = 0;
  unsigned rate;

  /* If we don't know the format yet we cannot start. */
  if(!playing->got_format) {
    D((" - not got format for %s", playing->id));
    return -1;
  }
  /* If we need to change format then close the current device. */
  if(pcm && !formats_equal(&playing->format, &pcm_format))
     idle();
  if(!pcm) {
    D(("snd_pcm_open"));
    if((err = snd_pcm_open(&pcm,
                           config->device,
                           SND_PCM_STREAM_PLAYBACK,
                           SND_PCM_NONBLOCK))) {
      error(0, "error from snd_pcm_open: %d", err);
      goto error;
    }
    snd_pcm_hw_params_alloca(&hwparams);
    D(("set up hw params"));
    if((err = snd_pcm_hw_params_any(pcm, hwparams)) < 0)
      fatal(0, "error from snd_pcm_hw_params_any: %d", err);
    if((err = snd_pcm_hw_params_set_access(pcm, hwparams,
                                           SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_access: %d", err);
    switch(playing->format.bits) {
    case 8:
      sample_format = SND_PCM_FORMAT_S8;
      break;
    case 16:
      switch(playing->format.byte_format) {
      case AO_FMT_NATIVE: sample_format = SND_PCM_FORMAT_S16; break;
      case AO_FMT_LITTLE: sample_format = SND_PCM_FORMAT_S16_LE; break;
      case AO_FMT_BIG: sample_format = SND_PCM_FORMAT_S16_BE; break;
        error(0, "unrecognized byte format %d", playing->format.byte_format);
        goto fatal;
      }
      break;
    default:
      error(0, "unsupported sample size %d", playing->format.bits);
      goto fatal;
    }
    if((err = snd_pcm_hw_params_set_format(pcm, hwparams,
                                           sample_format)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_format (%d): %d",
            sample_format, err);
      goto fatal;
    }
    rate = playing->format.rate;
    if((err = snd_pcm_hw_params_set_rate_near(pcm, hwparams, &rate, 0)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_rate (%d): %d",
            playing->format.rate, err);
      goto fatal;
    }
    if(rate != (unsigned)playing->format.rate)
      info("want rate %d, got %u", playing->format.rate, rate);
    if((err = snd_pcm_hw_params_set_channels(pcm, hwparams,
                                             playing->format.channels)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
            playing->format.channels, err);
      goto fatal;
    }
    pcm_bufsize = 3 * FRAMES;
    if((err = snd_pcm_hw_params_set_buffer_size_near(pcm, hwparams,
                                                     &pcm_bufsize)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_buffer_size (%d): %d",
            3 * FRAMES, err);
    if(pcm_bufsize != 3 * FRAMES)
      info("asked for PCM buffer of %d frames, got %d",
           3 * FRAMES, (int)pcm_bufsize);
    if((err = snd_pcm_hw_params(pcm, hwparams)) < 0)
      fatal(0, "error calling snd_pcm_hw_params: %d", err);
    D(("set up sw params"));
    snd_pcm_sw_params_alloca(&swparams);
    if((err = snd_pcm_sw_params_current(pcm, swparams)) < 0)
      fatal(0, "error calling snd_pcm_sw_params_current: %d", err);
    if((err = snd_pcm_sw_params_set_avail_min(pcm, swparams, FRAMES)) < 0)
      fatal(0, "error calling snd_pcm_sw_params_set_avail_min %d: %d",
            FRAMES, err);
    if((err = snd_pcm_sw_params(pcm, swparams)) < 0)
      fatal(0, "error calling snd_pcm_sw_params: %d", err);
    pcm_format = playing->format;
    bpf = bytes_per_frame(&pcm_format);
    D(("acquired audio device"));
  }
  return 0;
fatal:
  abandon();
error:
  /* We assume the error is temporary and that we'll retry in a bit. */
  if(pcm) {
    snd_pcm_close(pcm);
    pcm = 0;
  }
  return -1;
}

/* Check to see whether the current track has finished playing */
static void maybe_finished(void) {
  if(playing
     && playing->eof
     && (!playing->got_format
         || playing->used < bytes_per_frame(&playing->format)))
    abandon();
}

static void play(size_t frames) {
  snd_pcm_sframes_t written_frames;
  size_t avail_bytes, avail_frames, written_bytes;
  int err;

  if(activate()) {
    if(playing)
      forceplay = frames;
    else
      forceplay = 0;                    /* Must have called abandon() */
    return;
  }
  D(("play: play %zu/%zu%s %dHz %db %dc",  frames, playing->used / bpf,
     playing->eof ? " EOF" : "",
     playing->format.rate,
     playing->format.bits,
     playing->format.channels));
  /* If we haven't got enough bytes yet wait until we have.  Exception: when
   * we are at eof. */
  if(playing->used < frames * bpf && !playing->eof) {
    forceplay = frames;
    return;
  }
  /* We have got enough data so don't force play again */
  forceplay = 0;
  /* Figure out how many frames there are available to write */
  if(playing->start + playing->used > playing->size)
    avail_bytes = playing->size - playing->start;
  else
    avail_bytes = playing->used;
  avail_frames = avail_bytes / bpf;
  if(avail_frames > frames)
    avail_frames = frames;
  if(!avail_frames)
    return;
  written_frames = snd_pcm_writei(pcm,
                                  playing->buffer + playing->start,
                                  avail_frames);
  D(("actually play %zu frames, wrote %d",
     avail_frames, (int)written_frames));
  if(written_frames < 0) {
    switch(written_frames) {
    case -EPIPE:                        /* underrun */
      error(0, "snd_pcm_writei reports underrun");
      if((err = snd_pcm_prepare(pcm)) < 0)
        fatal(0, "error calling snd_pcm_prepare: %d", err);
      return;
    case -EAGAIN:
      return;
    default:
      fatal(0, "error calling snd_pcm_writei: %d", (int)written_frames);
    }
  }
  written_bytes = written_frames * bpf;
  playing->start += written_bytes;
  playing->used -= written_bytes;
  playing->played += written_frames;
  /* If the pointer is at the end of the buffer (or the buffer is completely
   * empty) wrap it back to the start. */
  if(!playing->used || playing->start == playing->size)
    playing->start = 0;
  frames -= written_frames;
}

/* Notify the server what we're up to. */
static void report(void) {
  struct speaker_message sm;

  if(playing && playing->buffer != (void *)&playing->format) {
    memset(&sm, 0, sizeof sm);
    sm.type = paused ? SM_PAUSED : SM_PLAYING;
    strcpy(sm.id, playing->id);
    sm.data = playing->played / playing->format.rate;
    speaker_send(1, &sm, 0);
  }
  time(&last_report);
}

static int addfd(int fd, int events) {
  if(fdno < NFDS) {
    fds[fdno].fd = fd;
    fds[fdno].events = events;
    return fdno++;
  } else
    return -1;
}

int main(int argc, char **argv) {
  int n, fd, stdin_slot, alsa_slots, alsa_nslots = -1, err;
  unsigned short alsa_revents;
  struct track *t;
  struct speaker_message sm;

  set_progname(argv);
  mem_init(0);
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
  if(getenv("DISORDER_DEBUG_SPEAKER")) debugging = 1;
  /* If stderr is a TTY then log there, otherwise to syslog. */
  if(!isatty(2)) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read()) fatal(0, "cannot read configuration");
  /* ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
  /* set nice value */
  xnice(config->nice_speaker);
  /* change user */
  become_mortal();
  /* make sure we're not root, whatever the config says */
  if(getuid() == 0 || geteuid() == 0) fatal(0, "do not run as root");
  info("started");
  while(getppid() != 1) {
    fdno = 0;
    /* Always ready for commands from the main server. */
    stdin_slot = addfd(0, POLLIN);
    /* Try to read sample data for the currently playing track if there is
     * buffer space. */
    if(playing && !playing->eof && playing->used < playing->size) {
      playing->slot = addfd(playing->fd, POLLIN);
    } else if(playing)
      playing->slot = -1;
    /* If forceplay is set then wait until it succeeds before waiting on the
     * sound device. */
    if(pcm && !forceplay) {
      alsa_slots = fdno;
      alsa_nslots = snd_pcm_poll_descriptors(pcm, &fds[fdno], NFDS - fdno);
      fdno += alsa_nslots;
    } else
      alsa_slots = -1;
    /* If any other tracks don't have a full buffer, try to read sample data
     * from them. */
    for(t = tracks; t; t = t->next)
      if(t != playing) {
        if(!t->eof && t->used < t->size) {
          t->slot = addfd(t->fd,  POLLIN);
        } else
          t->slot = -1;
      }
    /* Wait up to a second before thinking about current state */
    n = poll(fds, fdno, 1000);
    if(n < 0) {
      if(errno == EINTR) continue;
      fatal(errno, "error calling poll");
    }
    /* Play some sound before doing anything else */
    if(alsa_slots != -1) {
      if((err = snd_pcm_poll_descriptors_revents(pcm,
                                                 &fds[alsa_slots],
                                                 alsa_nslots,
                                                 &alsa_revents)) < 0)
        fatal(0, "error calling snd_pcm_poll_descriptors_revents: %d", err);
      if(alsa_revents & POLLOUT)
        play(3 * FRAMES);
    } else {
      /* Some attempt to play must have failed */
      if(playing && !paused)
        play(forceplay);
      else
        forceplay = 0;                  /* just in case */
    }
    /* Perhaps we have a command to process */
    if(fds[stdin_slot].revents & POLLIN) {
      n = speaker_recv(0, &sm, &fd);
      if(n > 0)
	switch(sm.type) {
	case SM_PREPARE:
          D(("SM_PREPARE %s %d", sm.id, fd));
	  if(fd == -1) fatal(0, "got SM_PREPARE but no file descriptor");
	  t = findtrack(sm.id, 1);
          acquire(t, fd);
	  break;
	case SM_PLAY:
          D(("SM_PLAY %s %d", sm.id, fd));
          if(playing) fatal(0, "got SM_PLAY but already playing something");
	  t = findtrack(sm.id, 1);
          if(fd != -1) acquire(t, fd);
          playing = t;
          play(pcm_bufsize);
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
            if(playing)
              play(pcm_bufsize);
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
              speaker_send(1, &sm, 0);
	      playing = 0;
            }
	    destroy(t);
	  } else
	    error(0, "SM_CANCEL for unknown track %s", sm.id);
          report();
	  break;
	case SM_RELOAD:
          D(("SM_RELOAD"));
	  if(config_read()) error(0, "cannot read configuration");
          info("reloaded configuration");
	  break;
	default:
	  error(0, "unknown message type %d", sm.type);
        }
    }
    /* Read in any buffered data */
    for(t = tracks; t; t = t->next)
      if(t->slot != -1 && (fds[t->slot].revents & POLLIN))
         fill(t);
    /* We might be able to play now */
    if(pcm && forceplay && playing && !paused)
      play(forceplay);
    /* Maybe we finished playing a track somewhere in the above */
    maybe_finished();
    /* If we don't need the sound device for now then close it for the benefit
     * of anyone else who wants it. */
    if((!playing || paused) && pcm)
      idle();
    /* If we've not reported out state for a second do so now. */
    if(time(0) > last_report)
      report();
  }
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
/* arch-tag:HQ4ayCGCjeBF97RuRnvcyg */
