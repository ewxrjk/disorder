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
/** @file lib/regexp.h
 * @brief Regular expressions
 */
#ifndef REGEXP_H
#define REGEXP_H

#if defined(HAVE_PCRE_H)
# include <pcre.h>
  typedef pcre regexp;
# define RXF_CASELESS PCRE_CASELESS
# define RXERR_NOMATCH PCRE_ERROR_NOMATCH
#else
# error "no supported regular expression library found"
#endif

void regexp_setup(void);

#define RXCERR_LEN 128
regexp *regexp_compile(const char *pat, unsigned f,
		       char *errbuf, size_t errlen, size_t *erroff_out);

int regexp_match(const regexp *re, const char *s, size_t n, unsigned f,
		 size_t *ov, size_t on);

void regexp_free(regexp *re);

#endif /* REGEXP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
