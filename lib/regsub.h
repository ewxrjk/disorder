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
/** @file lib/regsub.h
 * @brief Regexp substitution
 */
#ifndef REGSUB_H
#define REGSUB_H

#include "regexp.h"

#define REGSUB_GLOBAL 		0x0001	/* global replace */
#define REGSUB_MUST_MATCH	0x0002	/* return 0 if no match */
#define REGSUB_CASE_INDEPENDENT	0x0004	/* case independent */
#define REGSUB_REPLACE          0x0008  /* replace whole, not match */

unsigned regsub_flags(const char *flags);
/* parse a flag string */

int regsub_compile_options(unsigned flags);
/* convert compile-time options */

const char *regsub(const regexp *re, const char *subject,
		   const char *replace, unsigned flags);

#endif /* REGSUB_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
