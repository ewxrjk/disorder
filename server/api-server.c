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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pcre.h>

#include "log.h"
#include "mem.h"
#include "disorder.h"
#include "event.h"
#include "trackdb.h"

int disorder_track_exists(const char *track)  {
  return trackdb_exists(track);
}

const char *disorder_track_get_data(const char *track, const char *key)  {
  return trackdb_get(track, key);
}

int disorder_track_set_data(const char *track,
			    const char *key, const char *value)  {
  return trackdb_set(track, key, value);
}

const char *disorder_track_random(void)  {
  return trackdb_random(16);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
