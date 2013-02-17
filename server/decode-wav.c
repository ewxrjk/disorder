/*
 * This file is part of DisOrder
 * Copyright (C) 2007-2011 Richard Kettlewell
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
/** @file server/decode-wav.c
 * @brief General-purpose decoder for use by speaker process
 */
#include "decode.h"
#include "wav.h"

/** @brief Sample data callback used by decode_wav() */
static int wav_write(struct wavfile attribute((unused)) *f,
                     const char *data,
                     size_t nbytes,
                     void attribute((unused)) *u) {
  if(fwrite(data, 1, nbytes, outputfp) < nbytes)
    disorder_fatal(errno, "decoding %s: writing sample data", path);
  return 0;
}

/** @brief WAV file decoder */
void decode_wav(void) {
  struct wavfile f[1];
  int err;

  if((err = wav_init(f, path)))
    disorder_fatal(err, "opening %s", path);
  output_header(f->rate, f->channels, f->bits, f->datasize, ENDIAN_LITTLE);
  if((err = wav_data(f, wav_write, 0)))
    disorder_fatal(err, "error decoding %s", path);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
