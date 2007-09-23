/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
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
 * @brief Speaker processs
 *
 * This program is responsible for transmitting a single coherent audio stream
 * to its destination (over the network, to some sound API, to some
 * subprocess).  It receives connections from decoders via file descriptor
 * passing from the main server and plays them in the right order.
 *
 * For the <a href="http://www.alsa-project.org/">ALSA</a> API, 8- and 16- bit
 * stereo and mono are supported, with any sample rate (within the limits that
 * ALSA can deal with.)
 *
 * When communicating with a subprocess, <a
 * href="http://sox.sourceforge.net/">sox</a> is invoked to convert the inbound
 * data to a single consistent format.  The same applies for network (RTP)
 * play, though in that case currently only 44.1KHz 16-bit stereo is supported.
 *
 * The inbound data starts with a structure defining the data format.  Note
 * that this is NOT portable between different platforms or even necessarily
 * between versions; the speaker is assumed to be built from the same source
 * and run on the same host as the main server.
 *
 * This program deliberately does not use the garbage collector even though it
 * might be convenient to do so.  This is for two reasons.  Firstly some sound
 * APIs use thread threads and we do not want to have to deal with potential
 * interactions between threading and garbage collection.  Secondly this
 * process needs to be able to respond quickly and this is not compatible with
 * the collector hanging the program even relatively briefly.
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
#include <sys/socket.h>
#include <netdb.h>
#include <gcrypt.h>
#include <sys/uio.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "mem.h"
#include "speaker.h"
#include "user.h"
#include "addr.h"
#include "timeval.h"
#include "rtp.h"

#if API_ALSA
#include <alsa/asoundlib.h>
#endif

#ifdef WORDS_BIGENDIAN
# define MACHINE_AO_FMT AO_FMT_BIG
#else
# define MACHINE_AO_FMT AO_FMT_LITTLE
#endif

/** @brief How many seconds of input to buffer
 *
 * While any given connection has this much audio buffered, no more reads will
 * be issued for that connection.  The decoder will have to wait.
 */
#define BUFFER_SECONDS 5

#define FRAMES 4096                     /* Frame batch size */

/** @brief Bytes to send per network packet
 *
 * Don't make this too big or arithmetic will start to overflow.
 */
#define NETWORK_BYTES (1024+sizeof(struct rtp_header))

/** @brief Maximum RTP playahead (ms) */
#define RTP_AHEAD_MS 1000

/** @brief Maximum number of FDs to poll for */
#define NFDS 256

/** @brief Track structure
 *
 * Known tracks are kept in a linked list.  Usually there will be at most two
 * of these but rearranging the queue can cause there to be more.
 */
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
static ao_sample_format pcm_format;     /* current format if aodev != 0 */
static size_t bpf;                      /* bytes per frame */
static struct pollfd fds[NFDS];         /* if we need more than that */
static int fdno;                        /* fd number */
static size_t bufsize;                  /* buffer size */
#if API_ALSA
static snd_pcm_t *pcm;                  /* current pcm handle */
static snd_pcm_uframes_t last_pcm_bufsize; /* last seen buffer size */
#endif
static int ready;                       /* ready to send audio */
static int forceplay;                   /* frames to force play */
static int cmdfd = -1;                  /* child process input */
static int bfd = -1;                    /* broadcast FD */

/** @brief RTP timestamp
 *
 * This counts the number of samples played (NB not the number of frames
 * played).
 *
 * The timestamp in the packet header is only 32 bits wide.  With 44100Hz
 * stereo, that only gives about half a day before wrapping, which is not
 * particularly convenient for certain debugging purposes.  Therefore the
 * timestamp is maintained as a 64-bit integer, giving around six million years
 * before wrapping, and truncated to 32 bits when transmitting.
 */
static uint64_t rtp_time;

/** @brief RTP base timestamp
 *
 * This is the real time correspoding to an @ref rtp_time of 0.  It is used
 * to recalculate the timestamp after idle periods.
 */
