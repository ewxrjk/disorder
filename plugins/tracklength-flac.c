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
/** @file plugins/tracklength-flac.c
 * @brief Compute track lengths for FLAC files
 */
#include "tracklength.h"
#include <FLAC/stream_decoder.h>

/* libFLAC's "simplified" interface is rather heavyweight... */

/** @brief State used when computing FLAC file length */
struct flac_state {
  /** @brief Duration or -1 */
  long duration;

  /** @brief File being analyzed */
  const char *path;
};

static void flac_metadata(const FLAC__StreamDecoder attribute((unused)) *decoder,
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

static void flac_error(const FLAC__StreamDecoder attribute((unused)) *decoder,
		       FLAC__StreamDecoderErrorStatus status,
		       void *client_data) {
  const struct flac_state *const state = client_data;

  disorder_error(0, "error decoding %s: %s", state->path,
		 FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus flac_write
    (const FLAC__StreamDecoder attribute((unused)) *decoder,
     const FLAC__Frame attribute((unused)) *frame,
     const FLAC__int32 attribute((unused)) *const buffer_[],
     void attribute((unused)) *client_data) {
  const struct flac_state *const state = client_data;

  if(state->duration >= 0)
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  else
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

long tl_flac(const char *path) {
  FLAC__StreamDecoder *sd = 0;
  FLAC__StreamDecoderInitStatus is;
  struct flac_state state[1];

  state->duration = -1;			/* error */
  state->path = path;
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
  return state->duration;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
