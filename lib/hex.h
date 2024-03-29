/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/hex.h @brief Hexadecimal encoding and decoding */

#ifndef HEX_H
#define HEX_H

char *hex(const uint8_t *ptr, size_t n);
/* convert an octet-string to hex */

uint8_t *unhex(const char *s, size_t *np);
/* convert a hex string back to an octet string */

int unhexdigit(int c);
/* if @c@ is a hex digit, return its value.  else return -1 and log an error. */

int unhexdigitq(int c);
/* same as unhexdigit() but doesn't issue an error message */

#endif /* HEX_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
