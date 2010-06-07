/*
 * This file is part of DisOrder
 * Copyright (C) 2007-2009 Richard Kettlewell
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
/** @file server/decode.c
 * @brief General-purpose decoder for use by speaker process
 */

#include "disorder-server.h"
#include "hreader.h"

#include <mad.h>
#include <vorbis/vorbisfile.h>

#include <FLAC/stream_decoder.h>

#include "wav.h"
#include "speaker-protocol.h"


/** @brief Encoding lookup table type */
struct decoder {
  /** @brief Glob pattern matching file */
  const char *pattern;
  /** @brief Decoder function */
  void (*decode)(void);
};

static struct hreader input[1];

/** @brief Output file */
static FILE *outputfp;

/** @brief Filename */
static const char *path;

/** @brief Input buffer */
static char input_buffer[1048576];

/** @brief Number of bytes read into buffer */
static int input_count;

/** @brief Write an 8-bit word */
static inline void output_8(int n) {
  if(putc(n, outputfp) < 0)
    disorder_fatal(errno, "decoding %s: output error", path);
}

/** @brief Write a 16-bit word in bigendian format */
static inline void output_16(uint16_t n) {
  if(putc(n >> 8, outputfp) < 0
     || putc(n, outputfp) < 0)
    disorder_fatal(errno, "decoding %s: output error", path);
}

/** @brief Write a 24-bit word in bigendian format */
static inline void output_24(uint32_t n) {
  if(putc(n >> 16, outputfp) < 0
     || putc(n >> 8, outputfp) < 0
     || putc(n, outputfp) < 0)
    disorder_fatal(errno, "decoding %s: output error", path);
}

/** @brief Write a 32-bit word in bigendian format */
static inline void output_32(uint32_t n) {
  if(putc(n >> 24, outputfp) < 0
     || putc(n >> 16, outputfp) < 0
     || putc(n >> 8, outputfp) < 0
     || putc(n, outputfp) < 0)
    disorder_fatal(errno, "decoding %s: output error", path);
}

/** @brief Write a block header
 * @param rate Sample rate in Hz
 * @param channels Channel count (currently only 1 or 2 supported)
 * @param bits Bits per sample (must be a multiple of 8, no more than 64)
 * @param nbytes Total number of data bytes
 * @param endian @ref ENDIAN_BIG or @ref ENDIAN_LITTLE
 *
 * Checks that the sample format is a supported one (so other calls do not have
 * to) and calls disorder_fatal() on error.
 */
static void output_header(int rate,
			  int channels,
			  int bits,
                          int nbytes,
                          int endian) {
  struct stream_header header;

  if(bits <= 0 || bits % 8 || bits > 64)
    disorder_fatal(0, "decoding %s: unsupported sample size %d bits",
                   path, bits);
  if(channels <= 0 || channels > 2)
    disorder_fatal(0, "decoding %s: unsupported channel count %d",
                   path, channels);
  if(rate <= 0)
    disorder_fatal(0, "decoding %s: nonsensical sample rate %dHz", path, rate);
  header.rate = rate;
  header.bits = bits;
  header.channels = channels;
  header.endian = endian;
  header.nbytes = nbytes;
  if(fwrite(&header, sizeof header, 1, outputfp) < 1)
    disorder_fatal(errno, "decoding %s: writing format header", path);
}

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
static void decode_mp3(void) {
  struct mad_decoder mad[1];

  if(hreader_init(path, input))
    disorder_fatal(errno, "opening %s", path);
  mad_decoder_init(mad, 0/*data*/, mp3_input, 0/*header*/, 0/*filter*/,
		   mp3_output, mp3_error, 0/*message*/);
  if(mad_decoder_run(mad, MAD_DECODER_MODE_SYNC))
    exit(1);
  mad_decoder_finish(mad);
}

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
static void decode_ogg(void) {
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

/** @brief Sample data callback used by decode_wav() */
static int wav_write(struct wavfile attribute((unused)) *f,
                     const char *data,
                     size_t nbytes,
                     void attribute((unused)) *u) {
  if(fwrite(data, 1, nbytes, outputfp) < nbytes)
    disorder_fatal(errno, "decoding %s: writing sample data", path);
  return 0;
}

/** @brief WAV file decoder */
static void decode_wav(void) {
  struct wavfile f[1];
  int err;

  if((err = wav_init(f, path)))
    disorder_fatal(err, "opening %s", path);
  output_header(f->rate, f->channels, f->bits, f->datasize, ENDIAN_LITTLE);
  if((err = wav_data(f, wav_write, 0)))
    disorder_fatal(err, "error decoding %s", path);
}

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
static void decode_flac(void) {
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

/** @brief Lookup table of decoders */
static const struct decoder decoders[] = {
  { "*.mp3", decode_mp3 },
  { "*.MP3", decode_mp3 },
  { "*.ogg", decode_ogg },
  { "*.OGG", decode_ogg },
  { "*.flac", decode_flac },
  { "*.FLAC", decode_flac },
  { "*.wav", decode_wav },
  { "*.WAV", decode_wav },
  { 0, 0 }
};

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { 0, 0, 0, 0 }
};

/* Display usage message and terminate. */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-decode [OPTIONS] PATH\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "\n"
	  "Audio decoder for DisOrder.  Only intended to be used by speaker\n"
	  "process, not for normal users.\n");
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  const char *e;

  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "calling setlocale");
  while((n = getopt_long(argc, argv, "hV", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-decode");
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(optind >= argc)
    disorder_fatal(0, "missing filename");
  if(optind + 1 < argc)
    disorder_fatal(0, "excess arguments");
  if((e = getenv("DISORDER_RAW_FD"))) {
    if(!(outputfp = fdopen(atoi(e), "wb")))
      disorder_fatal(errno, "fdopen");
  } else
    outputfp = stdout;
  path = argv[optind];
  for(n = 0;
      decoders[n].pattern
	&& fnmatch(decoders[n].pattern, path, 0) != 0;
      ++n)
    ;
  if(!decoders[n].pattern)
    disorder_fatal(0, "cannot determine file type for %s", path);
  decoders[n].decode();
  xfclose(outputfp);
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
