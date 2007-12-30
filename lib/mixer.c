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
/** @file lib/mixer.c
 * @brief Mixer support
 */

#include <config.h>
#include "types.h"

#include "configuration.h"
#include "mixer.h"
#include "log.h"
#include "mem.h"

/** @brief Whether lack of volume support has been reported yet */
static int none_reported;

/** @brief Get/set volume stub if volume control is not supported */
static int none_get_set(int attribute((unused)) *left,
			int attribute((unused)) *right) {
  if(!none_reported) {
    error(0, "don't know how to get/set volume with this api");
    none_reported = 1;
  }
  return -1;
}

/** @brief Stub mixer control */
static const struct mixer mixer_none = {
  -1,
  none_get_set,
  none_get_set,
  "",
  ""
};

/** @brief Table of mixer definitions */
static const struct mixer *mixers[] = {
#if HAVE_SYS_SOUNDCARD_H
  &mixer_oss,
#endif
#if HAVE_ALSA_ASOUNDLIB_H
  &mixer_alsa,
#endif
  &mixer_none				/* make sure array is never empty */
};

/** @brief Number of mixer definitions */
#define NMIXERS (sizeof mixers / sizeof *mixers)

/** @brief Find the mixer definition */
static const struct mixer *find_mixer(int api) {
  size_t n;

  for(n = 0; n < NMIXERS; ++n)
    if(mixers[n]->api == api)
      return mixers[n];
  return &mixer_none;
}

/** @brief Get/set volume
 * @param left Left channel level, 0-100
 * @param right Right channel level, 0-100
 * @param set Set volume if non-0
 * @return 0 on success, non-0 on error
 *
 * If getting the volume then @p left and @p right are filled in.
 *
 * If setting the volume then the target levels are read from @p left and
 * @p right, and the actual level set is stored in them.
 */
int mixer_control(int *left, int *right, int set) {
  const struct mixer *const m = find_mixer(config->api);

  /* We impose defaults bizarrely late, but this has the advantage of
   * not making everything depend on sound libraries */
  if(!config->mixer)
    config->mixer = xstrdup(m->device);
  if(!config->channel)
    config->channel = xstrdup(m->channel);
  if(set)
    return m->set(left, right);
  else
    return m->get(left, right);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
