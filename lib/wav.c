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
/** @file lib/wav.c
 * @brief WAV file support
 *
 * This is used by the WAV file suppoort in the tracklength plugin and
 * by disorder-decode (see @ref server/decode.c).
 */

/* Sources:
 *
 * http://www.technology.niagarac.on.ca/courses/comp530/WavFileFormat.html
 * http://www.borg.com/~jglatt/tech/wave.htm
 * http://www.borg.com/~jglatt/tech/aboutiff.htm
 *
 * These files consists of a header followed by chunks.
 * Multibyte values are little-endian.
 *
 * 12 byte file header:
 *  offset  size  meaning
 *  00      4     'RIFF'
 *  04      4     length of rest of file
 *  08      4     'WAVE'
 *
 * The length includes 'WAVE' but excludes the 1st 8 bytes.
 *
 * Chunk header:
 *  00      4     chunk ID
 *  04      4     length of rest of chunk
 *
 * The stated length may be odd, if so then there is an implicit padding byte
 * appended to the chunk to make it up to an even length (someone wasn't
 * think about 32/64-bit worlds).
 *
 * Also some files seem to have extra stuff at the end of chunks that nobody
 * I know of documents.  Go figure, but check the length field rather than
 * deducing the length from the ID.
 *
 * Format chunk:
 *  00      4     'fmt'
 *  04      4     length of rest of chunk
 *  08      2     compression (1 = none)
 *  0a      2     number of channels
 *  0c      4     samples/second
 *  10      4     average bytes/second, = (samples/sec) * (bytes/sample)
 *  14      2     bytes/sample
 *  16      2     bits/sample point
 *
 * 'sample' means 'sample frame' above, i.e. a sample point for each channel.
 *
 * Data chunk:
 *  00      4     'data'
 *  04      4     length of rest of chunk
 *  08      ...   data
 *
 * There is only allowed to be one data chunk.  Some people violate this; we
 * shall encourage people to fix their broken WAV files by not supporting
 * this violation and because it's easier.
 *
 * As to the encoding of the data:
 *
 * Firstly, samples up to 8 bits in size are unsigned, larger samples are
 * signed.  Madness.
 *
 * Secondly sample points are stored rounded up to a multiple of 8 bits in
 * size.  Marginally saner.
 *
 * Written as a single word (of 8, 16, 24, whatever bits) the padding to
 * implement this happens at the right hand (least significant) end.
 * e.g. assuming a 9 bit sample:
 *
 * |                 padded sample word              |
 * | 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 |
 * |  8  7  6  5  4  3  2  1  0  -  -  -  -  -  -  - |
 * 
 * But this is a little-endian file format so the least significant byte is
 * the first, which means that the padding is "between" the bits if you
 * imagine them in their usual order:
 *
 *  |     first byte         |     second byte        |
 *  | 7  6  5  4  3  2  1  0 | 7  6  5  4  3  2  1  0 |
 *  | 0  -  -  -  -  -  -  - | 8  7  6  5  4  3  2  1 |
 *
 * Sample points are grouped into sample frames, consisting of as many
 * samples points as their are channels.  It seems that there are standard
 * orderings of different channels.
 */

#include "common.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "log.h"
#include "wav.h"

static inline uint16_t get16(const char *ptr) {
  return (uint8_t)ptr[0] + 256 * (uint8_t)ptr[1];
}

static inline uint32_t get32(const char *ptr) {
  return (uint8_t)ptr[0] + 256 * (uint8_t)ptr[1]
         + 65536 * (uint8_t)ptr[2] + 16777216 * (uint8_t)ptr[3];
}

/** @brief Open a WAV file
 * @return 0 on success, an errno value on error
 */
int wav_init(struct wavfile *f, const char *path) {
  int err, n;
  char header[64];
  off_t where;
  
  memset(f, 0, sizeof *f);
  f->data = -1;
  hreader_init(path, f->input);
  /* Read the file header
   *
   *  offset  size  meaning
   *  00      4     'RIFF'
   *  04      4     length of rest of file
   *  08      4     'WAVE'
   * */
  if((n = hreader_pread(f->input, header, 12, 0)) < 0) goto error_errno;
  else if(n < 12) goto einval;
  if(strncmp(header, "RIFF", 4) || strncmp(header + 8, "WAVE", 4))
    goto einval;
  f->length = 8 + get32(header + 4);
  /* Visit all the chunks */
  for(where = 12; where + 8 <= f->length;) {
    /* Read the chunk header
     *
     *  offset  size  meaning
     *  00      4     chunk ID
     *  04      4     length of rest of chunk
     */
    if((n = hreader_pread(f->input, header, 8, where)) < 0) goto error_errno;
    else if(n < 8) goto einval;
    if(!strncmp(header,"fmt ", 4)) {
      /* This is the format chunk
       *
       *  offset  size  meaning
       *  00      4     'fmt'
       *  04      4     length of rest of chunk
       *  08      2     compression (1 = none)
       *  0a      2     number of channels
       *  0c      4     samples/second
       *  10      4     average bytes/second, = (samples/sec) * (bytes/sample)
       *  14      2     bytes/sample
       *  16      2     bits/sample point
       *  18      ?     extra undocumented rubbish
       */
      if(get32(header + 4) < 16) goto einval;
      if((n = hreader_pread(f->input, header + 8, 16, where + 8)) < 0)
        goto error_errno;
      else if(n < 16) goto einval;
      f->channels = get16(header + 0x0A);
      f->rate = get32(header + 0x0C);
      f->bits = get16(header + 0x16);
    } else if(!strncmp(header, "data", 4)) {
      /* Remember where the data chunk was and how big it is */
      f->data = where;
      f->datasize = get32(header + 4);
    }
    where += 8 + get32(header + 4);
  }
  /* There had better have been a format chunk */
  if(f->rate == 0) goto einval;
  /* There had better have been a data chunk */
  if(f->data == -1) goto einval;
  return 0;
einval:
  err = EINVAL;
  goto error;
error_errno:
  err = errno;
error:
  wav_destroy(f);
  return err;
}

/** @brief Close a WAV file */
void wav_destroy(struct wavfile attribute((unused)) *f) {
}

/** @brief Visit all the data in a WAV file
 * @param f WAV file handle
 * @param callback Called for successive blocks of data
 * @param u User data
 *
 * @p callback will only ever be passed whole frames.
 */
int wav_data(struct wavfile *f,
	     wav_data_callback *callback,
	     void *u) {
  off_t left = f->datasize;
  off_t where = f->data + 8;
  char buffer[4096];
  int err;
  ssize_t n;
  const size_t bytes_per_frame = f->channels * ((f->bits + 7) / 8);

  while(left > 0) {
    size_t want = (off_t)sizeof buffer > left ? (size_t)left : sizeof buffer;

    want -= want % bytes_per_frame;
    if((n = hreader_pread(f->input, buffer, want, where)) < 0) return errno;
    if((size_t)n < want) return EINVAL;
    if((err = callback(f, buffer, n, u))) return err;
    where += n;
    left -= n;
  }
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
