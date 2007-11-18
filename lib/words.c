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
#include "unicode.h"

const char *casefold(const char *ptr) {
  return utf8_casefold_canon(ptr, strlen(ptr), 0);
}

static enum unicode_General_Category cat(uint32_t c) {
  if(c < UNICODE_NCHARS) {
    const struct unidata *const ud = &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];
    return ud->general_category;
  } else
    return unicode_General_Category_Cn;
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
    case unicode_General_Category_Ll:
    case unicode_General_Category_Lm:
    case unicode_General_Category_Lo:
    case unicode_General_Category_Lt:
    case unicode_General_Category_Lu:
    case unicode_General_Category_Nd:
    case unicode_General_Category_Nl:
    case unicode_General_Category_No:
    case unicode_General_Category_Sc:
    case unicode_General_Category_Sk:
    case unicode_General_Category_Sm:
    case unicode_General_Category_So:
      /* letters, digits and symbols are considered to be part of
       * words */
      if(!in_word) {
	dynstr_init(&d);
	in_word = 1;
      }
      dynstr_append_bytes(&d, start, s - start);
      break;

    case unicode_General_Category_Cc:
    case unicode_General_Category_Cf:
    case unicode_General_Category_Co:
    case unicode_General_Category_Cs:
    case unicode_General_Category_Zl:
    case unicode_General_Category_Zp:
    case unicode_General_Category_Zs:
    case unicode_General_Category_Pe:
    case unicode_General_Category_Ps:
    separator:
      if(in_word) {
	dynstr_terminate(&d);
	vector_append(&v, d.vec);
	in_word = 0;
      }
      break;

    case unicode_General_Category_Mc:
    case unicode_General_Category_Me:
    case unicode_General_Category_Mn:
    case unicode_General_Category_Pc:
    case unicode_General_Category_Pd:
    case unicode_General_Category_Pf:
    case unicode_General_Category_Pi:
    case unicode_General_Category_Po:
    case unicode_General_Category_Cn:
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
