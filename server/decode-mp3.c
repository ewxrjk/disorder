/*
 * This file is part of DisOrder
 * Copyright (C) 2007-2010 Richard Kettlewell
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
/** @file server/decode-mp3.c
 * @brief Decode MP3 files.
 */
#include "decode.h"
#include <mad.h>

static struct hreader input[1];

/** @brief Dithering state
 * Filched from mpg321, which credits it to Robert Leslie */
struct audio_dither {
  mad_fixed_t error[3];
  mad_fixed_t random;
};

/** @brief 32-bit PRNG
 * Filched from mpg321, which credits it to Robert Leslie */
static inline unsigned long prng(unsigned long state)
{
  return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

/** @brief Generic linear sample quantize and dither routine
 * Filched from mpg321, which credits it to Robert Leslie */
static long audio_linear_dither(mad_fixed_t sample,
				struct audio_dither *dither) {
  unsigned int scalebits;
  mad_fixed_t output, mask, rnd;
  const int bits = 16;

  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  /* noise shape */
  sample += dither->error[0] - dither->error[1] + dither->error[2];

  dither->error[2] = dither->error[1];
  dither->error[1] = dither->error[0] / 2;

  /* bias */
  output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

  scalebits = MAD_F_FRACBITS + 1 - bits;
  mask = (1L << scalebits) - 1;

  /* dither */
  rnd  = prng(dither->random);
  output += (rnd & mask) - (dither->random & mask);

  dither->random = rnd;

  /* clip */
  if (output > MAX) {
    output = MAX;

    if (sample > MAX)
      sample = MAX;
  }
  else if (output < MIN) {
    output = MIN;

    if (sample < MIN)
      sample = MIN;
  }

  /* quantize */
  output &= ~mask;

  /* error feedback */
  dither->error[0] = sample - output;

  /* scale */
  return output >> scalebits;
}

/** @brief MP3 output callback */
static enum mad_flow mp3_output(void attribute((unused)) *data,
				struct mad_header const *header,
				struct mad_pcm *pcm) {
  size_t n = pcm->length;
  const mad_fixed_t *l = pcm->samples[0], *r = pcm->samples[1];
  static struct audio_dither ld[1], rd[1];

  output_header(header->samplerate,
		pcm->channels,
		16,
                2 * pcm->channels * pcm->length,
                ENDIAN_BIG);
  switch(pcm->channels) {
  case 1:
    while(n--)
      output_16(audio_linear_dither(*l++, ld));
    break;
  case 2:
    while(n--) {
      output_16(audio_linear_dither(*l++, ld));
      output_16(audio_linear_dither(*r++, rd));
    }
    break;
  }
  return MAD_FLOW_CONTINUE;
}

/** @brief MP3 input callback */
static enum mad_flow mp3_input(void attribute((unused)) *data,
			       struct mad_stream *stream) {
  int used, remain, n;

  /* libmad requires its caller to do ALL the buffering work, including coping
   * with partial frames.  Given that it appears to be completely undocumented
   * you could perhaps be forgiven for not discovering this...  */
  if(input_count) {
    /* Compute total number of bytes consumed */
    used = (char *)stream->next_frame - input_buffer;
    /* Compute number of bytes left to consume */
    remain = input_count - used;
    memmove(input_buffer, input_buffer + used, remain);
  } else {
    remain = 0;
  }
  /* Read new data */
  n = hreader_read(input,
                   input_buffer + remain, 
                   (sizeof input_buffer) - remain);
  if(n < 0)
    disorder_fatal(errno, "reading from %s", path);
  /* Compute total number of bytes available */
  input_count = remain + n;
  if(input_count)
    mad_stream_buffer(stream, (unsigned char *)input_buffer, input_count);
  if(n)
    return MAD_FLOW_CONTINUE;
  else
    return MAD_FLOW_STOP;
}

/** @brief MP3 error callback */
static enum mad_flow mp3_error(void attribute((unused)) *data,
			       struct mad_stream *stream,
			       struct mad_frame attribute((unused)) *frame) {
  if(0)
    /* Just generates pointless verbosity l-( */
    disorder_error(0, "decoding %s: %s (%#04x)",
                   path, mad_stream_errorstr(stream), stream->error);
  return MAD_FLOW_CONTINUE;
}

/** @brief MP3 decoder */
void decode_mp3(void) {
  struct mad_decoder mad[1];

  if(hreader_init(path, input))
    disorder_fatal(errno, "opening %s", path);
  mad_decoder_init(mad, 0/*data*/, mp3_input, 0/*header*/, 0/*filter*/,
		   mp3_output, mp3_error, 0/*message*/);
  if(mad_decoder_run(mad, MAD_DECODER_MODE_SYNC))
    exit(1);
  mad_decoder_finish(mad);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
