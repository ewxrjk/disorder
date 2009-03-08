/*
 * This file is part of DisOrder.
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
/** @file lib/basen.c @brief Arbitrary base conversion 
 *
 * The functions in this file handle arbitrary-size non-negative integers,
 * represented as a bigendian (MSW first) sequence of @c unsigned @c long
 * words.  The words themselves use the native byte order.
 */

#include "common.h"

#include "basen.h"

/** @brief Test whether v is 0
 * @param v Pointer to bigendian bignum
 * @param nwords Length of bignum
 * @return !v
 */
static int zero(const uint32_t *v, int nwords) {
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
static unsigned divide(uint32_t *v, int nwords, unsigned long m) {
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

/** @brief Multiple v by m and add a
 * @param v Pointer to bigendian bignum
 * @param nwords Length of bignum
 * @param m Value to multiply by
 * @param a Value to add
 * @return 0 on success, non-0 on overflow
 *
 * Does v = m * v + a.
 */
static int mla(uint32_t *v, int nwords, uint32_t m, uint32_t a) {
  int n = nwords - 1;
  uint32_t carry = a;

  while(n >= 0) {
    const uint64_t p = (uint64_t)v[n] * m + carry;
    carry = (uint32_t)(p >> 32);
    v[n] = (uint32_t)p;
    --n;
  }
  /* If there is still a carry then we overflowed */
  return !!carry;
}

static const char basen_chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/** @brief Convert v to a chosen base
 * @param v Pointer to bigendian bignum (modified!)
 * @param nwords Length of bignum
 * @param buffer Output buffer
 * @param bufsize Size of output buffer
 * @param base Number base (2..62)
 * @return 0 on success, -1 if the buffer is too small
 *
 * Converts @p v to a string in the given base using decimal digits, lower case
 * letters and upper case letters as digits.
 *
 * The inverse of nesab().
 */
int basen(uint32_t *v,
	  int nwords,
	  char buffer[],
	  size_t bufsize,
	  unsigned base) {
  size_t i = bufsize;
  
  do {
    if(i <= 1)
      return -1;			/* overflow */
    buffer[--i] = basen_chars[divide(v, nwords, base)];
  } while(!zero(v, nwords));
  memmove(buffer, buffer + i, bufsize - i);
  buffer[bufsize - i] = 0;
  return 0;
}

/** @brief Convert a string back to a large integer in an arbitrary base
 * @param v Where to store result as a bigendian bignum
 * @param nwords Space available in @p v
 * @param s Input string
 * @param base Number base (2..62)
 * @return 0 on success, non-0 on overflow or invalid input
 *
 * The inverse of basen().  If the number is much smaller than the buffer then
 * the first words will be 0.
 */
int nesab(uint32_t *v,
          int nwords,
          const char *s,
          unsigned base) {
  /* Initialize to 0 */
  memset(v, 0, nwords * sizeof *v);
  while(*s) {
    const char *dp = strchr(basen_chars, *s++);
    if(!dp)
      return -1;
    if(mla(v, nwords, base, dp - basen_chars))
      return -1;
  }
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
