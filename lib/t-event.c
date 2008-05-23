/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#include "test.h"
#include "event.h"

#include <time.h>

static ev_source *ev;

static int run1, run2, run3;
static ev_timeout_handle t1, t2, t3;

static int callback1(ev_source attribute((unused)) *ev,
		     const struct timeval attribute((unused)) *now,
		     void attribute((unused)) *u) {
  run1 = 1;
  ev_timeout_cancel(ev, t2);
  return 0;
}

static int callback2(ev_source attribute((unused)) *ev,
		     const struct timeval attribute((unused)) *now,
		     void attribute((unused)) *u) {
  run2 = 1;
  return 0;
}

static int callback3(ev_source attribute((unused)) *ev,
		     const struct timeval attribute((unused)) *now,
		     void attribute((unused)) *u) {
  run3 = 1;
  return 1;
}

static void test_event(void) {
  struct timeval w;

  ev = ev_new();
  w.tv_sec = time(0) + 2;
  w.tv_usec = 0;
  ev_timeout(ev, &t1, &w, callback1, 0);
  w.tv_sec = time(0) + 3;
  w.tv_usec = 0;
  ev_timeout(ev, &t2, &w, callback2, 0);
  w.tv_sec = time(0) + 4;
  w.tv_usec = 0;
  ev_timeout(ev, &t3, &w, callback3, 0);
  check_integer(ev_run(ev), 1);
  check_integer(run1, 1);
  check_integer(run2, 0);
  check_integer(run3, 1);
}

TEST(event);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
