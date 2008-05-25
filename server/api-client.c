/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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

#include "common.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <locale.h>

#include "client.h"
#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "disorder.h"
#include "api-client.h"

static disorder_client *c;

disorder_client *disorder_get_client(void) {
  if(!c)
    if(!(c = disorder_new(0))) exit(EXIT_FAILURE);
  return c;
}

int disorder_track_exists(const char *track) {
  int result;

  return disorder_exists(c, track, &result) ? 0 : result;
}

const char *disorder_track_get_data(const char *track, const char *key) {
  char *value;

  if(disorder_get(c, track, key, &value)) return 0;
  return value;
}

int disorder_track_set_data(const char *track,
			    const char *key,
			    const char *value) {
  if(value)
    return disorder_set(c, track, key, value);
  else
    return disorder_unset(c, track, key);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
