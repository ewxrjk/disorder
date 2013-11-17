/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2007-9, 2013 Richard Kettlewell
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
/** @file lib/hex.c @brief Hexadecimal encoding and decoding */

#include "common.h"

#include "hex.h"
#include "mem.h"
#include "log.h"
#include "printf.h"

/** @brief Convert a byte sequence to hex
 * @param ptr Pointer to first byte
 * @param n Number of bytes
 * @return Allocated string containing hexdump
 */
char *hex(const uint8_t *ptr, size_t n) {
  char *buf = xmalloc_noptr(n * 2 + 1), *p = buf;

  while(n-- > 0)
    p += byte_snprintf(p, 3, "%02x", (unsigned)*ptr++);
  *p = 0;
  return buf;
}

/** @brief Convert a character to its value as a hex digit
 * @param c Character code
 * @return Value has hex digit or -1
 *
 * The 'q' stands for 'quiet' - this function does not report errors.
 */
int unhexdigitq(int c) {
  switch(c) {
  case '0': return 0;
  case '1': return 1;
  case '2': return 2;
  case '3': return 3;
  case '4': return 4;
  case '5': return 5;
  case '6': return 6;
  case '7': return 7;
  case '8': return 8;
  case '9': return 9;
  case 'a': case 'A': return 10;
  case 'b': case 'B': return 11;
  case 'c': case 'C': return 12;
  case 'd': case 'D': return 13;
  case 'e': case 'E': return 14;
  case 'f': case 'F': return 15;
  default: return -1;
  }
}

/** @brief Convert a character to its value as a hex digit
 * @param c Character code
 * @return Value has hex digit or -1
 *
 * If the character is not a valid hex digit then an error is logged.
 * See @ref unhexdigitq() if that is a problem.
 */
int unhexdigit(int c) {
  int d;

  if((d = unhexdigitq(c)) < 0)
    disorder_error(0, "invalid hex digit");
  return d;
}

/** @brief Convert a hex string to bytes
 * @param s Pointer to hex string
 * @param np Where to store byte string length or NULL
 * @return Allocated buffer, or 0
 *
 * @p s should point to a 0-terminated string containing an even number
 * of hex digits.  They are converted to bytes and returned via the return
 * value and optionally the length via @p np.
 *
 * On any error a message is logged and a null pointer is returned.
 */
uint8_t *unhex(const char *s, size_t *np) {
  size_t l;
  uint8_t *buf, *p;
  int d1, d2;

  if((l = strlen(s)) & 1) {
    disorder_error(0, "hex string has odd length");
    return 0;
  }
  p = buf = xmalloc_noptr(l / 2);
  while(*s) {
    if((d1 = unhexdigit(*s++)) < 0) return 0;
    if((d2 = unhexdigit(*s++)) < 0) return 0;
    *p++ = d1 * 16 + d2;
  }
  if(np)
    *np = l / 2;
  return buf;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
