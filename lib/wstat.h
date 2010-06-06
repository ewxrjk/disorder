/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007-2009 Richard Kettlewell
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
/** @file lib/wstat.h
 * @brief Convert wait status to text
 */

#ifndef WSTAT_H
#define WSTAT_H

#include <sys/wait.h>

char *wstat(int w);
/* Format wait status @w@.  In extremis the return value might be a
 * pointer to a string literal.  The result should always be ASCII. */

#endif /* WSTAT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
