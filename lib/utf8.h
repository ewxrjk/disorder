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
#ifndef UTF8_H
#define UTF8_H

#define PARSE_UTF8(S,C,E) do {			\
  if((unsigned char)*S < 0x80)			\
    C = *S++;					\
  else if((unsigned char)*S <= 0xDF) {		\
    C = (*S++ & 0x1F) << 6;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F);				\
    if(C < 0x80) { E; }				\
  } else if((unsigned char)*S <= 0xEF) {	\
    C = (*S++ & 0x0F) << 12;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F) << 6;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F);				\
    if(C < 0x800				\
       || (C >= 0xD800 && C <= 0xDFFF)) {	\
      E;					\
    }						\
  } else if((unsigned char)*S <= 0xF7) {	\
    C = (*S++ & 0x07) << 18;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F) << 12;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F) << 6;			\
    if((*S & 0xC0) != 0x80) { E; }		\
    C |= (*S++ & 0x3F);				\
    if(C < 0x10000 || C > 0x10FFFF) { E; }	\
  } else {					\
    E;						\
  }						\
} while(0)

int validutf8(const char *s);
/* return nonzero if S is a valid UTF-8 sequence, else false */

#endif /* UTF8_h */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:456aedaec99ad19d321ac15b7765da4d */
