/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007 Richard Kettlewell
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
/** @file lib/mixer.h
 * @brief Mixer support
 */

#ifndef MIXER_H
#define MIXER_H

/** @brief Definition of a mixer */
struct mixer {
  /** @brief API used by this mixer */
  int api;

  /** @brief Get the volume
   * @param left Where to store left-channel volume
   * @param right Where to store right-channel volume
   * @return 0 on success, non-0 on error
   */
  int (*get)(int *left, int *right);

  /** @brief Set the volume
   * @param left Pointer to target left-channel volume
   * @param right Pointer to target right-channel volume
   * @return 0 on success, non-0 on error
   *
   * @p left and @p right are updated with the actual volume set.
   */
  int (*set)(int *left, int *right);

  /** @brief Default device */
  const char *device;

  /** @brief Default channel */
  const char *channel;
};

int mixer_control(int *left, int *right, int set);
const char *mixer_default_device(int api);
const char *mixer_default_channel(int api);

extern const struct mixer mixer_oss;
extern const struct mixer mixer_alsa;

#endif /* MIXER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
