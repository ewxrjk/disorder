/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007 Richard Kettlewell
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
#include <stddef.h>

#include "mem.h"
#include "vector.h"
#include "table.h"
#include "words.h"
#include "utf8.h"
#include "log.h"
#include "charset.h"

#include "unidata.h"

const char *casefold(const char *ptr) {
  struct dynstr d;
  uint32_t c;
  const char *s = ptr;

  dynstr_init(&d);
  while(*s) {
    /* Convert UTF-8 to UCS-32 */
    PARSE_UTF8(s, c, return ptr);
    /* Normalize */
    if(c < UNICODE_NCHARS) {
      /* If this a known character, convert it to lower case */
      const struct unidata *const ud = &unidata[c / 256][c % 256];
      c += ud->lower_offset;
    }
    /* Convert UCS-4 back to UTF-8 */
    one_ucs42utf8(c, &d);
  }
  dynstr_terminate(&d);
  return d.vec;
}

static enum unicode_gc_cat cat(uint32_t c) {
  if(c < UNICODE_NCHARS) {
    /* If this a known character, convert it to lower case */
    const struct unidata *const ud = &unidata[c / 256][c % 256];
    return ud->gc;
  } else
    return unicode_gc_Cn;
}

/* XXX this is a bit kludgy */

char **words(const char *s, int *nvecp) {
  struct vector v;
  struct dynstr d;
  const char *start;
  uint32_t c;
  int in_word = 0;

  vector_init(&v);
  while(*s) {
    start = s;
    PARSE_UTF8(s, c, return 0);
    /* special cases first */
    switch(c) {
    case '/':
    case '.':
    case '+':
    case '&':
    case ':':
    case '_':
    case '-':
      goto separator;
    }
    /* do the rest on category */
    switch(cat(c)) {
    case unicode_gc_Ll:
    case unicode_gc_Lm:
    case unicode_gc_Lo:
    case unicode_gc_Lt:
    case unicode_gc_Lu:
    case unicode_gc_Nd:
    case unicode_gc_Nl:
    case unicode_gc_No:
    case unicode_gc_Sc:
    case unicode_gc_Sk:
    case unicode_gc_Sm:
    case unicode_gc_So:
      /* letters, digits and symbols are considered to be part of
       * words */
      if(!in_word) {
	dynstr_init(&d);
	in_word = 1;
      }
      dynstr_append_bytes(&d, start, s - start);
      break;

    case unicode_gc_Cc:
    case unicode_gc_Cf:
    case unicode_gc_Co:
    case unicode_gc_Cs:
    case unicode_gc_Zl:
    case unicode_gc_Zp:
    case unicode_gc_Zs:
    case unicode_gc_Pe:
    case unicode_gc_Ps:
    separator:
      if(in_word) {
	dynstr_terminate(&d);
	vector_append(&v, d.vec);
	in_word = 0;
      }
      break;

    case unicode_gc_Mc:
    case unicode_gc_Me:
    case unicode_gc_Mn:
    case unicode_gc_Pc:
    case unicode_gc_Pd:
    case unicode_gc_Pf:
    case unicode_gc_Pi:
    case unicode_gc_Po:
    case unicode_gc_Cn:
      /* control and punctuation is completely ignored */
      break;

    }
  }
  if(in_word) {
    /* pick up the final word */
    dynstr_terminate(&d);
    vector_append(&v, d.vec);
  }
  vector_terminate(&v);
  if(nvecp)
    *nvecp = v.nvec;
  return v.vec;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
