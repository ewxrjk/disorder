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
/** @file lib/eventlog.h
 * @brief Event logging
 */
#ifndef EVENTLOG_H
#define EVENTLOG_H

/** @brief An output for the event log
 *
 * The caller must allocate these (since log.c isn't allowed to perform memory
 * allocation).  They form a linked list, using eventlog_add() and
 * eventlog_remove().
 */
struct eventlog_output {
  /** @brief Next output */
  struct eventlog_output *next;

  /** @brief Handler for this output */
  void (*fn)(const char *msg, void *user);

  /** @brief Passed to @ref fn */
  void *user;
};

/** @brief Add an event log output
 * @param lo Pointer to output to add
 */
void eventlog_add(struct eventlog_output *lo);

/** @brief Remove an event log output
 * @param lo Pointer to output to remove
 */
void eventlog_remove(struct eventlog_output *lo);

/** @brief Send a message to the event log
 * @param keyword Distinguishing keyword for event
 * @param ... Extra data, terminated by (char *)0
 */
void eventlog(const char *keyword, ...);

/** @brief Send a message to the event log
 * @param keyword Distinguishing keyword for event
 * @param raw Unformatted data
 * @param ... Extra data, terminated by (char *)0
 */
void eventlog_raw(const char *keyword, const char *raw, ...);

#endif /* EVENTLOG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
