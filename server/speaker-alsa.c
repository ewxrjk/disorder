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
/** @file server/speaker-alsa.c
 * @brief Support for @ref BACKEND_ALSA */

#include <config.h>

#if HAVE_ALSA_ASOUNDLIB_H

#include "types.h"

#include <unistd.h>
#include <poll.h>
#include <alsa/asoundlib.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "speaker-protocol.h"
#include "speaker.h"

/** @brief The current PCM handle */
static snd_pcm_t *pcm;

/** @brief Last seen buffer size */
static snd_pcm_uframes_t last_pcm_bufsize;

/** @brief ALSA backend initialization */
static void alsa_init(void) {
  info("selected ALSA backend");
}

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

/** @brief ALSA deactivation */
static void alsa_deactivate(void) {
  if(pcm) {
    int err;
    
    if((err = snd_pcm_nonblock(pcm, 0)) < 0)
      fatal(0, "error calling snd_pcm_nonblock: %d", err);
    D(("draining pcm"));
    snd_pcm_drain(pcm);
    D(("closing pcm"));
    snd_pcm_close(pcm);
    pcm = 0;
    device_state = device_closed;
    D(("released audio device"));
  }
}

/** @brief ALSA backend activation */
static void alsa_activate(void) {
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
    switch(config->sample_format.bits) {
    case 8:
      sample_format = SND_PCM_FORMAT_S8;
      break;
    case 16:
      switch(config->sample_format.endian) {
      case ENDIAN_LITTLE: sample_format = SND_PCM_FORMAT_S16_LE; break;
      case ENDIAN_BIG: sample_format = SND_PCM_FORMAT_S16_BE; break;
      default:
        error(0, "unrecognized byte format %d", config->sample_format.endian);
        goto fatal;
      }
      break;
    default:
      error(0, "unsupported sample size %d", config->sample_format.bits);
      goto fatal;
    }
    if((err = snd_pcm_hw_params_set_format(pcm, hwparams,
                                           sample_format)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_format (%d): %d",
            sample_format, err);
      goto fatal;
    }
    rate = config->sample_format.rate;
    if((err = snd_pcm_hw_params_set_rate_near(pcm, hwparams, &rate, 0)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_rate (%d): %d",
            config->sample_format.rate, err);
      goto fatal;
    }
    if(rate != (unsigned)config->sample_format.rate)
      info("want rate %d, got %u", config->sample_format.rate, rate);
    if((err = snd_pcm_hw_params_set_channels
        (pcm, hwparams, config->sample_format.channels)) < 0) {
      error(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
            config->sample_format.channels, err);
      goto fatal;
    }
    pcm_bufsize = 3 * FRAMES;
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
    D(("acquired audio device"));
    log_params(hwparams, swparams);
    device_state = device_open;
  }
  return;
fatal:
  abandon();
error:
  /* We assume the error is temporary and that we'll retry in a bit. */
  if(pcm) {
    snd_pcm_close(pcm);
    pcm = 0;
    device_state = device_error;
  }
  return;
}

/** @brief Play via ALSA */
static size_t alsa_play(size_t frames) {
  snd_pcm_sframes_t pcm_written_frames;
  int err;
  
  pcm_written_frames = snd_pcm_writei(pcm,
                                      playing->buffer + playing->start,
                                      frames);
  D(("actually play %zu frames, wrote %d",
     frames, (int)pcm_written_frames));
  if(pcm_written_frames < 0) {
    switch(pcm_written_frames) {
    case -EPIPE:                        /* underrun */
      error(0, "snd_pcm_writei reports underrun");
      if((err = snd_pcm_prepare(pcm)) < 0)
        fatal(0, "error calling snd_pcm_prepare: %d", err);
      return 0;
    case -EAGAIN:
      return 0;
    default:
      fatal(0, "error calling snd_pcm_writei: %d",
            (int)pcm_written_frames);
    }
  } else
    return pcm_written_frames;
}

static int alsa_slots, alsa_nslots = -1;

/** @brief Fill in poll fd array for ALSA */
static void alsa_beforepoll(void) {
  /* We send sample data to ALSA as fast as it can accept it, relying on
   * the fact that it has a relatively small buffer to minimize pause
   * latency. */
  int retry = 3, err;
  
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
}

/** @brief Process poll() results for ALSA */
static int alsa_ready(void) {
  int err;

  unsigned short alsa_revents;
  
  if((err = snd_pcm_poll_descriptors_revents(pcm,
                                             &fds[alsa_slots],
                                             alsa_nslots,
                                             &alsa_revents)) < 0)
    fatal(0, "error calling snd_pcm_poll_descriptors_revents: %d", err);
  if(alsa_revents & (POLLOUT | POLLERR))
    return 1;
  else
    return 0;
}

const struct speaker_backend alsa_backend = {
  BACKEND_ALSA,
  0,
  alsa_init,
  alsa_activate,
  alsa_play,
  alsa_deactivate,
  alsa_beforepoll,
  alsa_ready
};

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
