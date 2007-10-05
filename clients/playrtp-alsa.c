/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file clients/playrtp-alsa.c
 * @brief RTP player - ALSA support
 */

#include <config.h>

#if HAVE_ALSA_ASOUNDLIB_H 
#include "types.h"

#include <poll.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <pthread.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "heap.h"
#include "playrtp.h"

/** @brief PCM handle */
static snd_pcm_t *pcm;

/** @brief True when @ref pcm is up and running */
static int playrtp_alsa_prepared = 1;

static void playrtp_alsa_init(void) {
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_sw_params_t *swparams;
  /* Only support one format for now */
  const int sample_format = SND_PCM_FORMAT_S16_BE;
  unsigned rate = 44100;
  const int channels = 2;
  const int samplesize = channels * sizeof(uint16_t);
  snd_pcm_uframes_t pcm_bufsize = MAXSAMPLES * samplesize * 3;
  /* If we can write more than this many samples we'll get a wakeup */
  const int avail_min = 256;
  int err;
  
  /* Open ALSA */
  if((err = snd_pcm_open(&pcm,
                         device ? device : "default",
                         SND_PCM_STREAM_PLAYBACK,
                         SND_PCM_NONBLOCK)))
    fatal(0, "error from snd_pcm_open: %d", err);
  /* Set up 'hardware' parameters */
  snd_pcm_hw_params_alloca(&hwparams);
  if((err = snd_pcm_hw_params_any(pcm, hwparams)) < 0)
    fatal(0, "error from snd_pcm_hw_params_any: %d", err);
  if((err = snd_pcm_hw_params_set_access(pcm, hwparams,
                                         SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_access: %d", err);
  if((err = snd_pcm_hw_params_set_format(pcm, hwparams,
                                         sample_format)) < 0)
    
    fatal(0, "error from snd_pcm_hw_params_set_format (%d): %d",
          sample_format, err);
  if((err = snd_pcm_hw_params_set_rate_near(pcm, hwparams, &rate, 0)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_rate (%d): %d",
          rate, err);
  if((err = snd_pcm_hw_params_set_channels(pcm, hwparams,
                                           channels)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
          channels, err);
  if((err = snd_pcm_hw_params_set_buffer_size_near(pcm, hwparams,
                                                   &pcm_bufsize)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_buffer_size (%d): %d",
          MAXSAMPLES * samplesize * 3, err);
  if((err = snd_pcm_hw_params(pcm, hwparams)) < 0)
    fatal(0, "error calling snd_pcm_hw_params: %d", err);
  /* Set up 'software' parameters */
  snd_pcm_sw_params_alloca(&swparams);
  if((err = snd_pcm_sw_params_current(pcm, swparams)) < 0)
    fatal(0, "error calling snd_pcm_sw_params_current: %d", err);
  if((err = snd_pcm_sw_params_set_avail_min(pcm, swparams, avail_min)) < 0)
    fatal(0, "error calling snd_pcm_sw_params_set_avail_min %d: %d",
          avail_min, err);
  if((err = snd_pcm_sw_params(pcm, swparams)) < 0)
    fatal(0, "error calling snd_pcm_sw_params: %d", err);
}

/** @brief Wait until ALSA wants some audio */
static void wait_alsa(void) {
  struct pollfd fds[64];
  int nfds, err;
  unsigned short events;

  for(;;) {
    do {
      if((nfds = snd_pcm_poll_descriptors(pcm,
                                          fds, sizeof fds / sizeof *fds)) < 0)
        fatal(0, "error calling snd_pcm_poll_descriptors: %d", nfds);
    } while(poll(fds, nfds, -1) < 0 && errno == EINTR);
    if((err = snd_pcm_poll_descriptors_revents(pcm, fds, nfds, &events)))
      fatal(0, "error calling snd_pcm_poll_descriptors_revents: %d", err);
    if(events & POLLOUT)
      return;
  }
}

/** @brief Play some sound via ALSA
 * @param s Pointer to sample data
 * @param n Number of samples
 * @return 0 on success, -1 on non-fatal error
 */
static int playrtp_alsa_writei(const void *s, size_t n) {
  /* Do the write */
  const snd_pcm_sframes_t frames_written = snd_pcm_writei(pcm, s, n / 2);
  if(frames_written < 0) {
    /* Something went wrong */
    switch(frames_written) {
    case -EAGAIN:
      return 0;
    case -EPIPE:
      error(0, "error calling snd_pcm_writei: %ld",
            (long)frames_written);
      return -1;
    default:
      fatal(0, "error calling snd_pcm_writei: %ld",
            (long)frames_written);
    }
  } else {
    /* Success */
    next_timestamp += frames_written * 2;
    return 0;
  }
}

/** @brief Play the relevant part of a packet
 * @param p Packet to play
 * @return 0 on success, -1 on non-fatal error
 */
static int playrtp_alsa_play(const struct packet *p) {
  return playrtp_alsa_writei(p->samples_raw + next_timestamp - p->timestamp,
                             (p->timestamp + p->nsamples) - next_timestamp);
}

/** @brief Play some silence
 * @param p Next packet or NULL
 * @return 0 on success, -1 on non-fatal error
 */
static int playrtp_alsa_infill(const struct packet *p) {
  static const uint16_t zeros[INFILL_SAMPLES];
  size_t samples_available = INFILL_SAMPLES;

  if(p && samples_available > p->timestamp - next_timestamp)
    samples_available = p->timestamp - next_timestamp;
  return playrtp_alsa_writei(zeros, samples_available);
}

static void playrtp_alsa_enable(void){
  int err;

  if(!playrtp_alsa_prepared) {
    if((err = snd_pcm_prepare(pcm)))
      fatal(0, "error calling snd_pcm_prepare: %d", err);
    playrtp_alsa_prepared = 1;
  }
}

/** @brief Reset ALSA state after we lost synchronization */
static void playrtp_alsa_disable(int hard_reset) {
  int err;

  if((err = snd_pcm_nonblock(pcm, 0)))
    fatal(0, "error calling snd_pcm_nonblock: %d", err);
  if(hard_reset) {
    if((err = snd_pcm_drop(pcm)))
      fatal(0, "error calling snd_pcm_drop: %d", err);
  } else
    if((err = snd_pcm_drain(pcm)))
      fatal(0, "error calling snd_pcm_drain: %d", err);
  if((err = snd_pcm_nonblock(pcm, 1)))
    fatal(0, "error calling snd_pcm_nonblock: %d", err);
  playrtp_alsa_prepared = 0;
}

void playrtp_alsa(void) {
  int escape;
  const struct packet *p;

  playrtp_alsa_init();
  pthread_mutex_lock(&lock);
  for(;;) {
    /* Wait for the buffer to fill up a bit */
    playrtp_fill_buffer();
    playrtp_alsa_enable();
    escape = 0;
    info("Playing...");
    /* Keep playing until the buffer empties out, or ALSA tells us to get
     * lost */
    while((nsamples >= minbuffer
	   || (nsamples > 0
	       && contains(pheap_first(&packets), next_timestamp)))
	  && !escape) {
      /* Wait for ALSA to ask us for more data */
      pthread_mutex_unlock(&lock);
      wait_alsa();
      pthread_mutex_lock(&lock);
      /* ALSA is ready for more data, find something to play */
      p = playrtp_next_packet();
      /* Play it or play some silence */
      if(contains(p, next_timestamp))
	escape = playrtp_alsa_play(p);
      else
	escape = playrtp_alsa_infill(p);
    }
    active = 0;
    /* We stop playing for a bit until the buffer re-fills */
    pthread_mutex_unlock(&lock);
    playrtp_alsa_disable(escape);
    pthread_mutex_lock(&lock);
  }
}

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
