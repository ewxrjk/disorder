/*
 * This file is part of DisOrder
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
/** @file lib/mixer-alsa.c
 * @brief ALSA mixer support
 *
 * The documentation for ALSA's mixer support is completely hopeless,
 * which is a particular nuisnace given it's got an incredibly verbose
 * API.  Much of this code is cribbed from
 * alsa-utils-1.0.13/amixer/amixer.c.
 *
 * Mono output devices are supported, but the support is not tested
 * (as I don't one).
 */

#include <config.h>

#if HAVE_ALSA_ASOUNDLIB_H

#include "types.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>

#include "configuration.h"
#include "mixer.h"
#include "log.h"
#include "syscalls.h"

/** @brief Shared state for ALSA mixer support */
struct alsa_mixer_state {
  /** @brief Mixer handle */
  snd_mixer_t *handle;

  /** @brief Mixer control */
  snd_mixer_elem_t *elem;

  /** @brief Left channel */
  snd_mixer_selem_channel_id_t left;

  /** @brief Right channel */
  snd_mixer_selem_channel_id_t right;

  /** @brief Minimum level */
  long min;

  /** @brief Maximum level */
  long max;
};

/** @brief Destroy a @ref alsa_mixer_state */
static void alsa_close(struct alsa_mixer_state *h) {
  /* TODO h->elem */
  if(h->handle)
    snd_mixer_close(h->handle);
}

/** @brief Initialize a @ref alsa_mixer_state */
static int alsa_open(struct alsa_mixer_state *h) {
  int err;
  snd_mixer_selem_id_t *id;

  snd_mixer_selem_id_alloca(&id);
  memset(h, 0, sizeof h);
  if((err = snd_mixer_open(&h->handle, 0))) {
    error(0, "snd_mixer_open: %s", snd_strerror(err));
    return -1;
  }
  if((err = snd_mixer_attach(h->handle, config->device))) {
    error(0, "snd_mixer_attach %s: %s",
	  config->device, snd_strerror(err));
    goto error;
  }
  if((err = snd_mixer_selem_register(h->handle, 0/*options*/, 0/*classp*/))) {
    error(0, "snd_mixer_selem_register %s: %s",
	  config->device, snd_strerror(err));
    goto error;
  }
  if((err = snd_mixer_load(h->handle))) {
    error(0, "snd_mixer_load %s: %s",
	  config->device, snd_strerror(err));
    goto error;
  }
  snd_mixer_selem_id_set_name(id, config->channel);
  snd_mixer_selem_id_set_index(id, atoi(config->mixer));
  if(!(h->elem = snd_mixer_find_selem(h->handle, id))) {
    error(0, "device '%s' mixer control '%s,%s' does not exist",
	  config->device, config->channel, config->mixer);
    goto error;
  }
  if(!snd_mixer_selem_has_playback_volume(h->elem)) {
    error(0, "device '%s' mixer control '%s,%s' has no playback volume",
	  config->device, config->channel, config->mixer);
    goto error;
  }
  if(snd_mixer_selem_is_playback_mono(h->elem)) {
    h->left = h->right = SND_MIXER_SCHN_MONO;
  } else {
    h->left = SND_MIXER_SCHN_FRONT_LEFT;
    h->right = SND_MIXER_SCHN_FRONT_RIGHT;
  }
  if(!snd_mixer_selem_has_playback_channel(h->elem, h->left)
     || !snd_mixer_selem_has_playback_channel(h->elem, h->right)) {
    error(0, "device '%s' mixer control '%s,%s' lacks required playback channels",
	  config->device, config->channel, config->mixer);
    goto error;
  }
  snd_mixer_selem_get_playback_volume_range(h->elem, &h->min, &h->max);
  return 0;
error:
  alsa_close(h);
  return -1;
}

/** @brief Convert a level to a percentage */
static int to_percent(const struct alsa_mixer_state *h, long n) {
  return (n - h->min) * 100 / (h->max - h->min);
}

/** @brief Get ALSA volume */
static int alsa_get(int *left, int *right) {
  struct alsa_mixer_state h[1];
  long l, r;
  int err;
  
  if(alsa_open(h))
    return -1;
  if((err = snd_mixer_selem_get_playback_volume(h->elem, h->left, &l))
     || (err = snd_mixer_selem_get_playback_volume(h->elem, h->right, &r))) {
    error(0, "snd_mixer_selem_get_playback_volume: %s", snd_strerror(err));
    goto error;
  }
  *left = to_percent(h, l);
  *right = to_percent(h, r);
  alsa_close(h);
  return 0;
error:
  alsa_close(h);
  return -1;
}

/** @brief Convert a percentage to a level */
static int from_percent(const struct alsa_mixer_state *h, int n) {
  return h->min + n * (h->max - h->min) / 100;
}

/** @brief Set ALSA volume */
static int alsa_set(int *left, int *right) {
  struct alsa_mixer_state h[1];
  long l, r;
  int err;
  
  if(alsa_open(h))
    return -1;
  /* Set the volume */
  if(h->left == h->right) {
    /* Mono output - just use the loudest */
    if((err = snd_mixer_selem_set_playback_volume
	(h->elem, h->left,
	 from_percent(h, *left > *right ? *left : *right)))) {
      error(0, "snd_mixer_selem_set_playback_volume: %s", snd_strerror(err));
      goto error;
    }
  } else {
    /* Stereo output */
    if((err = snd_mixer_selem_set_playback_volume
	(h->elem, h->left, from_percent(h, *left)))
       || (err = snd_mixer_selem_set_playback_volume
	   (h->elem, h->right, from_percent(h, *right)))) {
      error(0, "snd_mixer_selem_set_playback_volume: %s", snd_strerror(err));
      goto error;
    }
  }
  /* Read it back to see what we ended up at */
  if((err = snd_mixer_selem_get_playback_volume(h->elem, h->left, &l))
     || (err = snd_mixer_selem_get_playback_volume(h->elem, h->right, &r))) {
    error(0, "snd_mixer_selem_get_playback_volume: %s", snd_strerror(err));
    goto error;
  }
  *left = to_percent(h, l);
  *right = to_percent(h, r);
  alsa_close(h);
  return 0;
error:
  alsa_close(h);
  return -1;
}

/** @brief ALSA mixer vtable */
const struct mixer mixer_alsa = {
  BACKEND_ALSA,
  alsa_get,
  alsa_set,
  "0",
  "PCM"
};
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
