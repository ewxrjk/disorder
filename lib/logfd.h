/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/logfd.h
 * @brief Redirect subprocess stderr to DisOrder server's log
 */

#ifndef LOGFD_H
#define LOGFD_H

struct ev_source;

int logfd(struct ev_source *ev, const char *tag);
/* return an FD to write log messages to */

#endif /* LOGFD_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
