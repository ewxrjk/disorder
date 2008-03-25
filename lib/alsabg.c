/*
 * This file is part of DisOrder.
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
/** @file alsabg.c
 * @brief Background-thread interface to ALSA
 */

#include <config.h>

#if HAVE_ALSA_ASOUNDLIB_H
#include "types.h"

#include <alsa/asoundlib.h>
#include <pthread.h>

#include "alsabg.h"
#include "log.h"

/** @brief Output handle */
static snd_pcm_t *pcm;

/** @brief Called to get audio data */
static alsa_bg_supply *supplyfn;

static pthread_t alsa_bg_collect_tid, alsa_bg_play_tid;

/** @brief Set to shut down the background threads */
static int alsa_bg_shutdown = 0;

/** @brief Number of channels (samples per frame) */
#define CHANNELS 2

/** @brief Number of bytes per samples */
#define BYTES_PER_SAMPLE 2

/** @brief Number of bytes per frame */
#define BYTES_PER_FRAME (CHANNELS * BYTES_PER_SAMPLE)

/** @brief Buffer size in bytes */
#define BUFFER_BYTES 65536

/** @brief Buffer size in frames */
#define BUFFER_FRAMES (BUFFER_BYTES / BYTES_PER_FRAME)

/** @brief Buffer size in samples */
#define BUFFER_SAMPLES (BUFFER_BYTES / BYTES_PER_SAMPLE)

/** @brief Audio buffer */
static uint8_t alsa_bg_buffer[BUFFER_BYTES];

/** @brief First playable byte in audio buffer */
static unsigned alsa_bg_start;

/** @brief Number of playable bytes in audio buffer */
static unsigned alsa_bg_count;

/** @brief Current enable status */
static int alsa_bg_enabled;

/** @brief Lock protecting audio buffer pointers */
static pthread_mutex_t alsa_bg_lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Signaled when buffer contents changes */
static pthread_cond_t alsa_bg_cond = PTHREAD_COND_INITIALIZER;

/** @brief Call a pthread_ function and fatal() on exit */
#define ep(x) do {				\
  int prc;					\
						\
  if((prc = (x)))				\
    fatal(prc, "%s", #x);			\
} while(0)

/** @brief Data collection thread
 *
 * This thread collects audio data to play and stores it in the ring
 * buffer.
 */
static void *alsa_bg_collect(void attribute((unused)) *arg) {
  unsigned avail_start, avail_count;
  int count;

  ep(pthread_mutex_lock(&alsa_bg_lock));
  for(;;) {
    /* If we're shutting down, quit straight away */
    if(alsa_bg_shutdown)
      break;
    /* While we're disabled or the buffer is full, just wait */
    if(!alsa_bg_enabled || alsa_bg_count == BUFFER_BYTES) {
      ep(pthread_cond_wait(&alsa_bg_cond, &alsa_bg_lock));
      continue;
    }
    /* Figure out where and how big the gap we can write into is */
    avail_start = alsa_bg_start + alsa_bg_count;
    if(avail_start < BUFFER_BYTES)
      avail_count = BUFFER_BYTES - avail_start;
    else {
      avail_start %= BUFFER_BYTES;
      avail_count = alsa_bg_start - avail_start;
    }
    assert(avail_start < BUFFER_BYTES);
    assert(avail_count <= BUFFER_BYTES);
    assert(avail_count + alsa_bg_count <= BUFFER_BYTES);
    ep(pthread_mutex_unlock(&alsa_bg_lock));
    count = supplyfn(alsa_bg_buffer + avail_start,
		     avail_count / BYTES_PER_SAMPLE);
    ep(pthread_mutex_lock(&alsa_bg_lock));
    alsa_bg_count += count * BYTES_PER_SAMPLE;
    assert(alsa_bg_start < BUFFER_BYTES);
    assert(alsa_bg_count <= BUFFER_BYTES);
    ep(pthread_cond_signal(&alsa_bg_cond));
  }
  ep(pthread_mutex_unlock(&alsa_bg_lock));
  return 0;
}

/** @brief Playback thread
 *
 * This thread reads audio data out of the ring buffer and plays it back
 */
