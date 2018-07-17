/* salsa208.h --- The Salsa20/8 stream cipher
 * Copyright (C) 2018 Mark Wooding
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
/** @file lib/salsa208.h
 * @brief Salsa20/8 stream cipher implementation
 */

#ifndef SALSA208_H
# define SALSA208_H

# include <stddef.h>
# include <stdint.h>

/** @brief Context structure for Salsa208 stream cipher */
typedef struct {
  uint32_t m[16];			/* the raw state matrix */
  uint8_t buf[64];			/* current output buffer */
  unsigned i;				/* cursor in output buffer */
} salsa208_context;

extern void salsa208_stream(salsa208_context *context,
			    const void *inbuf, void *outbuf, size_t length);
extern void salsa208_setkey(salsa208_context *context,
			    const void *key, size_t keylen);
extern void salsa208_setnonce(salsa208_context *context,
			      const void *nonce, size_t noncelen);

#endif /* SALSA208_H */
