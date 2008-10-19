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

#ifndef EVENTLOG_H
#define EVENTLOG_H

/* definition of an event log output.  The caller must allocate these
 * (since log.c isn't allowed to perform memory allocation). */
struct eventlog_output {
  struct eventlog_output *next;
  void (*fn)(const char *msg, void *user);
  void *user;
};

void eventlog_add(struct eventlog_output *lo);
/* add a log output */

void eventlog_remove(struct eventlog_output *lo);
/* remove a log output */

void eventlog(const char *keyword, ...);
void eventlog_raw(const char *keyword, const char *raw, ...);
/* send a message to the event log */

#endif /* EVENTLOG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
