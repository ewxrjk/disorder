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
#include "common.h"

#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <getopt.h>

#include "resample.h"
#include "mem.h"
#include "syscalls.h"
#include "log.h"

static int input_bits = 16;
static int input_channels = 2;
static int input_rate = 44100;
static int input_signed = 1;
static int input_endian = ENDIAN_NATIVE;
static int output_bits = 16;
static int output_channels = 2;
static int output_rate = 44100;
static int output_signed = 1;
static int output_endian = ENDIAN_NATIVE;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "input-bits", required_argument, 0, 'b' },
  { "input-channels", required_argument, 0, 'c' },
  { "input-rate", required_argument, 0, 'r' },
  { "input-signed", no_argument, 0, 's' },
  { "input-unsigned", no_argument, 0, 'u' },
  { "input-endian", required_argument, 0, 'e' },
  { "output-bits", required_argument, 0, 'B' },
  { "output-channels", required_argument, 0, 'C' },
  { "output-rate", required_argument, 0, 'R' },
  { "output-signed", no_argument, 0, 'S' },
  { "output-unsigned", no_argument, 0, 'U' },
  { "output-endian", required_argument, 0, 'E' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  resample [OPTIONS] < INPUT > OUTPUT\n"
	  "Options:\n"
	  "  --help, -h                      Display usage message\n"
          "Input format:\n"
	  "  --input-bits, -b N              Bits/sample (16)\n"
	  "  --input-channels, -c N          Samples/frame (2)\n"
	  "  --input-rate, -r N              Frames/second (44100)\n"
	  "  --input-signed, -s              Signed samples (yes)\n"
	  "  --input-unsigned, -u            Unsigned samples\n"
	  "  --input-endian, -e big|little   Sample endianness (native)\n"
          "Output format:\n"
	  "  --output-bits, -B N             Bits/sample (16)\n"
	  "  --output-channels, -C N         Samples/frame (2)\n"
	  "  --output-rate, -R N             Frames/second (44100)\n"
	  "  --output-signed, -S             Signed samples (yes)\n"
	  "  --output-unsigned, -U           Unsigned samples\n"
	  "  --output-endian, -E big|little  Sample endianness (native)\n"
          "Defaults are in brackets.\n"
          "\n"
          "Feeds raw sample data through resample_convert().\n");
  xfclose(stdout);
  exit(0);
}

static void converted(uint8_t *bytes,
                      size_t nbytes,
                      void attribute((unused)) *cd) {
  while(nbytes > 0) {
    ssize_t n = write(1, bytes, nbytes);
    if(n < 0)
      disorder_fatal(errno, "writing to stdout");
    bytes += n;
    nbytes -= n;
  }
}

int main(int argc, char **argv) {
  int n;

  mem_init();
  if(!setlocale(LC_CTYPE, "")) 
    disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "+hb:c:r:sue:B:C:R:SUE:", 
                         options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'b': input_bits = atoi(optarg); break;
    case 'c': input_channels = atoi(optarg); break;
    case 'r': input_rate = atoi(optarg); break;
    case 's': input_signed = 1; break;
    case 'u': input_signed = 1; break;
    case 'e':
      switch(optarg[0]) {
      case 'b': case 'B': input_endian = ENDIAN_BIG; break;
      case 'l': case 'L': input_endian = ENDIAN_LITTLE; break;
      case 'n': case 'N': input_endian = ENDIAN_NATIVE; break;
      default: disorder_fatal(0, "unknown endianness '%s'", optarg);
      }
      break;
    case 'B': output_bits = atoi(optarg); break;
    case 'C': output_channels = atoi(optarg); break;
    case 'R': output_rate = atoi(optarg); break;
    case 'S': output_signed = 1; break;
    case 'U': output_signed = 1; break;
    case 'E':
      switch(optarg[0]) {
      case 'b': case 'B': output_endian = ENDIAN_BIG; break;
      case 'l': case 'L': output_endian = ENDIAN_LITTLE; break;
      case 'n': case 'N': output_endian = ENDIAN_NATIVE; break;
      default: disorder_fatal(0, "unknown endianness '%s'", optarg);
      }
      break;
    default: fatal(0, "invalid option");
    }
  }
  struct resampler rs[1];
  resample_init(rs, input_bits, input_channels, input_rate, input_signed,
                input_endian, output_bits, output_channels, output_rate,
                output_signed, output_endian);
#define BUFFER_SIZE (1024 * 1024)
  uint8_t *buffer = xmalloc_noptr(BUFFER_SIZE);
  size_t used = 0;
  int eof = 0;

  while(used || !eof) {
    if(!eof) {
      ssize_t r = read(0, buffer + used, BUFFER_SIZE - used);
      if(r < 0)
        disorder_fatal(errno, "reading from stdin");
      if(r == 0)
        eof = 1;
      used += r;
    }
    size_t consumed = resample_convert(rs, buffer, used, eof, converted, 0);
    memmove(buffer, buffer + consumed, used - consumed);
    used -= consumed;
  }
  if(close(1) < 0)
    disorder_fatal(errno, "closing stdout");
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
