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
/** @file lib/eventdist.c
 * @brief Event distribution
 */
#include "common.h"

#include "mem.h"
#include "eventdist.h"
#include "hash.h"

struct event_data {
  struct event_data *next;
  const char *event;
  event_handler *callback;
  void *callbackdata;
};

static hash *events;

/** @brief Register an event handler
 * @param event Event type to handle
 * @param callback Function to call when event occurs
 * @param callbackdata Passed to @p callback
 * @return Handle for this registration (for use with event_cancel())
 */
event_handle event_register(const char *event,
                            event_handler *callback,
                            void *callbackdata) {
  static const struct event_data *null;
  struct event_data *ed = xmalloc(sizeof *ed), **head;

  if(!events)
    events = hash_new(sizeof (struct event_data *));
  if(!(head = hash_find(events, event))) {
    hash_add(events, event, &null, HASH_INSERT);
    head = hash_find(events, event);
  }
  ed->next = *head;
  ed->event = xstrdup(event);
  ed->callback = callback;
  ed->callbackdata = callbackdata;
  *head = ed;
  return ed;
}

/** @brief Stop handling an event
 * @param handle Registration to cancel (as returned from event_register())
 *
 * @p handle is allowed to be NULL.
 */
void event_cancel(event_handle handle) {
  struct event_data **head, **edp;
  
  if(!handle)
    return;
  assert(events);
  head = hash_find(events, handle->event);
  for(edp = head; *edp && *edp != handle; edp = &(*edp)->next)
    ;
  assert(*edp == handle);
  *edp = handle->next;
}

/** @brief Raise an event
 * @param event Event type to raise
 * @param eventdata Event-specific data
 */
void event_raise(const char *event,
                 void *eventdata) {
  struct event_data *ed, **head;
  if(!events)
    return;
  if(!(head = hash_find(events, event)))
    return;
  for(ed = *head; ed; ed = ed->next)
    ed->callback(event, eventdata, ed->callbackdata);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
