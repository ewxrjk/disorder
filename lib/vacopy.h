/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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

#ifndef VACOPY_H
#define VACOPY_H

#ifndef va_copy
# ifdef __va_copy
#  define va_copy __va_copy
# else
#  define va_copy(d, s) ((void)memcpy(&d, &s, sizeof s))
# endif
#endif

#endif /* VACOPY_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
