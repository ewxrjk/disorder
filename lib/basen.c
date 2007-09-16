/*
 * This file is part of DisOrder.
 * Copyright (C) 2005 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <string.h>

#include "basen.h"

/** @brief Test whether v is 0
 * @param v Pointer to bigendian bignum
 * @param nwords Length of bignum
 * @return !v
 */
static int zero(const unsigned long *v, int nwords) {
  int n;

  for(n = 0; n < nwords && !v[n]; ++n)
    ;
  return n == nwords;
}

/** @brief Divide v by m returning the remainder.
 * @param v Pointer to bigendian bignum
 * @param nwords Length of bignum
 * @param m Divisor (must not be 0)
 * @return Remainder
 *
 * The quotient is stored in @p v.
 */
static unsigned divide(unsigned long *v, int nwords, unsigned long m) {
  unsigned long r = 0, a, b;
  int n;

  /* we do the divide 16 bits at a time */
  for(n = 0; n < nwords; ++n) {
    a = v[n] >> 16;
    b = v[n] & 0xFFFF;
    a += r << 16;
    r = a % m;
    a /= m;
    b += r << 16;
    r = b % m;
    b /= m;
    v[n] = (a << 16) + b;
  }
  return r;
}

/** @brief Convert v to a chosen base
 * @param v Pointer to bigendian bignum
 * @param nwords Length of bignum
 * @param buffer Output buffer
 * @param bufsize Size of output buffer
 * @param base Number base (2..62)
 * @return 0 on success, -1 if the buffer is too small
 *
 * Converts @p v to a string in the given base using decimal digits, lower case
 * letter sand upper case letters as digits.
 */
int basen(unsigned long *v,
	  int nwords,
	  char buffer[],
	  size_t bufsize,
	  unsigned base) {
  size_t i = bufsize;
  static const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  
  do {
    if(i <= 1) return -1;	/* overflow */
    buffer[--i] = chars[divide(v, nwords, base)];
  } while(!zero(v, nwords));
  memmove(buffer, buffer + i, bufsize - i);
  buffer[bufsize - i] = 0;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
