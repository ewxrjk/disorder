/* salsa208.c --- The Salsa20/8 stream cipher
 * Copyright (C) Mark Wooding
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
/** @file lib/salsa208.c
 * @brief Salsa20/8 stream cipher implementation
 *
 * For a description of the algorithm, see:
 *
 *   Daniel J. Bernstein, `The Salsa20 family of stream ciphers', in Matthew
 *   Robshaw and Olivier Billet (eds.), `New Stream Cipher Designs',
 *   Springer--Verlag 2008, pp. 84--97;
 *   http://cr.yp.to/snuffle/salsafamily-20071225.pdf
 *
 * As far as I know, the best attack against all 8 rounds of Salsa20/8 is by
 * Aumasson, Fischer, Khazaei, Meier, and Rechberger, which takes 2^251
 * operations to recover a 256-bit key, which is hopelessly impractical.
 * Much more effective attacks are known against Salsa20/7, so we would have
 * a tiny security margin if we were trying for security -- but we aren't.
 * Instead, we want high-quality randomness for queue ids and for selecting
 * random tracks.  (The cookie machinery, which does want cryptographic
 * security, makes its own arrangements.)  Specifically, the intention is to
 * replace RC4, which (a) is slow because it has a long dependency chain
 * which plays badly with the deep pipelines in modern CPUs, and (b) has
 * well-known and rather embarassing biases.  On the other hand, Salsa20/8
 * has no known biases, and admits considerable instruction-level
 * parallelism.  In practice, Salsa20/8 is about 30% faster than RC4 even
 * without a fancy SIMD implementation (which is good, because this isn't one
 * of those); a vectorized implementation acting on multiple blocks at a time
 * would be even faster.
 *
 * Salsa20/8 has a number of other attractive features, such as being
 * trivially seekable, but we don't need those here and the necessary
 * machinery is not implemented.
 */

#include <assert.h>
#include <string.h>

#include "salsa208.h"

static inline uint32_t ld16(const void *p) {
  const unsigned char *q = p;
  return ((uint32_t)q[0] <<  0) | ((uint32_t)q[1] <<  8);
}

static inline uint32_t ld32(const void *p) {
  const unsigned char *q = p;
  return ((uint32_t)q[0] <<  0) | ((uint32_t)q[1] <<  8) |
	 ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24);
}

static inline void st32(void *p, uint32_t x) {
  unsigned char *q = p;
  q[0] = (x >>  0)&0xff; q[1] = (x >>  8)&0xff;
  q[2] = (x >> 16)&0xff; q[3] = (x >> 24)&0xff;
}

static inline uint32_t rol32(uint32_t x, unsigned n)
  { return (x << n) | (x >> (32 - n)); }

static inline void quarterround(uint32_t m[16], int a, int b, int c, int d) {
  m[b] ^= rol32(m[a] + m[d],  7); m[c] ^= rol32(m[b] + m[a],  9);
  m[d] ^= rol32(m[c] + m[b], 13); m[a] ^= rol32(m[d] + m[c], 18);
}

static void core(salsa208_context *context) {
  unsigned i;
  uint32_t t[16];

  /* Copy the state. */
  for(i = 0; i < 16; i++) t[i] = context->m[i];

  /* Hack on the state. */
  for(i = 0; i < 4; i++) {

    /* Vertical quarter-rounds. */
    quarterround(t,  0,  4,  8, 12);
    quarterround(t,  5,  9, 13,  1);
    quarterround(t, 10, 14,  2,  6);
    quarterround(t, 15,  3,  7, 11);

    /* Horizontal quarter-rounds. */
    quarterround(t,  0,  1,  2,  3);
    quarterround(t,  5,  6,  7,  4);
    quarterround(t, 10, 11,  8,  9);
    quarterround(t, 15, 12, 13, 14);
  }

  /* Final feedforward. */
  for(i = 0; i < 16; i++) t[i] += context->m[i];

  /* Output. */
  for(i = 0; i < 16; i++) st32(context->buf + 4*i, t[i]);
}

static inline void xorbuf(void *z, const void *x, const void *y, size_t sz) {
  unsigned char *zz = z;
  const unsigned char *xx = x, *yy = y;

  if(!xx) memcpy(zz, yy, sz);
  else while(sz--) *zz++ = *xx++ ^ *yy++;
}

static inline void step(salsa208_context *context)
  { if(!++context->m[8]) context->m[9]++; }

