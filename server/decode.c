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
/** @file server/decode.c
 * @brief General-purpose decoder for use by speaker process
 */
#include "decode.h"

#include <mad.h>
#include <vorbis/vorbisfile.h>

#include <FLAC/stream_decoder.h>

#include "wav.h"


/** @brief Encoding lookup table type */
struct decoder {
  /** @brief Glob pattern matching file */
  const char *pattern;
  /** @brief Decoder function */
  void (*decode)(void);
};

FILE *outputfp;
const char *path;
char input_buffer[INPUT_BUFFER_SIZE];
int input_count;

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
void output_header(int rate,
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
