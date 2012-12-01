/*
 * This file is part of DisOrder.
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
#include "test.h"
#include "eventdist.h"

static int wibbles, wobbles, wobble2s;

static void on_wibble(const char *event, void *eventdata, void *callbackdata) {
  check_string(event, "wibble");
  check_string(eventdata, "wibble_eventdata");
  check_string(callbackdata, "wibble_data");
  ++wibbles;
}

static void on_wobble(const char *event, void *eventdata, void *callbackdata) {
  check_string(event, "wobble");
  check_string(eventdata, "wobble_eventdata");
  check_string(callbackdata, "wobble_data");
  ++wobbles;
}

static void on_wobble2(const char *event, void *eventdata, void *callbackdata) {
  check_string(event, "wobble");
  check_string(eventdata, "wobble_eventdata");
  check_string(callbackdata, "wobble2_data");
  ++wobble2s;
}

static void test_eventdist(void) {
  event_handle wibble_handle, wobble_handle, wobble2_handle;
  /* Raising unregistered events should be safe */
  event_raise("wibble", NULL);
  ++tests;
  
  wibble_handle = event_register("wibble", on_wibble, (void *)"wibble_data");
  wobble_handle = event_register("wobble", on_wobble, (void *)"wobble_data");
  wobble2_handle = event_register("wobble", on_wobble2, (void *)"wobble2_data");
  event_raise("wibble", (void *) "wibble_eventdata");
  check_integer(wibbles, 1);
  check_integer(wobbles, 0);
  check_integer(wobble2s, 0);

  event_raise("wobble", (void *)"wobble_eventdata");
  check_integer(wibbles, 1);
  check_integer(wobbles, 1);
  check_integer(wobble2s, 1);

  event_raise("wobble", (void *)"wobble_eventdata");
  check_integer(wibbles, 1);
  check_integer(wobbles, 2);
  check_integer(wobble2s, 2);

  event_cancel(wobble_handle);
  
  event_raise("wibble", (void *)"wibble_eventdata");
  check_integer(wibbles, 2);
  check_integer(wobbles, 2);
  check_integer(wobble2s, 2);
  
  event_raise("wobble", (void *)"wobble_eventdata");
  check_integer(wibbles, 2);
  check_integer(wobbles, 2);
  check_integer(wobble2s, 3);

  event_cancel(wibble_handle);
  event_cancel(wobble2_handle);
}

TEST(eventdist);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
