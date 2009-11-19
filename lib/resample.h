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

/** @file lib/resample.h
 * @brief Audio resampling
 */

#ifndef RESAMPLE_H
#define RESAMPLE_H

#if HAVE_SAMPLERATE_H
#include <samplerate.h>
#endif

#define ENDIAN_BIG 1
#define ENDIAN_LITTLE 2

struct resampler {
  int input_bits, input_channels, input_rate, input_signed, input_endian;
  int output_bits, output_channels, output_rate, output_signed, output_endian;
  int input_bytes_per_sample;
  int input_bytes_per_frame;
#if HAVE_SAMPLERATE_H
  SRC_STATE *state;
#endif
};

void resample_init(struct resampler *rs, 
                   int input_bits, int input_channels, 
                   int input_rate, int input_signed,
                   int input_endian,
                   int output_bits, int output_channels, 
                   int output_rate, int output_signed,
                   int output_endian);
size_t resample_convert(const struct resampler *rs,
                        const uint8_t *bytes,
                        size_t nbytes,
                        int eof,
                        void (*converted)(uint8_t *bytes,
                                          size_t nbytes,
                                          void *cd),
                        void *cd);
void resample_close(struct resampler *rs);

#endif /* RESAMPLE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
