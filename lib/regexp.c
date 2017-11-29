/*
 * This file is part of DisOrder
 * Copyright (C) 2017 Mark Wooding
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
/** @file lib/regexp.c
 * @brief Regular expressions
 */
#include "common.h"

#include "regexp.h"
#include "mem.h"

void regexp_setup(void)
{
  pcre_malloc = xmalloc;
  pcre_free = xfree;
}

regexp *regexp_compile(const char *pat, unsigned f,
		       char *errbuf, size_t errlen, size_t *erroff_out)
{
  char *p;
  const char *e;
  int erroff;
  regexp *re;
  size_t i;

  re = pcre_compile(pat, f, &e, &erroff, 0);
  if(!re) {
    *erroff_out = erroff;
    for(p = errbuf, i = errlen - 1; i && *e; i--) *p++ = *e++;
    *p = 0;
  }
  return re;
}

int regexp_match(const regexp *re, const char *s, size_t n, unsigned f,
		 size_t *ov, size_t on)
{
  int rc;
  int *myov;
  size_t i;

  myov = xmalloc(on*sizeof(*myov));
  rc = pcre_exec(re, 0, s, n, 0, f, myov, on);
  for(i = 0; i < on; i++) ov[i] = myov[i];
  xfree(myov);
  return rc;
}

void regexp_free(regexp *re)
  { pcre_free(re); }

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
