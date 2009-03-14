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

/** @brief Sample rate (Hz) */
int uaudio_rate;

/** @brief Bits per channel */
int uaudio_bits;

/** @brief Number of channels */
int uaudio_channels;

/** @brief Whether samples are signed or unsigned */
int uaudio_signed;

/** @brief Sample size in bytes
 *
 * NB one sample is a single point sample; up to @c uaudio_channels samples may
 * play at the same time through different speakers.  Thus this value is
 * independent of @ref uaudio_channels.
 */
size_t uaudio_sample_size;

/** @brief Set a uaudio option */
void uaudio_set(const char *name, const char *value) {
  if(!value) {
    if(uaudio_options)
      hash_remove(uaudio_options, name);
    return;
  }
  if(!uaudio_options)
    uaudio_options = hash_new(sizeof(char *));
  value = xstrdup(value);
  hash_add(uaudio_options, name, &value, HASH_INSERT_OR_REPLACE);
}

/** @brief Get a uaudio option */
char *uaudio_get(const char *name, const char *default_value) {
  if(!uaudio_options)
    return default_value ? xstrdup(default_value) : 0;
  char **valuep = hash_find(uaudio_options, name);
  if(!valuep)
    return default_value ? xstrdup(default_value) : 0;
  return xstrdup(*valuep);
}

/** @brief Set sample format 
 * @param rate Sample rate in KHz
 * @param channels Number of channels (i.e. 2 for stereo)
 * @param bits Number of bits per channel (typically 8 or 16)
 * @param signed_ True for signed samples, false for unsigned
 *
 * Sets @ref uaudio_rate, @ref uaudio_channels, @ref uaudio_bits, @ref
 * uaudio_signed and @ref uaudio_sample_size.
 *
 * Currently there is no way to specify non-native endian formats even if the
 * underlying API can conveniently handle them.  Actually this would be quite
 * convenient for playrtp, so it might be added at some point.
 *
 * Not all APIs can support all sample formats.  Generally the @c start
 * function will do some error checking but some may be deferred to the point
 * the device is opened (which might be @c activate).
 */
void uaudio_set_format(int rate, int channels, int bits, int signed_) {
  uaudio_rate = rate;
  uaudio_channels = channels;
  uaudio_bits = bits;
  uaudio_signed = signed_;
  uaudio_sample_size = bits / CHAR_BIT;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
