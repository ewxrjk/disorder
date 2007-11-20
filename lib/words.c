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
  return utf8_casefold_compat(ptr, strlen(ptr), 0);
}

char **words(const char *s, int *nvecp) {
  size_t nv;
  char **v;

  v = utf8_word_split(s, strlen(s), &nv);
  *nvecp = nv;
  return v;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