/** @brief Encrypt or decrypt data using Salsa20/8.
 *
 * @param context The Salsa20/8 context, initialized using salsa208_setkey().
 * @param inbuf Pointer to input buffer
 * @param outbuf Pointer to output buffer
 * @param length Common size of both buffers
 *
 * Encrypt or decrypt (the operations are the same) @p length bytes of input
 * data, writing the result, of the same length, to @p outbuf.  The input and
 * output pointers may be equal; the two buffers may not otherwise overlap.
 *
 * If @p inbuf is null, then simply write the next @p length bytes of
 * Salsa20/8 output to @p outbuf.
 */
void salsa208_stream(salsa208_context *context,
		     const void *inbuf, void *outbuf, size_t length) {
  size_t left = 64 - context->i;
  unsigned char *z = outbuf;
  const unsigned char *x = inbuf;

  /* If we can satisfy the request from our buffer then we should do that. */
  if(length <= left) {
    xorbuf(z, x, context->buf + context->i, length);
    context->i += length;
    return;
  }

  /* Drain the buffer of what we currently have and cycle the state. */
  xorbuf(z, x, context->buf + context->i, left);
  length -= left; z += left; if(x) x += left;
  core(context); step(context);

  /* Take multiple complete blocks directly. */
  while(length > 64) {
    xorbuf(z, x, context->buf, 64);
    length -= 64; z += 64; if(x) x += 64;
    core(context); step(context);
  }

  /* And the final tail end. */
  xorbuf(z, x, context->buf, length);
  context->i = length;
}

/** @brief Initialize a Salsa20/8 context.
 *
 * @param context The Salsa20/8 context to initialize
 * @param key A pointer to the key material
 * @param keylen The length of the key data, in bytes (must be 10, 16, or 32)
 *
 * The context is implicitly initialized with a zero nonce, which is fine if
 * the key will be used only for a single message.  Otherwise, a fresh nonce
 * should be chosen somehow and set using salsa208_setnonce().
 */
void salsa208_setkey(salsa208_context *context,
		     const void *key, size_t keylen) {
  const unsigned char *k = key;
  switch(keylen) {
  case 32:
    context->m[ 0] = 0x61707865;
    context->m[ 1] = ld32(k +  0);
    context->m[ 2] = ld32(k +  4);
    context->m[ 3] = ld32(k +  8);
    context->m[ 4] = ld32(k + 12);
    context->m[ 5] = 0x3320646e;
    context->m[ 6] = 0;
    context->m[ 7] = 0;
    context->m[ 8] = 0;
    context->m[ 9] = 0;
    context->m[10] = 0x79622d32;
    context->m[11] = ld32(k + 16);
    context->m[12] = ld32(k + 20);
    context->m[13] = ld32(k + 24);
    context->m[14] = ld32(k + 28);
    context->m[15] = 0x6b206574;
    break;
  case 16:
    context->m[ 0] = 0x61707865;
    context->m[ 1] = ld32(k +  0);
    context->m[ 2] = ld32(k +  4);
    context->m[ 3] = ld32(k +  8);
    context->m[ 4] = ld32(k + 12);
    context->m[ 5] = 0x3120646e;
    context->m[ 6] = 0;
    context->m[ 7] = 0;
    context->m[ 8] = 0;
    context->m[ 9] = 0;
    context->m[10] = 0x79622d36;
    context->m[11] = context->m[1];
    context->m[12] = context->m[2];
    context->m[13] = context->m[3];
    context->m[14] = context->m[4];
    context->m[15] = 0x6b206574;
    break;
  case 10:
    context->m[ 0] = 0x61707865;
    context->m[ 1] = ld32(k +  0);
    context->m[ 2] = ld32(k +  4);
    context->m[ 3] = ld16(k +  8);
    context->m[ 4] = 0;
    context->m[ 5] = 0x3120646e;
    context->m[ 6] = 0;
    context->m[ 7] = 0;
    context->m[ 8] = 0;
    context->m[ 9] = 0;
    context->m[10] = 0x79622d30;
    context->m[11] = context->m[1];
    context->m[12] = context->m[2];
    context->m[13] = context->m[3];
    context->m[14] = 0;
    context->m[15] = 0x6b206574;
    break;
  default:
    assert(!"bad Salsa20 key length");
  }
  context->i = 64;
}

/** @brief Set the Salsa20/8 nonce.
 *
 * @param context The Salsa20/8 context
 * @param nonce A pointer to the nonce
 * @param noncelen The length of the nonce data, in bytes (must be exactly 8)
 *
 * The context is automatically rewound to the start of the stream
 * corresponding to this nonce.
 */
void salsa208_setnonce(salsa208_context *context,
		       const void *nonce, size_t noncelen) {
  const unsigned char *n = nonce;
  assert(noncelen == 8);
  context->m[6] = ld32(n +  0);
  context->m[7] = ld32(n +  4);
  context->m[8] = context->m[9] = 0;
  context->i = 64;
}