static void *alsa_bg_play(void attribute((unused)) *arg) {
  int prepared = 1, err;
  int start, nbytes, nframes, rframes;
  
  ep(pthread_mutex_lock(&alsa_bg_lock));
  for(;;) {
    /* If we're shutting down, quit straight away */
    if(alsa_bg_shutdown)
      break;
    /* Wait for some data to be available.  (If we are marked disabled
     * we keep on playing what we've got.) */
    if(alsa_bg_count == 0) {
      if(prepared) {
	if((err = snd_pcm_drain(pcm)))
	  fatal(0, "snd_pcm_drain: %d", err);
	prepared = 0;
      }
      ep(pthread_cond_wait(&alsa_bg_cond, &alsa_bg_lock));
      continue;
    }
    /* Calculate how much we can play */
    start = alsa_bg_start;
    if(start + alsa_bg_count <= BUFFER_BYTES)
      nbytes = alsa_bg_count;
    else
      nbytes = BUFFER_BYTES - start;
    /* Limit how much of the buffer we play.  The effect is that we return from
     * _writei earlier, and therefore free up more buffer space to read fresh
     * data into.  /2 works fine, /4 is just conservative.  /1 (i.e. abolishing
     * the heuristic) produces noticably noisy output. */
    if(nbytes > BUFFER_BYTES / 4)
      nbytes = BUFFER_BYTES / 4;
    assert((unsigned)nbytes <= alsa_bg_count);
    nframes = nbytes / BYTES_PER_FRAME;
    ep(pthread_mutex_unlock(&alsa_bg_lock));
    /* Make sure the PCM is prepared */
    if(!prepared) {
      if((err = snd_pcm_prepare(pcm)))
	fatal(0, "snd_pcm_prepare: %d", err);
      prepared = 1;
    }
    /* Play what we can */
    rframes = snd_pcm_writei(pcm, alsa_bg_buffer + start, nframes);
    ep(pthread_mutex_lock(&alsa_bg_lock));
    if(rframes < 0) {
      switch(rframes) {
      case -EPIPE:
        error(0, "underrun detected");
        if((err = snd_pcm_prepare(pcm)))
          fatal(0, "snd_pcm_prepare: %d", err);
        break;
      default:
        fatal(0, "snd_pcm_writei: %d", rframes);
      }
    } else {
      const int rbytes = rframes * BYTES_PER_FRAME;
      /*fprintf(stderr, "%5d -> %5d\n", nbytes, rbytes);*/
      /* Update the buffer pointers */
      alsa_bg_count -= rbytes;
      alsa_bg_start += rbytes;
      if(alsa_bg_start >= BUFFER_BYTES)
	alsa_bg_start -= BUFFER_BYTES;
      assert(alsa_bg_start < BUFFER_BYTES);
      assert(alsa_bg_count <= BUFFER_BYTES);
      /* Let the collector know we've opened up some space */
      ep(pthread_cond_signal(&alsa_bg_cond));
    }
  }
  ep(pthread_mutex_unlock(&alsa_bg_lock));
  return 0;
}

/** @brief Enable ALSA play */
void alsa_bg_enable(void) {
  ep(pthread_mutex_lock(&alsa_bg_lock));
  alsa_bg_enabled = 1;
  ep(pthread_cond_broadcast(&alsa_bg_cond));
  ep(pthread_mutex_unlock(&alsa_bg_lock));
}

/** @brief Disable ALSA play */
void alsa_bg_disable(void) {
  ep(pthread_mutex_lock(&alsa_bg_lock));
  alsa_bg_enabled = 0;
  ep(pthread_cond_broadcast(&alsa_bg_cond));
  ep(pthread_mutex_unlock(&alsa_bg_lock));
}

/** @brief Initialize background ALSA playback
 * @param device Target device or NULL to use default
 * @param supply Function to call to get audio data to play
 *
 * Playback is not initially enabled; see alsa_bg_enable().  When playback is
 * enabled, @p supply will be called in a background thread to request audio
 * data.  It should return in a timely manner, but playback happens from a
 * further thread and delays in @p supply will not delay transfer of data to
 * the sound device (provided it doesn't actually run out).
 */
void alsa_bg_init(const char *device,
		  alsa_bg_supply *supply) {
  snd_pcm_hw_params_t *hwparams;
  /* Only support one format for now */
  const int sample_format = SND_PCM_FORMAT_S16_BE;
  unsigned rate = 44100;
  int err;

  if((err = snd_pcm_open(&pcm,
                         device ? device : "default",
                         SND_PCM_STREAM_PLAYBACK,
                         0)))
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
                                           CHANNELS)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
          CHANNELS, err);
  if((err = snd_pcm_hw_params(pcm, hwparams)) < 0)
    fatal(0, "error calling snd_pcm_hw_params: %d", err);

  /* Record the audio supply function */
  supplyfn = supply;

  /* Create the audio output thread */
  alsa_bg_shutdown = 0;
  alsa_bg_enabled = 0;
  ep(pthread_create(&alsa_bg_collect_tid, 0, alsa_bg_collect, 0));
  ep(pthread_create(&alsa_bg_play_tid, 0, alsa_bg_play, 0));
}

void alsa_bg_close(void) {
  void *r;

  /* Notify background threads that we're shutting down */
  ep(pthread_mutex_lock(&alsa_bg_lock));
  alsa_bg_enabled = 0;
  alsa_bg_shutdown = 1;
  ep(pthread_cond_signal(&alsa_bg_cond));
  ep(pthread_mutex_unlock(&alsa_bg_lock));
  /* Join background threads when they're done */
  ep(pthread_join(alsa_bg_collect_tid, &r));
  ep(pthread_join(alsa_bg_play_tid, &r));
  /* Close audio device */
  snd_pcm_close(pcm);
  pcm = 0;
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
