/*
 * This file is part of DisOrder.
 * Portions copyright (C) 2004, 2005 Richard Kettlewell (see also below)
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

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <vorbis/vorbisfile.h>
#include <mad.h>

#include <disorder.h>

#include "madshim.h"

static void *mmap_file(const char *path, size_t *lengthp) {
  int fd;
  void *base;
  struct stat sb;
  
  if((fd = open(path, O_RDONLY)) < 0) {
    disorder_error(errno, "error opening %s", path);
    return 0;
  }
  if(fstat(fd, &sb) < 0) {
    disorder_error(errno, "error calling stat on %s", path);
    goto error;
  }
  if(sb.st_size == 0)			/* can't map 0-length files */
    goto error;
  if((base = mmap(0, sb.st_size, PROT_READ,
		  MAP_SHARED, fd, 0)) == (void *)-1) {
    disorder_error(errno, "error calling mmap on %s", path);
    goto error;
  }
  *lengthp = sb.st_size;
  close(fd);
  return base;
error:
  close(fd);
  return 0;
}

static long tl_mp3(const char *path) {
  size_t length;
  void *base;
  buffer b;

  if(!(base = mmap_file(path, &length))) return -1;
  b.duration = mad_timer_zero;
  scan_mp3(base, length, &b);
  munmap(base, length);
  return b.duration.seconds + !!b.duration.fraction;
}

static long tl_ogg(const char *path) {
  OggVorbis_File vf;
  FILE *fp = 0;
  double length;

  if(!path) goto error;
  if(!(fp = fopen(path, "rb"))) goto error;
  if(ov_open(fp, &vf, 0, 0)) goto error;
  fp = 0;
  length = ov_time_total(&vf, -1);
  ov_clear(&vf);
  return ceil(length);
error:
  if(fp) fclose(fp);
  return -1;
}

static long tl_wav(const char *path) {
  size_t length;
  void *base;
  long duration = -1;
  unsigned char *ptr;
  unsigned n, m, data_bytes = 0, samples_per_second = 0;
  unsigned n_channels = 0, bits_per_sample = 0, sample_point_size;
  unsigned sample_frame_size, n_samples;

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
   *
   * Given all of the above all we need to do is pick up some numbers from the
   * format chunk, and the length of the data chunk, and do some arithmetic.
   */
  if(!(base = mmap_file(path, &length))) return -1;
#define get16(p) ((p)[0] + 256 * (p)[1])
#define get32(p) ((p)[0] + 256 * ((p)[1] + 256 * ((p)[2] + 256 * (p)[3])))
  ptr = base;
  if(length < 12) goto out;
  if(strncmp((char *)ptr, "RIFF", 4)) goto out;	/* wrong type */
  n = get32(ptr + 4);			/* file length */
  if(n > length - 8) goto out;		/* truncated */
  ptr += 8;				/* skip file header */
  if(n < 4 || strncmp((char *)ptr, "WAVE", 4)) goto out; /* wrong type */
  ptr += 4;				/* skip 'WAVE' */
  n -= 4;
  while(n >= 8) {
    m = get32(ptr + 4);			/* chunk length */
    if(m > n - 8) goto out;		/* truncated */
    if(!strncmp((char *)ptr, "fmt ", 4)) {
      if(samples_per_second) goto out;	/* duplicate format chunk! */
      n_channels = get16(ptr + 0x0a);
      samples_per_second = get32(ptr + 0x0c);
      bits_per_sample = get16(ptr + 0x16);
      if(!samples_per_second) goto out;	/* bogus! */
    } else if(!strncmp((char *)ptr, "data", 4)) {
      if(data_bytes) goto out;		/* multiple data chunks! */
      data_bytes = m;			/* remember data size */
    }
    m += 8;				/* include chunk header */
    ptr += m;				/* skip chunk */
    n -= m;
  }
  sample_point_size = (bits_per_sample + 7) / 8;
  sample_frame_size = sample_point_size * n_channels;
  if(!sample_frame_size) goto out;	/* bogus or overflow */
  n_samples = data_bytes / sample_frame_size;
  duration = (n_samples + samples_per_second - 1) / samples_per_second;
out:
  munmap(base, length);
  return duration;
}

static const struct {
  const char *ext;
  long (*fn)(const char *path);
} file_formats[] = {
  { ".MP3", tl_mp3 },
  { ".OGG", tl_ogg },
  { ".WAV", tl_wav },
  { ".mp3", tl_mp3 },
  { ".ogg", tl_ogg },
  { ".wav", tl_wav }
};
#define N_FILE_FORMATS (int)(sizeof file_formats / sizeof *file_formats)

long disorder_tracklength(const char attribute((unused)) *track,
			  const char *path) {
  const char *ext = strrchr(path, '.');
  int l, r, m = 0, c = 0;		/* quieten compiler */

  if(ext) {
    l = 0;
    r = N_FILE_FORMATS - 1;
    while(l <= r && (c = strcmp(ext, file_formats[m = (l + r) / 2].ext)))
      if(c < 0)
	r = m - 1;
      else
	l = m + 1;
    if(!c)
      return file_formats[m].fn(path);
  }
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