static struct timeval rtp_time_0;

static uint16_t rtp_seq;                /* frame sequence number */
static uint32_t rtp_id;                 /* RTP SSRC */
static int idled;                       /* set when idled */
static int audio_errors;                /* audio error counter */

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

/** @brief Return the number of bytes per frame in @p format */
static size_t bytes_per_frame(const ao_sample_format *format) {
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
    /* The initial input buffer will be the sample format. */
    t->buffer = (void *)&t->format;
    t->size = sizeof t->format;
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
  if(t->buffer != (void *)&t->format) free(t->buffer);
  free(t);
}

/** @brief Notice a new connection */
static void acquire(struct track *t, int fd) {
  D(("acquire %s %d", t->id, fd));
  if(t->fd != -1)
    xclose(t->fd);
  t->fd = fd;
  nonblock(fd);
}

/** @brief Return true if A and B denote identical libao formats, else false */
static int formats_equal(const ao_sample_format *a,
                         const ao_sample_format *b) {
  return (a->bits == b->bits
          && a->rate == b->rate
          && a->channels == b->channels
          && a->byte_format == b->byte_format);
}

/** @brief Compute arguments to sox */
static void soxargs(const char ***pp, char **qq, ao_sample_format *ao) {
  int n;

  *(*pp)++ = "-t.raw";
  *(*pp)++ = "-s";
  *(*pp)++ = *qq; n = sprintf(*qq, "-r%d", ao->rate); *qq += n + 1;
  *(*pp)++ = *qq; n = sprintf(*qq, "-c%d", ao->channels); *qq += n + 1;
  /* sox 12.17.9 insists on -b etc; CVS sox insists on -<n> etc; both are
   * deployed! */
  switch(config->sox_generation) {
  case 0:
    if(ao->bits != 8
       && ao->byte_format != AO_FMT_NATIVE
       && ao->byte_format != MACHINE_AO_FMT) {
      *(*pp)++ = "-x";
    }
    switch(ao->bits) {
    case 8: *(*pp)++ = "-b"; break;
    case 16: *(*pp)++ = "-w"; break;
    case 32: *(*pp)++ = "-l"; break;
    case 64: *(*pp)++ = "-d"; break;
    default: fatal(0, "cannot handle sample size %d", (int)ao->bits);
    }
    break;
  case 1:
    switch(ao->byte_format) {
    case AO_FMT_NATIVE: break;
    case AO_FMT_BIG: *(*pp)++ = "-B"; break;
    case AO_FMT_LITTLE: *(*pp)++ = "-L"; break;
    }
    *(*pp)++ = *qq; n = sprintf(*qq, "-%d", ao->bits/8); *qq += n + 1;
    break;
  }
}

/** @brief Enable format translation
 *
 * If necessary, replaces a tracks inbound file descriptor with one connected
 * to a sox invocation, which performs the required translation.
 */
static void enable_translation(struct track *t) {
  switch(config->speaker_backend) {
  case BACKEND_COMMAND:
  case BACKEND_NETWORK:
    /* These backends need a specific sample format */
    break;
  case BACKEND_ALSA:
    /* ALSA can cope */
    return;
  }
  if(!formats_equal(&t->format, &config->sample_format)) {
    char argbuf[1024], *q = argbuf;
    const char *av[18], **pp = av;
    int soxpipe[2];
    pid_t soxkid;

    *pp++ = "sox";
    soxargs(&pp, &q, &t->format);
    *pp++ = "-";
    soxargs(&pp, &q, &config->sample_format);
    *pp++ = "-";
    *pp++ = 0;
    if(debugging) {
      for(pp = av; *pp; pp++)
        D(("sox arg[%d] = %s", pp - av, *pp));
      D(("end args"));
    }
    xpipe(soxpipe);
    soxkid = xfork();
    if(soxkid == 0) {
      signal(SIGPIPE, SIG_DFL);
      xdup2(t->fd, 0);
      xdup2(soxpipe[1], 1);
      fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);
      close(soxpipe[0]);
      close(soxpipe[1]);
      close(t->fd);
      execvp("sox", (char **)av);
      _exit(1);
    }
    D(("forking sox for format conversion (kid = %d)", soxkid));
    close(t->fd);
    close(soxpipe[1]);
    t->fd = soxpipe[0];
    t->format = config->sample_format;
    ready = 0;
  }
}

