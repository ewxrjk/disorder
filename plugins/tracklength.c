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
/* libFLAC has had an API change and stupidly taken away the old API */
#if HAVE_FLAC_FILE_DECODER_H
# include <FLAC/file_decoder.h>
#else
# include <FLAC/stream_decoder.h>
#define FLAC__FileDecoder FLAC__StreamDecoder
#define FLAC__FileDecoderState FLAC__StreamDecoderState
#endif


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

/* libFLAC's "simplified" interface is rather heavyweight... */

struct flac_state {
  long duration;
  const char *path;
};

static void flac_metadata(const FLAC__FileDecoder attribute((unused)) *decoder,
			  const FLAC__StreamMetadata *metadata,
			  void *client_data) {
  struct flac_state *const state = client_data;
  const FLAC__StreamMetadata_StreamInfo *const stream_info
    = &metadata->data.stream_info;

  if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    /* FLAC uses 0 to mean unknown and conveniently so do we */
    state->duration = (stream_info->total_samples
		       + stream_info->sample_rate - 1)
           / stream_info->sample_rate;
}

static void flac_error(const FLAC__FileDecoder attribute((unused)) *decoder,
		       FLAC__StreamDecoderErrorStatus status,
		       void *client_data) {
  const struct flac_state *const state = client_data;

  disorder_error(0, "error decoding %s: %s", state->path,
		 FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus flac_write
    (const FLAC__FileDecoder attribute((unused)) *decoder,
     const FLAC__Frame attribute((unused)) *frame,
     const FLAC__int32 attribute((unused)) *const buffer_[],
     void attribute((unused)) *client_data) {
  const struct flac_state *const state = client_data;

  if(state->duration >= 0)
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  else
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static long tl_flac(const char *path) {
  struct flac_state state[1];

  state->duration = -1;			/* error */
  state->path = path;
#if HAVE_FLAC_FILE_DECODER_H 
  {
    FLAC__FileDecoder *fd = 0;
    FLAC__FileDecoderState fs;
    
    if(!(fd = FLAC__file_decoder_new())) {
      disorder_error(0, "FLAC__file_decoder_new failed");
      goto fail;
    }
    if(!(FLAC__file_decoder_set_filename(fd, path))) {
      disorder_error(0, "FLAC__file_set_filename failed");
      goto fail;
    }
    FLAC__file_decoder_set_metadata_callback(fd, flac_metadata);
    FLAC__file_decoder_set_error_callback(fd, flac_error);
    FLAC__file_decoder_set_write_callback(fd, flac_write);
    FLAC__file_decoder_set_client_data(fd, state);
    if((fs = FLAC__file_decoder_init(fd))) {
      disorder_error(0, "FLAC__file_decoder_init: %s",
		     FLAC__FileDecoderStateString[fs]);
      goto fail;
    }
    FLAC__file_decoder_process_until_end_of_metadata(fd);
fail:
    if(fd)
      FLAC__file_decoder_delete(fd);
  }
#else
  {
    FLAC__StreamDecoder *sd = 0;
    FLAC__StreamDecoderInitStatus is;
    
    if(!(sd = FLAC__stream_decoder_new())) {
      disorder_error(0, "FLAC__stream_decoder_new failed");
      goto fail;
    }
    if((is = FLAC__stream_decoder_init_file(sd, path, flac_write, flac_metadata,
					    flac_error, state))) {
      disorder_error(0, "FLAC__stream_decoder_init_file %s: %s",
		     path, FLAC__StreamDecoderInitStatusString[is]);
      goto fail;
    }
    FLAC__stream_decoder_process_until_end_of_metadata(sd);
fail:
    if(sd)
      FLAC__stream_decoder_delete(sd);
  }
#endif
  return state->duration;
}

static const struct {
  const char *ext;
  long (*fn)(const char *path);
} file_formats[] = {
  { ".FLAC", tl_flac },
  { ".MP3", tl_mp3 },
  { ".OGG", tl_ogg },
  { ".WAV", tl_wav },
  { ".flac", tl_flac },
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
