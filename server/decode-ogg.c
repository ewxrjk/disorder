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
/** @file server/decode-ogg.c
 * @brief General-purpose decoder for use by speaker process
 */
#include "decode.h"

#include <vorbis/vorbisfile.h>

static size_t ogg_read_func(void *ptr, size_t size, size_t nmemb, void *datasource) {
  struct hreader *h = datasource;
  
  int n = hreader_read(h, ptr, size * nmemb);
  if(n < 0) n = 0;
  return n / size;
}

static int ogg_seek_func(void *datasource, ogg_int64_t offset, int whence) {
  struct hreader *h = datasource;
  
  return hreader_seek(h, offset, whence) < 0 ? -1 : 0;
}

static int ogg_close_func(void attribute((unused)) *datasource) {
  return 0;
}

static long ogg_tell_func(void *datasource) {
  struct hreader *h = datasource;
  
  return hreader_seek(h, 0, SEEK_CUR);
}

static const ov_callbacks ogg_callbacks = {
  ogg_read_func,
  ogg_seek_func,
  ogg_close_func,
  ogg_tell_func,
};

/** @brief OGG decoder */
void decode_ogg(void) {
  struct hreader ogginput[1];
  OggVorbis_File vf[1];
  int err;
  long n;
  int bitstream;
  vorbis_info *vi;

  hreader_init(path, ogginput);
  /* There doesn't seem to be any standard function for mapping the error codes
   * to strings l-( */
  if((err = ov_open_callbacks(ogginput, vf, 0/*initial*/, 0/*ibytes*/,
                              ogg_callbacks)))
    disorder_fatal(0, "ov_open_callbacks %s: %d", path, err);
  if(!(vi = ov_info(vf, 0/*link*/)))
    disorder_fatal(0, "ov_info %s: failed", path);
  while((n = ov_read(vf, input_buffer, sizeof input_buffer, 1/*bigendianp*/,
                     2/*bytes/word*/, 1/*signed*/, &bitstream))) {
    if(n < 0)
      disorder_fatal(0, "ov_read %s: %ld", path, n);
    if(bitstream > 0)
      disorder_fatal(0, "only single-bitstream ogg files are supported");
    output_header(vi->rate, vi->channels, 16/*bits*/, n, ENDIAN_BIG);
    if(fwrite(input_buffer, 1, n, outputfp) < (size_t)n)
      disorder_fatal(errno, "decoding %s: writing sample data", path);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
