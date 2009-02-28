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

/** @file lib/uaudio.c
 * @brief Uniform audio interface
 */

#include "common.h"
#include "uaudio.h"
#include "hash.h"
#include "mem.h"

/** @brief Options for chosen uaudio API */
static hash *uaudio_options;

/** @brief Set a uaudio option */
void uaudio_set(const char *name, const char *value) {
  if(!uaudio_options)
    uaudio_options = hash_new(sizeof(char *));
  value = xstrdup(value);
  hash_add(uaudio_options, name, &value, HASH_INSERT_OR_REPLACE);
}

/** @brief Set a uaudio option */
const char *uaudio_get(const char *name) {
  const char *value = (uaudio_options ?
                       *(char **)hash_find(uaudio_options, name)
                       : NULL);
  return value ? xstrdup(value) : NULL;
}

/** @brief List of known APIs
 *
 * Terminated by a null pointer.
 *
 * The first one will be used as a default, so putting ALSA before OSS
 * constitutes a policy decision.
 */
const struct uaudio *uaudio_apis[] = {
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
  &uaudio_coreaudio,
#endif  
#if HAVE_ALSA_ASOUNDLIB_H
  &uaudio_alsa,
#endif
#if HAVE_SYS_SOUNDCARD_H || EMPEG_HOST
  &uaudio_oss,
#endif
  &uaudio_rtp,
  NULL,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
