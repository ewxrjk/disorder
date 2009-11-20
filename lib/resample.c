/*
 * This file is part of DisOrder.
 * Copyright (C) 2009 Richard Kettlewell
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

/** @file lib/resample.c
 * @brief Audio resampling
 *
 * General purpose audio format conversion.  Rate conversion only works if the
 * SRC samplerate library is available, but the bitness/channel/endianness
 * conversion works regardless.
 */

#include "common.h"
#include "resample.h"
#include "log.h"
#include "mem.h"

/** @brief Number of intermediate-format samples */
#define SAMPLES 1024

/** @brief Multiplier for signed formats to allow easy switching */
#define SIGNED 4

/** @brief Initialize a resampler
 * @param rs Resampler
 * @param input_bits Bits/sample in input
 * @param input_channels Number of input channels
 * @param input_signed Whether input samples are signed or unsigned
 * @param input_rate Frames/second in input
 * @param output_bits Bits/sample in output
 * @param output_channels Number of output channels
 * @param output_rate Frames/second in output
 * @param output_signed Whether output samples are signed or unsigned
 *
 * For formats with more than two channels it's assume that the first
 * two channels are left and right.  No particular meaning is attached
 * to additional channels other than to assume channel N in an input
 * means the same as channel N in an output, for N>1.
 */
void resample_init(struct resampler *rs,
                    int input_bits, int input_channels,
                    int input_rate, int input_signed,
                    int input_endian,
                    int output_bits, int output_channels,
                    int output_rate, int output_signed,
                    int output_endian) {
  memset(rs, 0, sizeof *rs);
  assert(input_bits == 8 || input_bits == 16);
  assert(output_bits == 8 || output_bits == 16);
  assert(input_endian == ENDIAN_BIG || input_endian == ENDIAN_LITTLE);
  assert(output_endian == ENDIAN_BIG || output_endian == ENDIAN_LITTLE);
  assert(ENDIAN_BIG >= 0 && ENDIAN_BIG < SIGNED);
  assert(ENDIAN_LITTLE >= 0 && ENDIAN_LITTLE < SIGNED);
  rs->input_bits = input_bits;
  rs->input_channels = input_channels;
  rs->input_rate = input_rate;
  rs->input_signed = SIGNED * !!input_signed;
  rs->input_endian = input_endian;
  rs->output_bits = output_bits;
  rs->output_channels = output_channels;
  rs->output_rate = output_rate;
  rs->output_signed = SIGNED * !!output_signed;
  rs->output_endian = output_endian;
  rs->input_bytes_per_sample = (rs->input_bits + 7) / 8;
  rs->input_bytes_per_frame = rs->input_channels * rs->input_bytes_per_sample;
  if(rs->input_rate != rs->output_rate) {
#if HAVE_SAMPLERATE_H
    int error_;
    rs->state = src_new(SRC_SINC_BEST_QUALITY, rs->output_channels, &error_);
    if(!rs->state)
      disorder_fatal(0, "calling src_new: %s", src_strerror(error_));
#else
    disorder_fatal(0, "need to resample audio data but libsamplerate not available");
#endif
  }
}

/** @brief Destroy a resampler
 * @param rs Resampler
 */
void resample_close(struct resampler *rs) {
#if HAVE_SAMPLERATE_H
  if(rs->state)
    src_delete(rs->state);
#else
  rs = 0;                               /* quieten compiler */
#endif
}

/** @brief Get one sample value and normalize it to [-1,1]
 * @param rs Resampler state
 * @param bytes Pointer to input data
 * @param where Where to store result
 * @return Number of bytes consumed
 */
static size_t resample_get_sample(const struct resampler *rs,
                                  const uint8_t *bytes,
                                  float *where) {
  switch(rs->input_bits + rs->input_signed + rs->input_endian) {
  case 8+ENDIAN_BIG:
  case 8+ENDIAN_LITTLE:
    *where = (bytes[0] - 128)/ 128.0;
    return 1;
  case 8+SIGNED+ENDIAN_BIG:
  case 8+SIGNED+ENDIAN_LITTLE:
    *where = (int8_t)bytes[0] / 128.0;
    return 1;
  case 16+ENDIAN_BIG:
    *where = (bytes[0] * 256 + bytes[1] - 32768)/ 32768.0;
    return 2;
    break;
  case 16+ENDIAN_LITTLE:
    *where = (bytes[1] * 256 + bytes[0] - 32768)/ 32768.0;
    return 2;
    break;
  case 16+SIGNED+ENDIAN_BIG:
    *where = (int16_t)(bytes[0] * 256 + bytes[1])/ 32768.0;
    return 2;
    break;
  case 16+SIGNED+ENDIAN_LITTLE:
    *where = (int16_t)(bytes[1] * 256 + bytes[0])/ 32768.0;
    return 2;
    break;
  default:
    assert(!"unsupported sample format");
  }
}

static inline int clip(int n, int min, int max) {
  if(n >= min) {
    if(n <= max)
      return n;
    else
      return max;
  } else
    return min;
}

/** @brief Store one sample value
 * @param rs Resampler state
 * @param sample Sample value
 * @param bytes Where to store it
 * @return Number of bytes stored
 *
 * The value is clipped naively if it will not fit.
 */
