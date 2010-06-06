/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/base64.c
 * @brief Support for MIME base64
 */

#include "common.h"

#include "mem.h"
#include "base64.h"
#include "vector.h"

static const char mime_base64_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/** @brief Convert MIME base64
 * @param s base64 data
 * @param nsp Where to store length of converted data
 * @return Decoded data
 *
 * See <a href="http://tools.ietf.org/html/rfc2045#section-6.8">RFC
 * 2045 s6.8</a>.
 */
char *mime_base64(const char *s, size_t *nsp) {
  return generic_base64(s, nsp, mime_base64_table);
}

/** @brief Convert base64
 * @param s base64 data
 * @param nsp Where to store length of converted data
 * @param table Table of characters to use
 * @return Decoded data
 *
 * @p table should consist of 65 characters.  The first 64 will be used to
 * represents the 64 digits and the 65th will be used as padding at the end
 * (i.e. the role of '=' in RFC2045 base64).
 */
char *generic_base64(const char *s, size_t *nsp, const char *table) {
  struct dynstr d;
  const char *t;
  int b[4], n, c;

  dynstr_init(&d);
  n = 0;
  while((c = (unsigned char)*s++)) {
    if(c == table[64]) {
      if(n >= 2) {
	dynstr_append(&d, (b[0] << 2) + (b[1] >> 4));
	if(n == 3)
	  dynstr_append(&d, (b[1] << 4) + (b[2] >> 2));
      }
      break;
    } else if((t = strchr(table, c))) {
      b[n++] = t - table;
      if(n == 4) {
	dynstr_append(&d, (b[0] << 2) + (b[1] >> 4));
	dynstr_append(&d, (b[1] << 4) + (b[2] >> 2));
	dynstr_append(&d, (b[2] << 6) + b[3]);
	n = 0;
      }
    }
  }
  if(nsp)
    *nsp = d.nvec;
  dynstr_terminate(&d);
  return d.vec;
}

/** @brief Convert a binary string to MIME base64
 * @param s Bytes to convert
 * @param ns Number of bytes to convert
 * @return Encoded data
 *
 * This function does not attempt to split up lines.
 *
 * See <a href="http://tools.ietf.org/html/rfc2045#section-6.8">RFC
 * 2045 s6.8</a>.
 */
char *mime_to_base64(const uint8_t *s, size_t ns) {
  return generic_to_base64(s, ns, mime_base64_table);
}

/** @brief Convert a binary string to base64
 * @param s Bytes to convert
 * @param ns Number of bytes to convert
 * @param table Table of characters to use
 * @return Encoded data
 *
 * This function does not attempt to split up lines.
 *
 * @p table should consist of 65 characters.  The first 64 will be used to
 * represents the 64 digits and the 65th will be used as padding at the end
 * (i.e. the role of '=' in RFC2045 base64).
 */
char *generic_to_base64(const uint8_t *s, size_t ns, const char *table) {
  struct dynstr d[1];

  dynstr_init(d);
  while(ns >= 3) {
    /* Input bytes with output bits: AAAAAABB BBBBCCCC CCDDDDDD */
    /* Output bytes with input bits: 000000 001111 111122 222222 */
    dynstr_append(d, table[s[0] >> 2]);
    dynstr_append(d, table[((s[0] & 3) << 4)
				       + (s[1] >> 4)]);
    dynstr_append(d, table[((s[1] & 15) << 2)
				       + (s[2] >> 6)]);
    dynstr_append(d, table[s[2] & 63]);
    ns -= 3;
    s += 3;
  }
  if(ns > 0) {
    dynstr_append(d, table[s[0] >> 2]);
    switch(ns) {
    case 1:
      dynstr_append(d, table[(s[0] & 3) << 4]);
      dynstr_append(d, table[64]);
      dynstr_append(d, table[64]);
      break;
    case 2:
      dynstr_append(d, table[((s[0] & 3) << 4)
					 + (s[1] >> 4)]);
      dynstr_append(d, table[(s[1] & 15) << 2]);
      dynstr_append(d, table[64]);
      break;
    }
  }
  dynstr_terminate(d);
  return d->vec;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
