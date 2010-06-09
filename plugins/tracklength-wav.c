/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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
/** @file plugins/tracklength-wav.c
 * @brief Compute track lengths for WAV files
 */
#include "tracklength.h"
#include "wav.h"

long tl_wav(const char *path) {
  struct wavfile f[1];
  int err, sample_frame_size;
  long duration;

  if((err = wav_init(f, path))) {
    disorder_error(err, "error opening %s", path); 
    return -1;
  }
  sample_frame_size = (f->bits + 7) / 8 * f->channels;
  if(sample_frame_size) {
    const long long n_samples = f->datasize / sample_frame_size;
    duration = (n_samples + f->rate - 1) / f->rate;
  } else
    duration = -1;
  wav_destroy(f);
  return duration;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
