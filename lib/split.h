/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
#ifndef SPLIT_H
#define SPLIT_H

#define SPLIT_COMMENTS	0001		/* # starts a comment */
#define SPLIT_QUOTES	0002		/* " and ' quote strings */

char **split(const char *s,
	     int *np,
	     unsigned flags,
	     void (*error_handler)(const char *msg, void *u),
	     void *u);
/* split @s@ up into fields.  Return a null-pointer-terminated array
 * of pointers to the fields.  If @np@ is not a null pointer store the
 * number of fields there.  Calls @error_handler@ to report any
 * errors.
 *
 * split() operates on UTF-8 strings.
 */

const char *quoteutf8(const char *s);
/* quote a UTF-8 string.  Might return @s@ if no quoting is required.  */

#endif /* SPLIT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
