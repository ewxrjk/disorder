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
#include "configuration.h"

/** @brief The current PCM handle */
static snd_pcm_t *alsa_pcm;

static const char *const alsa_options[] = {
  "device",
  "mixer-control",
  "mixer-channel",
  NULL
};

/** @brief Mixer handle */
snd_mixer_t *alsa_mixer_handle;

/** @brief Mixer control */
static snd_mixer_elem_t *alsa_mixer_elem;

/** @brief Left channel */
static snd_mixer_selem_channel_id_t alsa_mixer_left;

/** @brief Right channel */
static snd_mixer_selem_channel_id_t alsa_mixer_right;

/** @brief Minimum level */
static long alsa_mixer_min;

/** @brief Maximum level */
static long alsa_mixer_max;

/** @brief Actually play sound via ALSA */
static size_t alsa_play(void *buffer, size_t samples, unsigned flags) {
  /* If we're paused we just pretend.  We rely on snd_pcm_writei() blocking so
   * we have to fake up a sleep here.  However it doesn't have to be all that
   * accurate - in particular it's quite acceptable to greatly underestimate
   * the required wait time.  For 'lengthy' waits we do this by the blunt
   * instrument of halving it.  */
  if(flags & UAUDIO_PAUSED) {
    if(samples > 64)
      samples /= 2;
    const uint64_t ns = ((uint64_t)samples * 1000000000
                         / (uaudio_rate * uaudio_channels));
    struct timespec ts[1];
    ts->tv_sec = ns / 1000000000;
    ts->tv_nsec = ns % 1000000000;
    while(nanosleep(ts, ts) < 0 && errno == EINTR)
      ;
    return samples;
  }
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
  const char *device = uaudio_get("device", "default");
  int err;

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
                      4096 / uaudio_sample_size,
                      0);
}

static void alsa_stop(void) {
  uaudio_thread_stop();
  snd_pcm_close(alsa_pcm);
  alsa_pcm = 0;
}

/** @brief Convert a level to a percentage */
static int to_percent(long n) {
  return (n - alsa_mixer_min) * 100 / (alsa_mixer_max - alsa_mixer_min);
}

/** @brief Convert a percentage to a level */
static int from_percent(int n) {
  return alsa_mixer_min + n * (alsa_mixer_max - alsa_mixer_min) / 100;
}

static void alsa_open_mixer(void) {
  int err;
  snd_mixer_selem_id_t *id;
  const char *device = uaudio_get("device", "default");
  const char *mixer = uaudio_get("mixer-control", "0");
  const char *channel = uaudio_get("mixer-channel", "PCM");

  snd_mixer_selem_id_alloca(&id);
  if((err = snd_mixer_open(&alsa_mixer_handle, 0)))
    fatal(0, "snd_mixer_open: %s", snd_strerror(err));
  if((err = snd_mixer_attach(alsa_mixer_handle, device)))
    fatal(0, "snd_mixer_attach %s: %s", device, snd_strerror(err));
  if((err = snd_mixer_selem_register(alsa_mixer_handle,
                                     0/*options*/, 0/*classp*/)))
    fatal(0, "snd_mixer_selem_register %s: %s",
          device, snd_strerror(err));
  if((err = snd_mixer_load(alsa_mixer_handle)))
    fatal(0, "snd_mixer_load %s: %s", device, snd_strerror(err));
  snd_mixer_selem_id_set_name(id, channel);
  snd_mixer_selem_id_set_index(id, atoi(mixer));
  if(!(alsa_mixer_elem = snd_mixer_find_selem(alsa_mixer_handle, id)))
    fatal(0, "device '%s' mixer control '%s,%s' does not exist",
	  device, channel, mixer);
  if(!snd_mixer_selem_has_playback_volume(alsa_mixer_elem))
    fatal(0, "device '%s' mixer control '%s,%s' has no playback volume",
	  device, channel, mixer);
  if(snd_mixer_selem_is_playback_mono(alsa_mixer_elem)) {
    alsa_mixer_left = alsa_mixer_right = SND_MIXER_SCHN_MONO;
  } else {
    alsa_mixer_left = SND_MIXER_SCHN_FRONT_LEFT;
    alsa_mixer_right = SND_MIXER_SCHN_FRONT_RIGHT;
  }
  if(!snd_mixer_selem_has_playback_channel(alsa_mixer_elem,
                                           alsa_mixer_left)
     || !snd_mixer_selem_has_playback_channel(alsa_mixer_elem,
                                              alsa_mixer_right))
    fatal(0, "device '%s' mixer control '%s,%s' lacks required playback channels",
	  device, channel, mixer);
  snd_mixer_selem_get_playback_volume_range(alsa_mixer_elem,
                                            &alsa_mixer_min, &alsa_mixer_max);

}

static void alsa_close_mixer(void) {
  /* TODO alsa_mixer_elem */
  if(alsa_mixer_handle)
    snd_mixer_close(alsa_mixer_handle);
}

static void alsa_get_volume(int *left, int *right) {
  long l, r;
  int err;
  
  if((err = snd_mixer_selem_get_playback_volume(alsa_mixer_elem,
                                                alsa_mixer_left, &l))
     || (err = snd_mixer_selem_get_playback_volume(alsa_mixer_elem,
                                                   alsa_mixer_right, &r)))
    fatal(0, "snd_mixer_selem_get_playback_volume: %s", snd_strerror(err));
  *left = to_percent(l);
  *right = to_percent(r);
}

static void alsa_set_volume(int *left, int *right) {
  long l, r;
  int err;
  
  /* Set the volume */
  if(alsa_mixer_left == alsa_mixer_right) {
    /* Mono output - just use the loudest */
    if((err = snd_mixer_selem_set_playback_volume
	(alsa_mixer_elem, alsa_mixer_left,
	 from_percent(*left > *right ? *left : *right))))
      fatal(0, "snd_mixer_selem_set_playback_volume: %s", snd_strerror(err));
  } else {
    /* Stereo output */
    if((err = snd_mixer_selem_set_playback_volume
	(alsa_mixer_elem, alsa_mixer_left, from_percent(*left)))
       || (err = snd_mixer_selem_set_playback_volume
	   (alsa_mixer_elem, alsa_mixer_right, from_percent(*right))))
      fatal(0, "snd_mixer_selem_set_playback_volume: %s", snd_strerror(err));
  }
  /* Read it back to see what we ended up at */
  if((err = snd_mixer_selem_get_playback_volume(alsa_mixer_elem,
                                                alsa_mixer_left, &l))
     || (err = snd_mixer_selem_get_playback_volume(alsa_mixer_elem,
                                                   alsa_mixer_right, &r)))
    fatal(0, "snd_mixer_selem_get_playback_volume: %s", snd_strerror(err));
  *left = to_percent(l);
  *right = to_percent(r);
}

static void alsa_configure(void) {
  uaudio_set("device", config->device);
  uaudio_set("mixer-control", config->mixer);
  uaudio_set("mixer-channel", config->channel);
}

const struct uaudio uaudio_alsa = {
  .name = "alsa",
  .options = alsa_options,
  .start = alsa_start,
  .stop = alsa_stop,
  .activate = uaudio_thread_activate,
  .deactivate = uaudio_thread_deactivate,
  .open_mixer = alsa_open_mixer,
  .close_mixer = alsa_close_mixer,
  .get_volume = alsa_get_volume,
  .set_volume = alsa_set_volume,
  .configure = alsa_configure
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
