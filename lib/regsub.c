/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2007, 20008 Richard Kettlewell
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
/** @file lib/regsub.c
 * @brief Regexp substitution
 */
#include "common.h"

#include <pcre.h>

#include "regsub.h"
#include "mem.h"
#include "vector.h"
#include "log.h"

#define PREMATCH (-1)			/* fictitious pre-match substring */
#define POSTMATCH (-2)			/* fictitious post-match substring */

static inline int substring_start(const char attribute((unused)) *subject,
				  const int *ovector,
				  int n) {
  switch(n) {
  case PREMATCH: return 0;
  case POSTMATCH: return ovector[1];
  default: return ovector[2 * n];
  }
}

static inline int substring_end(const char *subject,
				const int *ovector,
				int n) {
  switch(n) {
  case PREMATCH: return ovector[0];
  case POSTMATCH: return strlen(subject);
  default: return ovector[2 * n + 1];
  }
}

static void transform_append(struct dynstr *d,
			     const char *subject,
			     const int *ovector,
			     int n) {
  int start = substring_start(subject, ovector, n);
  int end = substring_end(subject, ovector, n);

  if(start != -1)
    dynstr_append_bytes(d, subject + start, end - start);
}

static void replace_core(struct dynstr *d,
			 const char *subject,
			 const char *replace,
			 int rc,
			 const int *ovector) {
  int substr;
  
  while(*replace) {
    if(*replace == '$')
      switch(replace[1]) {
      case '&':
	transform_append(d, subject, ovector, 0);
	replace += 2;
	break;
      case '1': case '2': case '3':
      case '4': case '5': case '6':
      case '7': case '8': case '9':
	substr = replace[1] - '0';
	if(substr < rc)
	  transform_append(d, subject, ovector, substr);
	replace += 2;
	break;
      case '$':
	dynstr_append(d, '$');
	replace += 2;
	break;
      default:
	dynstr_append(d, *replace++);
	break;
      }
    else
      dynstr_append(d, *replace++);
  }
}

unsigned regsub_flags(const char *flags) {
  unsigned f = 0;

  while(*flags) {
    switch(*flags++) {
    case 'g': f |= REGSUB_GLOBAL; break;
    case 'i': f |= REGSUB_CASE_INDEPENDENT; break;
    default: break;
    }
  }
  return f;
}

int regsub_compile_options(unsigned flags) {
  int options = 0;

  if(flags & REGSUB_CASE_INDEPENDENT)
    options |= PCRE_CASELESS;
  return options;
}

const char *regsub(const pcre *re, const char *subject, const char *replace,
		   unsigned flags) {
  int rc, ovector[99], matches;
  struct dynstr d;

  dynstr_init(&d);
  matches = 0;
  /* find the next match */
  while((rc = pcre_exec(re, 0, subject, strlen(subject), 0,
		     0, ovector, sizeof ovector / sizeof (int))) > 0) {
    /* text just before the match */
    if(!(flags & REGSUB_REPLACE))
      transform_append(&d, subject, ovector, PREMATCH);
    /* the replacement text */
    replace_core(&d, subject, replace, rc, ovector);
    ++matches;
    if(!*subject)			/* end of subject */
      break;
    if(flags & REGSUB_REPLACE)		/* replace subject entirely */
      break;
    /* step over the matched substring */
    subject += substring_start(subject, ovector, POSTMATCH);
    if(!(flags & REGSUB_GLOBAL))
      break;
  }
  if(rc <= 0 && rc != PCRE_ERROR_NOMATCH) {
    disorder_error(0, "pcre_exec returned %d, subject '%s'", rc, subject);
    return 0;
  }
  if((flags & REGSUB_MUST_MATCH) && matches == 0)
    return 0;
  /* append the remainder of the subject */
  if(!(flags & REGSUB_REPLACE))
    dynstr_append_string(&d, subject);
  dynstr_terminate(&d);
  return d.vec;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