/** @brief Read data into a sample buffer
 * @param t Pointer to track
 * @return 0 on success, -1 on EOF
 *
 * This is effectively the read callback on @c t->fd.
 */
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
      /* If the input format is unsuitable, arrange to translate it */
      enable_translation(t);
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

/** @brief Close the sound device */
static void idle(void) {
  D(("idle"));
#if API_ALSA
  if(config->speaker_backend == BACKEND_ALSA && pcm) {
    int  err;

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
#endif
  idled = 1;
  ready = 0;
}

/** @brief Abandon the current track */
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

#if API_ALSA
/** @brief Log ALSA parameters */
static void log_params(snd_pcm_hw_params_t *hwparams,
                       snd_pcm_sw_params_t *swparams) {
  snd_pcm_uframes_t f;
  unsigned u;

  return;                               /* too verbose */
  if(hwparams) {
    /* TODO */
  }
  if(swparams) {
    snd_pcm_sw_params_get_silence_size(swparams, &f);
    info("sw silence_size=%lu", (unsigned long)f);
    snd_pcm_sw_params_get_silence_threshold(swparams, &f);
    info("sw silence_threshold=%lu", (unsigned long)f);
    snd_pcm_sw_params_get_sleep_min(swparams, &u);
    info("sw sleep_min=%lu", (unsigned long)u);
    snd_pcm_sw_params_get_start_threshold(swparams, &f);
    info("sw start_threshold=%lu", (unsigned long)f);
    snd_pcm_sw_params_get_stop_threshold(swparams, &f);
    info("sw stop_threshold=%lu", (unsigned long)f);
    snd_pcm_sw_params_get_xfer_align(swparams, &f);
    info("sw xfer_align=%lu", (unsigned long)f);
  }
}
#endif

/** @brief Enable sound output
 *
 * Makes sure the sound device is open and has the right sample format.  Return
 * 0 on success and -1 on error.
 */
static int activate(void) {
  /* If we don't know the format yet we cannot start. */
  if(!playing->got_format) {
    D((" - not got format for %s", playing->id));
    return -1;
  }
  switch(config->speaker_backend) {
  case BACKEND_COMMAND:
  case BACKEND_NETWORK:
    if(!ready) {
      pcm_format = config->sample_format;
      bufsize = 3 * FRAMES;
      bpf = bytes_per_frame(&config->sample_format);
      D(("acquired audio device"));
      ready = 1;
    }
    return 0;
  case BACKEND_ALSA:
#if API_ALSA
    /* If we need to change format then close the current device. */
    if(pcm && !formats_equal(&playing->format, &pcm_format))
      idle();
    if(!pcm) {
      snd_pcm_hw_params_t *hwparams;
      snd_pcm_sw_params_t *swparams;
      snd_pcm_uframes_t pcm_bufsize;
      int err;
      int sample_format = 0;
      unsigned rate;

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
      bufsize = 3 * FRAMES;
      pcm_bufsize = bufsize;
      if((err = snd_pcm_hw_params_set_buffer_size_near(pcm, hwparams,
                                                       &pcm_bufsize)) < 0)
        fatal(0, "error from snd_pcm_hw_params_set_buffer_size (%d): %d",
              3 * FRAMES, err);
      if(pcm_bufsize != 3 * FRAMES && pcm_bufsize != last_pcm_bufsize)
        info("asked for PCM buffer of %d frames, got %d",
             3 * FRAMES, (int)pcm_bufsize);
      last_pcm_bufsize = pcm_bufsize;
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
      log_params(hwparams, swparams);
      ready = 1;
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
#endif
  default:
    assert(!"reached");
  }
}

/* Check to see whether the current track has finished playing */
static void maybe_finished(void) {
  if(playing
     && playing->eof
     && (!playing->got_format
         || playing->used < bytes_per_frame(&playing->format)))
    abandon();
}

static void fork_cmd(void) {
  pid_t cmdpid;
  int pfd[2];
  if(cmdfd != -1) close(cmdfd);
  xpipe(pfd);
  cmdpid = xfork();
  if(!cmdpid) {
    signal(SIGPIPE, SIG_DFL);
    xdup2(pfd[0], 0);
    close(pfd[0]);
    close(pfd[1]);
    execl("/bin/sh", "sh", "-c", config->speaker_command, (char *)0);
    fatal(errno, "error execing /bin/sh");
  }
  close(pfd[0]);
  cmdfd = pfd[1];
  D(("forked cmd %d, fd = %d", cmdpid, cmdfd));
}

static void play(size_t frames) {
  size_t avail_bytes, write_bytes, written_frames;
  ssize_t written_bytes;
  struct rtp_header header;
  struct iovec vec[2];

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

  switch(config->speaker_backend) {
#if API_ALSA
  case BACKEND_ALSA: {
    snd_pcm_sframes_t pcm_written_frames;
    size_t avail_frames;
    int err;

    avail_frames = avail_bytes / bpf;
    if(avail_frames > frames)
      avail_frames = frames;
    if(!avail_frames)
      return;
    pcm_written_frames = snd_pcm_writei(pcm,
                                        playing->buffer + playing->start,
                                        avail_frames);
    D(("actually play %zu frames, wrote %d",
       avail_frames, (int)pcm_written_frames));
    if(pcm_written_frames < 0) {
      switch(pcm_written_frames) {
        case -EPIPE:                        /* underrun */
          error(0, "snd_pcm_writei reports underrun");
          if((err = snd_pcm_prepare(pcm)) < 0)
            fatal(0, "error calling snd_pcm_prepare: %d", err);
          return;
        case -EAGAIN:
          return;
        default:
          fatal(0, "error calling snd_pcm_writei: %d",
                (int)pcm_written_frames);
      }
    }
    written_frames = pcm_written_frames;
    written_bytes = written_frames * bpf;
    break;
  }
#endif
  case BACKEND_COMMAND:
    if(avail_bytes > frames * bpf)
      avail_bytes = frames * bpf;
    written_bytes = write(cmdfd, playing->buffer + playing->start,
                          avail_bytes);
    D(("actually play %zu bytes, wrote %d",
       avail_bytes, (int)written_bytes));
    if(written_bytes < 0) {
      switch(errno) {
        case EPIPE:
          error(0, "hmm, command died; trying another");
          fork_cmd();
          return;
        case EAGAIN:
          return;
      }
    }
    written_frames = written_bytes / bpf; /* good enough */
    break;
  case BACKEND_NETWORK:
    /* We transmit using RTP (RFC3550) and attempt to conform to the internet
     * AVT profile (RFC3551). */

    if(idled) {
      /* There may have been a gap.  Fix up the RTP time accordingly. */
      struct timeval now;
      uint64_t delta;
      uint64_t target_rtp_time;

      /* Find the current time */
      xgettimeofday(&now, 0);
      /* Find the number of microseconds elapsed since rtp_time=0 */
      delta = tvsub_us(now, rtp_time_0);
      assert(delta <= UINT64_MAX / 88200);
      target_rtp_time = (delta * playing->format.rate
                               * playing->format.channels) / 1000000;
      /* Overflows at ~6 years uptime with 44100Hz stereo */

      /* rtp_time is the number of samples we've played.  NB that we play
       * RTP_AHEAD_MS ahead of ourselves, so it may legitimately be ahead of
       * the value we deduce from time comparison.
       *
       * Suppose we have 1s track started at t=0, and another track begins to
       * play at t=2s.  Suppose RTP_AHEAD_MS=1000 and 44100Hz stereo.  In that
       * case we'll send 1s of audio as fast as we can, giving rtp_time=88200.
       * rtp_time stops at this point.
       *
       * At t=2s we'll have calculated target_rtp_time=176400.  In this case we
       * set rtp_time=176400 and the player can correctly conclude that it
       * should leave 1s between the tracks.
       *
       * Suppose instead that the second track arrives at t=0.5s, and that
       * we've managed to transmit the whole of the first track already.  We'll
       * have target_rtp_time=44100.
       *
       * The desired behaviour is to play the second track back to back with
       * first.  In this case therefore we do not modify rtp_time.
       *
       * Is it ever right to reduce rtp_time?  No; for that would imply
       * transmitting packets with overlapping timestamp ranges, which does not
       * make sense.
       */
      if(target_rtp_time > rtp_time) {
        /* More time has elapsed than we've transmitted samples.  That implies
         * we've been 'sending' silence.  */
        info("advancing rtp_time by %"PRIu64" samples",
             target_rtp_time - rtp_time);
        rtp_time = target_rtp_time;
      } else if(target_rtp_time < rtp_time) {
        const int64_t samples_ahead = ((uint64_t)RTP_AHEAD_MS
                                           * config->sample_format.rate
                                           * config->sample_format.channels
                                           / 1000);
        
        if(target_rtp_time + samples_ahead < rtp_time) {
          info("reversing rtp_time by %"PRIu64" samples",
               rtp_time - target_rtp_time);
        }
      }
    }
    header.vpxcc = 2 << 6;              /* V=2, P=0, X=0, CC=0 */
    header.seq = htons(rtp_seq++);
    header.timestamp = htonl((uint32_t)rtp_time);
    header.ssrc = rtp_id;
    header.mpt = (idled ? 0x80 : 0x00) | 10;
    /* 10 = L16 = 16-bit x 2 x 44100KHz.  We ought to deduce this value from
     * the sample rate (in a library somewhere so that configuration.c can rule
     * out invalid rates).
     */
    idled = 0;
    if(avail_bytes > NETWORK_BYTES - sizeof header) {
      avail_bytes = NETWORK_BYTES - sizeof header;
      /* Always send a whole number of frames */
      avail_bytes -= avail_bytes % bpf;
    }
    /* "The RTP clock rate used for generating the RTP timestamp is independent
     * of the number of channels and the encoding; it equals the number of
     * sampling periods per second.  For N-channel encodings, each sampling
     * period (say, 1/8000 of a second) generates N samples. (This terminology
     * is standard, but somewhat confusing, as the total number of samples
     * generated per second is then the sampling rate times the channel
     * count.)"
     */
    write_bytes = avail_bytes;
    if(write_bytes) {
      vec[0].iov_base = (void *)&header;
      vec[0].iov_len = sizeof header;
      vec[1].iov_base = playing->buffer + playing->start;
      vec[1].iov_len = avail_bytes;
      do {
        written_bytes = writev(bfd,
                               vec,
                               2);
      } while(written_bytes < 0 && errno == EINTR);
      if(written_bytes < 0) {
        error(errno, "error transmitting audio data");
        ++audio_errors;
        if(audio_errors == 10)
          fatal(0, "too many audio errors");
      return;
      }
    } else
    audio_errors /= 2;
    written_bytes = avail_bytes;
    written_frames = written_bytes / bpf;
    /* Advance RTP's notion of the time */
    rtp_time += written_frames * playing->format.channels;
    break;
  default:
    assert(!"reached");
  }
  /* written_bytes and written_frames had better both be set and correct by
   * this point */
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

static void reap(int __attribute__((unused)) sig) {
  pid_t cmdpid;
  int st;

  do
    cmdpid = waitpid(-1, &st, WNOHANG);
  while(cmdpid > 0);
  signal(SIGCHLD, reap);
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
  int n, fd, stdin_slot, alsa_slots, cmdfd_slot, bfd_slot, poke, timeout;
  struct track *t;
  struct speaker_message sm;
  struct addrinfo *res, *sres;
  static const struct addrinfo pref = {
    0,
    PF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    0,
    0,
    0,
    0
  };
  static const struct addrinfo prefbind = {
    AI_PASSIVE,
    PF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    0,
    0,
    0,
    0
  };
  static const int one = 1;
  int sndbuf, target_sndbuf = 131072;
  socklen_t len;
  char *sockname, *ssockname;
#if API_ALSA
  int alsa_nslots = -1, err;
#endif

  set_progname(argv);
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
  /* reap kids */
  signal(SIGCHLD, reap);
  /* set nice value */
  xnice(config->nice_speaker);
  /* change user */
  become_mortal();
  /* make sure we're not root, whatever the config says */
  if(getuid() == 0 || geteuid() == 0) fatal(0, "do not run as root");
  switch(config->speaker_backend) {
  case BACKEND_ALSA:
    info("selected ALSA backend");
  case BACKEND_COMMAND:
    info("selected command backend");
    fork_cmd();
    break;
  case BACKEND_NETWORK:
    res = get_address(&config->broadcast, &pref, &sockname);
    if(!res) return -1;
    if(config->broadcast_from.n) {
      sres = get_address(&config->broadcast_from, &prefbind, &ssockname);
      if(!sres) return -1;
    } else
      sres = 0;
    if((bfd = socket(res->ai_family,
                     res->ai_socktype,
                     res->ai_protocol)) < 0)
      fatal(errno, "error creating broadcast socket");
    if(setsockopt(bfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one) < 0)
      fatal(errno, "error setting SO_BROADCAST on broadcast socket");
    len = sizeof sndbuf;
    if(getsockopt(bfd, SOL_SOCKET, SO_SNDBUF,
                  &sndbuf, &len) < 0)
      fatal(errno, "error getting SO_SNDBUF");
    if(setsockopt(bfd, SOL_SOCKET, SO_SNDBUF,
                  &target_sndbuf, sizeof target_sndbuf) < 0)
      error(errno, "error setting SO_SNDBUF to %d", target_sndbuf);
    else
      info("changed socket send buffer size from %d to %d",
           sndbuf, target_sndbuf);
    /* We might well want to set additional broadcast- or multicast-related
     * options here */
    if(sres && bind(bfd, sres->ai_addr, sres->ai_addrlen) < 0)
      fatal(errno, "error binding broadcast socket to %s", ssockname);
    if(connect(bfd, res->ai_addr, res->ai_addrlen) < 0)
      fatal(errno, "error connecting broadcast socket to %s", sockname);
    /* Select an SSRC */
    gcry_randomize(&rtp_id, sizeof rtp_id, GCRY_STRONG_RANDOM);
    info("selected network backend, sending to %s", sockname);
    if(config->sample_format.byte_format != AO_FMT_BIG) {
      info("forcing big-endian sample format");
      config->sample_format.byte_format = AO_FMT_BIG;
    }
    break;
  default:
    fatal(0, "unknown backend %d", config->speaker_backend);
  }
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
    alsa_slots = -1;
    cmdfd_slot = -1;
    bfd_slot = -1;
    /* By default we will wait up to a second before thinking about current
     * state. */
    timeout = 1000;
    if(ready && !forceplay) {
      switch(config->speaker_backend) {
      case BACKEND_COMMAND:
        /* We send sample data to the subprocess as fast as it can accept it.
         * This isn't ideal as pause latency can be very high as a result. */
        if(cmdfd >= 0)
          cmdfd_slot = addfd(cmdfd, POLLOUT);
        break;
      case BACKEND_NETWORK: {
        struct timeval now;
        uint64_t target_us;
        uint64_t target_rtp_time;
        const int64_t samples_ahead = ((uint64_t)RTP_AHEAD_MS
                                           * config->sample_format.rate
                                           * config->sample_format.channels
                                           / 1000);
#if 0
        static unsigned logit;
#endif

        /* If we're starting then initialize the base time */
        if(!rtp_time)
          xgettimeofday(&rtp_time_0, 0);
        /* We send audio data whenever we get RTP_AHEAD seconds or more
         * behind */
        xgettimeofday(&now, 0);
        target_us = tvsub_us(now, rtp_time_0);
        assert(target_us <= UINT64_MAX / 88200);
        target_rtp_time = (target_us * config->sample_format.rate
                                     * config->sample_format.channels)

                          / 1000000;
#if 0
        /* TODO remove logging guff */
        if(!(logit++ & 1023))
          info("rtp_time %llu target %llu difference %lld [%lld]", 
               rtp_time, target_rtp_time,
               rtp_time - target_rtp_time,
               samples_ahead);
#endif
        if((int64_t)(rtp_time - target_rtp_time) < samples_ahead)
          bfd_slot = addfd(bfd, POLLOUT);
        break;
      }
#if API_ALSA
      case BACKEND_ALSA: {
        /* We send sample data to ALSA as fast as it can accept it, relying on
         * the fact that it has a relatively small buffer to minimize pause
         * latency. */
        int retry = 3;
        
        alsa_slots = fdno;
        do {
          retry = 0;
          alsa_nslots = snd_pcm_poll_descriptors(pcm, &fds[fdno], NFDS - fdno);
          if((alsa_nslots <= 0
              || !(fds[alsa_slots].events & POLLOUT))
             && snd_pcm_state(pcm) == SND_PCM_STATE_XRUN) {
            error(0, "underrun detected after call to snd_pcm_poll_descriptors()");
            if((err = snd_pcm_prepare(pcm)))
              fatal(0, "error calling snd_pcm_prepare: %d", err);
          } else
            break;
        } while(retry-- > 0);
        if(alsa_nslots >= 0)
          fdno += alsa_nslots;
        break;
      }
#endif
      default:
        assert(!"unknown backend");
      }
    }
    /* If any other tracks don't have a full buffer, try to read sample data
     * from them. */
    for(t = tracks; t; t = t->next)
      if(t != playing) {
        if(!t->eof && t->used < t->size) {
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
    poke = 0;
    switch(config->speaker_backend) {
#if API_ALSA
    case BACKEND_ALSA:
      if(alsa_slots != -1) {
        unsigned short alsa_revents;
        
        if((err = snd_pcm_poll_descriptors_revents(pcm,
                                                   &fds[alsa_slots],
                                                   alsa_nslots,
                                                   &alsa_revents)) < 0)
          fatal(0, "error calling snd_pcm_poll_descriptors_revents: %d", err);
        if(alsa_revents & (POLLOUT | POLLERR))
          play(3 * FRAMES);
      } else
        poke = 1;
      break;
#endif
    case BACKEND_COMMAND:
      if(cmdfd_slot != -1) {
        if(fds[cmdfd_slot].revents & (POLLOUT | POLLERR))
          play(3 * FRAMES);
      } else
        poke = 1;
      break;
    case BACKEND_NETWORK:
      if(bfd_slot != -1) {
        if(fds[bfd_slot].revents & (POLLOUT | POLLERR))
          play(3 * FRAMES);
      } else
        poke = 1;
      break;
    }
    if(poke) {
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
          play(bufsize);
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
              play(bufsize);
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
      if(t->slot != -1 && (fds[t->slot].revents & (POLLIN | POLLHUP)))
         fill(t);
    /* We might be able to play now */
    if(ready && forceplay && playing && !paused)
      play(forceplay);
    /* Maybe we finished playing a track somewhere in the above */
    maybe_finished();
    /* If we don't need the sound device for now then close it for the benefit
     * of anyone else who wants it. */
    if((!playing || paused) && ready)
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
