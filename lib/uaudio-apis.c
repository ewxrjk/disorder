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

/** @file lib/uaudio-apis.c
 * @brief Audio API list
 */

#include "common.h"
#include "uaudio.h"
#include "log.h"

/** @brief List of known APIs
 *
 * Terminated by a null pointer.
 *
 * The first one will be used as a default, so putting ALSA before OSS
 * constitutes a policy decision.
 */
const struct uaudio *const uaudio_apis[] = {
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
  &uaudio_command,
  NULL,
};

/** @brief Look up an audio API by name */
const struct uaudio *uaudio_find(const char *name) {
  int n;

  for(n = 0; uaudio_apis[n]; ++n)
    if(!strcmp(uaudio_apis[n]->name, name))
      return uaudio_apis[n];
  if(!strcmp(name, "network"))
    return &uaudio_rtp;
  disorder_fatal(0, "cannot find audio API '%s'", name);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
