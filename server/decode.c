/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file server/decode.c
 * @brief General-purpose decoder for use by speaker process
 */

#include <config.h>
#include "types.h"

#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <locale.h>
#include <assert.h>
#include <fnmatch.h>
#include <mad.h>
#include <vorbis/vorbisfile.h>

#include "log.h"
#include "syscalls.h"
#include "defs.h"
#include "speaker-protocol.h"

/** @brief Encoding lookup table type */
struct decoder {
  /** @brief Glob pattern matching file */
  const char *pattern;
  /** @brief Decoder function */
  void (*decode)(void);
};

/** @brief Input file */
static int inputfd;

/** @brief Output file */
static FILE *outputfp;

/** @brief Filename */
static const char *path;

/** @brief Input buffer */
static char buffer[1048576];

/** @brief Open the input file */
static void open_input(void) {
  if((inputfd = open(path, O_RDONLY)) < 0)
    fatal(errno, "opening %s", path);
}

/** @brief Fill the buffer
 * @return Number of bytes read
 */
static size_t fill(void) {
  int n = read(inputfd, buffer, sizeof buffer);

  if(n < 0)
    fatal(errno, "reading from %s", path);
  return n;
}

/** @brief Write a 16-bit word in bigendian format */
static inline void output_16(uint16_t n) {
  if(putc(n >> 8, outputfp) < 0
     || putc(n & 0xFF, outputfp) < 0)
    fatal(errno, "decoding %s: output error", path);
}

/** @brief Write the header
 * If called more than once, either does nothing (if you kept the same
 * output encoding) or fails (if you changed it).
 */
static void output_header(int rate,
			  int channels,
			  int bits,
                          int nbytes) {
  struct stream_header header;

  header.rate = rate;
  header.bits = bits;
  header.channels = channels;
  header.endian = ENDIAN_BIG;
  header.nbytes = nbytes;
  if(fwrite(&header, sizeof header, 1, outputfp) < 1)
    fatal(errno, "decoding %s: writing format header", path);
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
#define bits 16
static long audio_linear_dither(mad_fixed_t sample,
				struct audio_dither *dither) {
  unsigned int scalebits;
  mad_fixed_t output, mask, rnd;

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
#undef bits

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
                2 * pcm->channels * pcm->length);
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
  default:
    fatal(0, "decoding %s: unsupported channel count %d", path, pcm->channels);
  }
  return MAD_FLOW_CONTINUE;
}

/** @brief MP3 input callback */
static enum mad_flow mp3_input(void attribute((unused)) *data,
			       struct mad_stream *stream) {
  const size_t n = fill();

  if(!n)
    return MAD_FLOW_STOP;
  mad_stream_buffer(stream, (unsigned char *)buffer, n);
  return MAD_FLOW_CONTINUE;
}


/** @brief MP3 error callback */
static enum mad_flow mp3_error(void attribute((unused)) *data,
			       struct mad_stream *stream,
			       struct mad_frame attribute((unused)) *frame) {
  if(0)
    /* Just generates pointless verbosity l-( */
    error(0, "decoding %s: %s (%#04x)",
          path, mad_stream_errorstr(stream), stream->error);
  return MAD_FLOW_CONTINUE;
}

/** @brief MP3 decoder */
static void decode_mp3(void) {
  struct mad_decoder mad[1];

  open_input();
  mad_decoder_init(mad, 0/*data*/, mp3_input, 0/*header*/, 0/*filter*/,
		   mp3_output, mp3_error, 0/*message*/);
  if(mad_decoder_run(mad, MAD_DECODER_MODE_SYNC))
    exit(1);
  mad_decoder_finish(mad);
}

/** @brief OGG decoder */
static void decode_ogg(void) {
  FILE *fp;
  OggVorbis_File vf[1];
  int err;
  long n;
  int bitstream;
  vorbis_info *vi;

  if(!(fp = fopen(path, "rb")))
    fatal(errno, "cannot open %s", path);
  /* There doesn't seem to be any standard function for mapping the error codes
   * to strings l-( */
  if((err = ov_open(fp, vf, 0/*initial*/, 0/*ibytes*/)))
    fatal(0, "ov_fopen %s: %d", path, err);
  if(!(vi = ov_info(vf, 0/*link*/)))
    fatal(0, "ov_info %s: failed", path);
  while((n = ov_read(vf, buffer, sizeof buffer, 1/*bigendianp*/,
                     2/*bytes/word*/, 1/*signed*/, &bitstream))) {
    if(n < 0)
      fatal(0, "ov_read %s: %ld", path, n);
    if(bitstream > 0)
      fatal(0, "only single-bitstream ogg files are supported");
    output_header(vi->rate, vi->channels, 16/*bits*/, n);
    if(fwrite(buffer, 1, n, outputfp) < (size_t)n)
      fatal(errno, "decoding %s: writing sample data", path);
  }
}

/** @brief Lookup table of decoders */
static const struct decoder decoders[] = {
  { "*.mp3", decode_mp3 },
  { "*.MP3", decode_mp3 },
  { "*.ogg", decode_ogg },
  { "*.OGG", decode_ogg },
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

/* Display version number and terminate. */
static void version(void) {
  xprintf("disorder-decode version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  const char *e;

  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "calling setlocale");
  while((n = getopt_long(argc, argv, "hV", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    default: fatal(0, "invalid option");
    }
  }
  if(optind >= argc)
    fatal(0, "missing filename");
  if(optind + 1 < argc)
    fatal(0, "excess arguments");
  if((e = getenv("DISORDER_RAW_FD"))) {
    if(!(outputfp = fdopen(atoi(e), "wb")))
      fatal(errno, "fdopen");
  } else
    outputfp = stdout;
  path = argv[optind];
  for(n = 0;
      decoders[n].pattern
	&& fnmatch(decoders[n].pattern, path, 0) != 0;
      ++n)
    ;
  if(!decoders[n].pattern)
    fatal(0, "cannot determine file type for %s", path);
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
