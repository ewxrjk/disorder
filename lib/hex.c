/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#include <stdio.h>
#include <string.h>

#include "hex.h"
#include "mem.h"
#include "log.h"

char *hex(const uint8_t *ptr, size_t n) {
  char *buf = xmalloc_noptr(n * 2 + 1), *p = buf;

  while(n-- > 0)
    p += sprintf(p, "%02x", (unsigned)*ptr++);
  return buf;
}

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

int unhexdigit(int c) {
  int d;

  if((d = unhexdigitq(c)) < 0) error(0, "invalid hex digit");
  return d;
}

uint8_t *unhex(const char *s, size_t *np) {
  size_t l;
  uint8_t *buf, *p;
  int d1, d2;

  if((l = strlen(s)) & 1) {
    error(0, "hex string has odd length");
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
/* arch-tag:3d2adfde608c6d54dc2cf2b42d78e462 */
