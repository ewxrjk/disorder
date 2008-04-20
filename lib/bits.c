/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file lib/bits.c
 * @brief Bit operations
 */

#include <config.h>
#include "types.h"

#include <math.h>

#include "bits.h"

#if !HAVE_FLS
/** @brief Compute index of leftmost 1 bit
 * @param n Integer
 * @return Index of leftmost 1 bit or -1
 *
 * For positive @p n we return the index of the leftmost bit of @p n.  For
 * instance @c leftmost_bit(1) returns 0, @c leftmost_bit(15) returns 3, etc.
 *
 * If @p n is zero then -1 is returned.
 */
int leftmost_bit(uint32_t n) {
  /* See e.g. Hacker's Delight s5-3 (p81) for where the idea comes from.
   * Warren is computing the number of leading zeroes, but that's not quite
   * what I wanted.  Also this version should be more portable than his, which
   * inspects the bytes of the floating point number directly.
   */
  int x;
  frexp((double)n, &x);
  /* This gives: n = m * 2^x, where 0.5 <= m < 1 and x is an integer.
   *
   * If we take log2 of either side then we have:
   *    log2(n) = x + log2 m
   *
   * We know that 0.5 <= m < 1 => -1 <= log2 m < 0.  So we floor either side:
   *
   *    floor(log2(n)) = x - 1
   *
   * What is floor(log2(n))?  Well, consider that:
   *
   *    2^k <= z < 2^(k+1)  =>  floor(log2(z)) = k.
   *
   * But 2^k <= z < 2^(k+1) is the same as saying that the leftmost bit of z is
   * bit k.
   *
   *
   * Warren adds 0.5 first, to deal with the case when n=0.  However frexp()
   * guarantees to return x=0 when n=0, so we get the right answer without that
   * step.
   */
  return x - 1;
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
