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
#include "test.h"
#include "resample.h"
#include "vector.h"

/* Accumulate converted bytes in a dynamic string */
static void converted(uint8_t *bytes,
                      size_t nbytes,
                      void *cd) {
  struct dynstr *d = cd;
  dynstr_append_bytes(d, (void *)bytes, nbytes);
}

/* Converter wrapper */
static uint8_t *convert(const struct resampler *rs,
                        const uint8_t *input, size_t input_bytes,
                        size_t *output_bytes) {
  struct dynstr d[1];

  dynstr_init(d);
  while(input_bytes > 0) {
    size_t chunk = input_bytes > 1024 ? 1024 : input_bytes;
    size_t consumed = resample_convert(rs,
                                       input, input_bytes,
                                       input_bytes == chunk,
                                       converted,
                                       d);
    input += consumed;
    input_bytes -= consumed;
  }
  *output_bytes = d->nvec;
  return (uint8_t *)d->vec;
}

static const struct {
  const char *description;
  int input_bits;
  int input_channels;
  int input_rate;
  int input_signed;
  int input_endian;
  const char *input;
  size_t input_bytes;
  int output_bits;
  int output_channels;
  int output_rate;
  int output_signed;
  int output_endian;
  const char *output;
  size_t output_bytes;
} conversions[] = {
  /* Conversions that don't change the sample rate */
  {
    "empty input",
    8, 1, 8000, 0, ENDIAN_LITTLE, "", 0,
    8, 1, 8000, 0, ENDIAN_LITTLE, "", 0
  },
  {
    "sign flip 8-bit unsigned->signed",
    8, 1, 8000, 0, ENDIAN_LITTLE, "\x00\x7F\x80\xFF", 4,
    8, 1, 8000, 1, ENDIAN_LITTLE, "\x80\xFF\x00\x7F", 4
  },
  {
    "sign flip 8-bit signed->unsigned",
    8, 1, 8000, 1, ENDIAN_BIG, "\x80\xFF\x00\x7F", 4,
    8, 1, 8000, 0, ENDIAN_BIG, "\x00\x7F\x80\xFF", 4
  },
  {
    "mono to stereo",
    8, 1, 8000, 0, ENDIAN_LITTLE, "\x00\x7F\x80\xFF", 4,
    8, 2, 8000, 0, ENDIAN_LITTLE, "\x00\x00\x7F\x7F\x80\x80\xFF\xFF", 8
  },
  {
    "stereo to mono",
    8, 2, 8000, 0, ENDIAN_LITTLE, "\x00\x01\x7F\x02\x80\x03\xFF\x04", 8,
    8, 1, 8000, 0, ENDIAN_LITTLE, "\x00\x7F\x80\xFF", 4
  },
  {
    "endian flip little->big",
    16, 1, 8000, 0, ENDIAN_LITTLE, "\x00\x01\x00\xFF\x01\x00\x01\xFF", 8,
    16, 1, 8000, 0, ENDIAN_BIG, "\x01\x00\xFF\x00\x00\x01\xFF\x01", 8,
  },
  {
    "endian flip big->little",
    16, 1, 8000, 0, ENDIAN_BIG, "\x01\x00\xFF\x00\x00\x01\xFF\x01", 8,
    16, 1, 8000, 0, ENDIAN_LITTLE, "\x00\x01\x00\xFF\x01\x00\x01\xFF", 8,
  },
  {
    "8-bit to 16-bit",
    8, 1, 8000, 0, ENDIAN_BIG, "\x00\x7F\x80\xFF", 4,
    16, 1, 8000, 0, ENDIAN_BIG, "\x00\x00\x7F\x00\x80\x00\xFF\x00", 8
  },
  {
    "16-bit to 8-bit",
    16, 1, 8000, 0, ENDIAN_BIG, "\x00\x00\x7F\xFF\x80\x00\xFF\xFF", 8,
    8, 1, 8000, 0, ENDIAN_BIG, "\x00\x7F\x80\xFF", 4
  },
#if HAVE_SAMPLERATE_H
  /* Conversions that do change the sample rate */
  
#endif
};
#define NCONVERSIONS (sizeof conversions / sizeof *conversions)

static void test_resample(void) {
  for(size_t n = 0; n < NCONVERSIONS; ++n) {
    struct resampler rs[1];

    resample_init(rs, 
                  conversions[n].input_bits,
                  conversions[n].input_channels,
                  conversions[n].input_rate,
                  conversions[n].input_signed,
                  conversions[n].input_endian,
                  conversions[n].output_bits,
                  conversions[n].output_channels,
                  conversions[n].output_rate,
                  conversions[n].output_signed,
                  conversions[n].output_endian);
    size_t output_bytes;
    const uint8_t *output = convert(rs,
                                    (const uint8_t *)conversions[n].input, 
                                    conversions[n].input_bytes, 
                                    &output_bytes);
    if(output_bytes != conversions[n].output_bytes
       || memcmp(output, conversions[n].output, output_bytes)) {
      fprintf(stderr, "index %zu description %s mismatch\n",
              n, conversions[n].description);
      size_t k = 0;
      while(k < conversions[n].output_bytes || k < output_bytes) {
        size_t j = 0;
        fprintf(stderr, "%8zu E:", k);
        for(j = 0; j < 16; ++j) {
          if(j + k < conversions[n].output_bytes)
            fprintf(stderr, " %02x", conversions[n].output[j + k]);
        }
        fprintf(stderr, "\n         G:");
        for(j = 0; j < 16; ++j) {
          if(j + k < output_bytes)
            fprintf(stderr, " %02x", output[j + k]);
        }
        fprintf(stderr, "\n");
        k += 16;
      }
      ++errors;
    }
    ++tests;
  }
}

TEST(resample);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
