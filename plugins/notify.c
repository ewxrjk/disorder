/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <disorder.h>

static void record(const char *track, const char *what) {
  const char *count;
  int ncount;
  char buf[64];

  if((count = disorder_track_get_data(track, what)))
    ncount = atoi(count);
  else
    ncount = 0;
  disorder_snprintf(buf, sizeof buf, "%d", ncount + 1);
  disorder_track_set_data(track, what, buf);
}

void disorder_notify_play(const char *track,
			  const char *submitter) {
  char buf[64];
  
  if(submitter)
    record(track, "requested");
  record(track, "played");
  disorder_snprintf(buf, sizeof buf, "%"PRIdMAX, (intmax_t)time(0));
  disorder_track_set_data(track, "played_time", buf);
}

void disorder_notify_queue(const char attribute((unused)) *track,
			   const char attribute((unused)) *submitter) {
}

void disorder_notify_scratch(const char *track,
			     const char attribute((unused)) *submitter,
			     const char attribute((unused)) *scratcher,
			     int attribute((unused)) seconds) {
  record(track, "scratched");
}

void disorder_notify_not_scratched(const char *track,
				   const char attribute((unused)) *submitter) {
  record(track, "unscratched");
}

void disorder_notify_queue_remove(const char attribute((unused)) *track,
				  const char attribute((unused)) *remover) {
}

void disorder_notify_queue_move(const char attribute((unused)) *track,
				const char attribute((unused)) *mover) {
}

void disorder_notify_pause(const char attribute((unused)) *track,
			   const char attribute((unused)) *who) {
}

void disorder_notify_resume(const char attribute((unused)) *track,
			    const char attribute((unused)) *who) {
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:09c6471644c1b075a86e1d79093b5542 */
