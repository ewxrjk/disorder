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
/** @file server/decode.h
 * @brief General-purpose decoder for use by speaker process
 */
#ifndef DECODE_H
#define DECODE_H

#include "disorder-server.h"
#include "hreader.h"
#include "speaker-protocol.h"

#define INPUT_BUFFER_SIZE 1048576
  
/** @brief Output file */
extern FILE *outputfp;

/** @brief Input filename */
extern const char *path;

/** @brief Input buffer */
extern char input_buffer[INPUT_BUFFER_SIZE];

/** @brief Number of bytes read into buffer */
extern int input_count;

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

void output_header(int rate,
                   int channels,
                   int bits,
                   int nbytes,
                   int endian);

void decode_mp3(void);
void decode_ogg(void);
void decode_wav(void);
void decode_flac(void);

#endif /* DECODE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
