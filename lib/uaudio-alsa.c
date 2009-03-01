/*
 * This file is part of DisOrder.
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file lib/uaudio-alsa.c
 * @brief Support for ALSA backend */
#include "common.h"

#if HAVE_ALSA_ASOUNDLIB_H

#include <alsa/asoundlib.h>

#include "mem.h"
#include "log.h"
#include "uaudio.h"

/** @brief The current PCM handle */
static snd_pcm_t *alsa_pcm;

static const char *const alsa_options[] = {
  "device",
  NULL
};

/** @brief Actually play sound via ALSA */
static size_t alsa_play(void *buffer, size_t samples) {
  int err;
  /* ALSA wants 'frames', where frame = several concurrently played samples */
  const snd_pcm_uframes_t frames = samples / uaudio_channels;

  snd_pcm_sframes_t rc = snd_pcm_writei(alsa_pcm, buffer, frames);
  if(rc < 0) {
    switch(rc) {
    case -EPIPE:
      if((err = snd_pcm_prepare(alsa_pcm)))
	fatal(0, "error calling snd_pcm_prepare: %d", err);
      return 0;
    case -EAGAIN:
      return 0;
    default:
      fatal(0, "error calling snd_pcm_writei: %d", (int)rc);
    }
  }
  return rc * uaudio_channels;
}

/** @brief Open the ALSA sound device */
static void alsa_open(void) {
  const char *device = uaudio_get("device");
  int err;

  if(!device || !*device)
    device = "default";
  if((err = snd_pcm_open(&alsa_pcm,
			 device,
			 SND_PCM_STREAM_PLAYBACK,
			 0)))
    fatal(0, "error from snd_pcm_open: %d", err);
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_hw_params_alloca(&hwparams);
  if((err = snd_pcm_hw_params_any(alsa_pcm, hwparams)) < 0)
    fatal(0, "error from snd_pcm_hw_params_any: %d", err);
  if((err = snd_pcm_hw_params_set_access(alsa_pcm, hwparams,
                                         SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_access: %d", err);
  int sample_format;
  if(uaudio_bits == 16)
    sample_format = uaudio_signed ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_U16;
  else
    sample_format = uaudio_signed ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_U8;
  if((err = snd_pcm_hw_params_set_format(alsa_pcm, hwparams,
                                         sample_format)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_format (%d): %d",
          sample_format, err);
  unsigned rate = uaudio_rate;
  if((err = snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &rate, 0)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_rate_near (%d): %d",
          rate, err);
  if((err = snd_pcm_hw_params_set_channels(alsa_pcm, hwparams,
                                           uaudio_channels)) < 0)
    fatal(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
          uaudio_channels, err);
  if((err = snd_pcm_hw_params(alsa_pcm, hwparams)) < 0)
    fatal(0, "error calling snd_pcm_hw_params: %d", err);
  
}

static void alsa_activate(void) {
  uaudio_thread_activate();
}

static void alsa_deactivate(void) {
  uaudio_thread_deactivate();
}
  
static void alsa_start(uaudio_callback *callback,
                      void *userdata) {
  if(uaudio_channels != 1 && uaudio_channels != 2)
    fatal(0, "asked for %d channels but only support 1 or 2",
          uaudio_channels); 
  if(uaudio_bits != 8 && uaudio_bits != 16)
    fatal(0, "asked for %d bits/channel but only support 8 or 16",
          uaudio_bits); 
  alsa_open();
  uaudio_thread_start(callback, userdata, alsa_play,
                      32 / uaudio_sample_size,
                      4096 / uaudio_sample_size);
}

static void alsa_stop(void) {
  uaudio_thread_stop();
  snd_pcm_close(alsa_pcm);
  alsa_pcm = 0;
}

const struct uaudio uaudio_alsa = {
  .name = "alsa",
  .options = alsa_options,
  .start = alsa_start,
  .stop = alsa_stop,
  .activate = alsa_activate,
  .deactivate = alsa_deactivate
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