static size_t resample_put_sample(const struct resampler *rs,
                                  float sample,
                                  uint8_t *bytes) {
  unsigned value;
  switch(rs->output_bits + rs->output_signed + rs->output_endian) {
  case 8+ENDIAN_BIG:
  case 8+ENDIAN_LITTLE:
    *bytes = clip(sample * 128.0 + 128, 0, 255);
    return 1;
  case 8+SIGNED+ENDIAN_BIG:
  case 8+SIGNED+ENDIAN_LITTLE:
    *bytes = clip((int)(sample * 128.0), -128, 127);
    return 1;
  case 16+ENDIAN_BIG:                   /* unsigned */
    value = clip(sample * 32768.0 + 32768, 0, 65535);
    *bytes++ = value >> 8;
    *bytes++ = value;
    return 2;
  case 16+ENDIAN_LITTLE:
    value = clip(sample * 32768.0 + 32768, 0, 65535);
    *bytes++ = value;
    *bytes++ = value >> 8;
    return 2;
  case 16+SIGNED+ENDIAN_BIG:
    value = clip(sample * 32768.0, -32768, 32767);
    *bytes++ = value >> 8;
    *bytes++ = value;
    return 2;
  case 16+SIGNED+ENDIAN_LITTLE:
    value = clip(sample * 32768.0, -32768, 32767);
    *bytes++ = value;
    *bytes++ = value >> 8;
    return 2;
  default:
    assert(!"unsupported sample format");
  }
}

/** @brief Convert input samples to floats
 * @param rs Resampler state
 * @param bytes Input bytes
 * @param nbytes Number of input bytes
 * @param floats Where to store converted data
 *
 * @p floats must be big enough.  As well as converting to floats this
 * also converts to the output's channel format.
 *
 * Excess input channels are just discarded.  If there are insufficient input
 * channels the last one is duplicated as often as necessary to make up the
 * numbers.  This is a rather naff heuristic and may be improved in a future
 * version, but mostly in DisOrder the output is pretty much always stereo and
 * the input either mono or stereo, so the result isn't actually going to be
 * too bad.
 */
static void resample_prepare_input(const struct resampler *rs,
                                   const uint8_t *bytes,
                                   size_t nbytes,
                                   float *floats) {
  size_t nframes = nbytes / (rs->input_bytes_per_frame);

  while(nframes > 0) {
    int n;

    for(n = 0; n < rs->input_channels && n < rs->output_channels; ++n) {
      bytes += resample_get_sample(rs, bytes, floats);
      ++floats;
    }
    if(n < rs->input_channels) {
      /* More input channels; discard them */
      bytes += (rs->input_channels - n) * rs->input_bytes_per_sample;
    } else if(n < rs->output_channels) {
      /* More output channels; duplicate the last input channel */
      for(; n < rs->output_channels; ++n) {
        *floats = floats[-1];
        ++floats;
      }
    }
    --nframes;
  }
}

/** @brief Convert between sample formats
 * @param rs Resampler state
 * @param bytes Bytes to convert
 * @param nbytes Number of bytes to convert
 * @param eof Set an end of input stream
 * @param converted Called with converted data (possibly more than once)
 * @param cd Passed to @p cd
 * @return Number of bytes consumed
 */
size_t resample_convert(const struct resampler *rs,
                        const uint8_t *bytes,
                        size_t nbytes,
                        int eof,
                        void (*converted)(uint8_t *bytes,
                                          size_t nbytes,
                                          void *cd),
                        void *cd) {
  size_t nframesin = nbytes / (rs->input_bytes_per_frame);
  size_t nsamplesout;
  float *input = xcalloc(nframesin * rs->output_channels, sizeof (float));
  float *output = 0;

  resample_prepare_input(rs, bytes, nbytes, input);
#if HAVE_SAMPLERATE_H
  if(rs->state) {
    /* A sample-rate conversion must be performed */
    SRC_DATA data;
    /* Compute how many frames are expected to come out. */
    size_t maxframesout = nframesin * rs->output_rate / rs->input_rate + 1;
    output = xcalloc(maxframesout * rs->output_channels, sizeof(float));
    data.data_in = input;
    data.data_out = output;
    data.input_frames = nframesin;
    data.output_frames = maxframesout;
    data.end_of_input = eof;
    data.src_ratio = rs->output_rate / rs->input_rate;
    int error_ = src_process(rs->state, &data);
    if(error_)
      disorder_fatal(0, "calling src_process: %s", src_strerror(error_));
    nframesin = data.input_frames_used;
    nsamplesout = data.output_frames_gen * rs->output_channels;
  }
#endif
  if(!output) {
    /* No sample-rate conversion required */
    output = input;
    nsamplesout = nframesin * rs->output_channels;
  }
  const float *op = output;
  while(nsamplesout > 0) {
    uint8_t buffer[4096];
    size_t bufused = 0;

    while(bufused < sizeof buffer && nsamplesout > 0) {
      bufused += resample_put_sample(rs, *op++, buffer + bufused);
      --nsamplesout;
    }
    converted(buffer, bufused, cd);
  }
  if(output != input)
    xfree(output);
  xfree(input);
  eof = 0;             /* quieten compiler */
  /* Report how many input bytes were actually consumed */
  return nframesin * rs->input_bytes_per_frame;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
