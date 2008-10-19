/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

#ifndef EVENTDIST_H
#define EVENTDIST_H

/** @brief Signature for event handlers
 * @param event Event type
 * @param eventdata Event-specific data
 * @param callbackdata Handler-specific data (as passed to event_register())
 */
typedef void event_handler(const char *event,
                           void *eventdata,
                           void *callbackdata);

/** @brief Handle identifying an event monitor */
typedef struct event_data *event_handle;

event_handle event_register(const char *event,
                            event_handler *callback,
                            void *callbackdata);
void event_cancel(event_handle handle);
void event_raise(const char *event,
                 void *eventdata);

#endif /* EVENTDIST_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
