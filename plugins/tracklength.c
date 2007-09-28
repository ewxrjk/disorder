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
#include "wav.h"

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
