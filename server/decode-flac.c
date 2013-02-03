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
/** @file server/decode-flac.c
 * @brief General-purpose decoder for use by speaker process
 */
#include "decode.h"
#include <FLAC/stream_decoder.h>

/** @brief Metadata callback for FLAC decoder
 *
 * This is a no-op here.
 */
static void flac_metadata(const FLAC__StreamDecoder attribute((unused)) *decoder,
			  const FLAC__StreamMetadata attribute((unused)) *metadata,
			  void attribute((unused)) *client_data) {
}

/** @brief Error callback for FLAC decoder */
static void flac_error(const FLAC__StreamDecoder attribute((unused)) *decoder,
		       FLAC__StreamDecoderErrorStatus status,
		       void attribute((unused)) *client_data) {
  disorder_fatal(0, "error decoding %s: %s", path,
                 FLAC__StreamDecoderErrorStatusString[status]);
}

/** @brief Write callback for FLAC decoder */
static FLAC__StreamDecoderWriteStatus flac_write
    (const FLAC__StreamDecoder attribute((unused)) *decoder,
     const FLAC__Frame *frame,
     const FLAC__int32 *const buffer[],
     void attribute((unused)) *client_data) {
  size_t n, c;

  output_header(frame->header.sample_rate,
                frame->header.channels,
                frame->header.bits_per_sample,
                (frame->header.channels * frame->header.blocksize
                 * frame->header.bits_per_sample) / 8,
                ENDIAN_BIG);
  for(n = 0; n < frame->header.blocksize; ++n) {
    for(c = 0; c < frame->header.channels; ++c) {
      switch(frame->header.bits_per_sample) {
      case 8: output_8(buffer[c][n]); break;
      case 16: output_16(buffer[c][n]); break;
      case 24: output_24(buffer[c][n]); break;
      case 32: output_32(buffer[c][n]); break;
      }
    }
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static FLAC__StreamDecoderReadStatus flac_read(const FLAC__StreamDecoder attribute((unused)) *decoder,
                                               FLAC__byte buffer[],
                                               size_t *bytes,
                                               void *client_data) {
  struct hreader *flacinput = client_data;
  int n = hreader_read(flacinput, buffer, *bytes);
  if(n == 0) {
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
  if(n < 0) {
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  *bytes = n;
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus flac_seek(const FLAC__StreamDecoder attribute((unused)) *decoder,
                                               FLAC__uint64 absolute_byte_offset, 
                                               void *client_data) {
  struct hreader *flacinput = client_data;
  if(hreader_seek(flacinput, absolute_byte_offset, SEEK_SET) < 0)
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
  else
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus flac_tell(const FLAC__StreamDecoder attribute((unused)) *decoder, 
                                               FLAC__uint64 *absolute_byte_offset,
                                               void *client_data) {
  struct hreader *flacinput = client_data;
  off_t offset = hreader_seek(flacinput, 0, SEEK_CUR);
  if(offset < 0)
    return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
  *absolute_byte_offset = offset;
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus flac_length(const FLAC__StreamDecoder attribute((unused)) *decoder, 
                                                   FLAC__uint64 *stream_length, 
                                                   void *client_data) {
  struct hreader *flacinput = client_data;
  *stream_length = hreader_size(flacinput);
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_eof(const FLAC__StreamDecoder attribute((unused)) *decoder, 
                           void *client_data) {
  struct hreader *flacinput = client_data;
  return hreader_eof(flacinput);
}

/** @brief FLAC file decoder */
void decode_flac(void) {
  FLAC__StreamDecoder *sd = FLAC__stream_decoder_new();
  FLAC__StreamDecoderInitStatus is;
  struct hreader flacinput[1];

  if (!sd)
    disorder_fatal(0, "FLAC__stream_decoder_new failed");
  if(hreader_init(path, flacinput))
    disorder_fatal(errno, "error opening %s", path);

  if((is = FLAC__stream_decoder_init_stream(sd,
                                            flac_read,
                                            flac_seek,
                                            flac_tell,
                                            flac_length,
                                            flac_eof,
                                            flac_write, flac_metadata,
                                            flac_error, 
                                            flacinput)))
    disorder_fatal(0, "FLAC__stream_decoder_init_stream %s: %s",
                   path, FLAC__StreamDecoderInitStatusString[is]);

  FLAC__stream_decoder_process_until_end_of_stream(sd);
  FLAC__stream_decoder_finish(sd);
  FLAC__stream_decoder_delete(sd);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
