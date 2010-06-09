/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/wav.h
 * @brief WAV file support
 */

#ifndef WAV_H
#define WAV_H

#include "hreader.h"

/** @brief WAV file access structure */
struct wavfile {
  /** @brief File read handle */
  struct hreader input[1];

  /** @brief File length */
  off_t length;

  /** @brief Offset of data chunk */
  off_t data;

  /** @brief Sample rate (Hz) */
  int rate;

  /** @brief Number of channels (usually 1 or 2) */
  int channels;

  /** @brief Bits per sample */
  int bits;

  /** @brief Size of data chunk in bytes */
  off_t datasize;
};

/** @brief Sample data callback from wav_data()
 * @param f WAV file being read
 * @param data Pointer to sample data
 * @param nbytes Number of bytes of data
 * @param u As passed to wav_data()
 * @return 0 on success or an errno value on error
 *
 * @p nbytes is always a multiple of the frame size and never 0.
 */
typedef int wav_data_callback(struct wavfile *f,
                              const char *data,
                              size_t nbytes,
                              void *u);

int wav_init(struct wavfile *f, const char *path);
void wav_destroy(struct wavfile *f);
int wav_data(struct wavfile *f,
	     wav_data_callback *callback,
	     void *u);

#endif /* WAV_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
