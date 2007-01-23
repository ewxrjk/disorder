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

#include <unistd.h>
#include <errno.h>

#include <disorder.h>

#ifndef TYPE
# define TYPE DISORDER_PLAYER_STANDALONE
#endif
const unsigned long disorder_player_type = TYPE;

void disorder_play_track(const char *const *parameters,
			 int nparameters,
			 const char *path,
			 const char attribute((unused)) *track,
			 void attribute((unused)) *data) {
  int i, j;
  const char **vec;

  vec = disorder_malloc((nparameters + 2) * sizeof (char *));
  i = 0;
  j = 0;
  for(i = 0; i < nparameters; ++i)
    vec[j++] = parameters[i];
  vec[j++] = path;
  vec[j] = 0;
  execvp(vec[0], (char **)vec);
  disorder_fatal(errno, "error executing %s", vec[0]);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
