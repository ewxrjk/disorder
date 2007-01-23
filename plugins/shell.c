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
#include <stdlib.h>

#include <disorder.h>

const unsigned long disorder_player_type = DISORDER_PLAYER_STANDALONE;

void disorder_play_track(const char *const *parameters,
			 int nparameters,
			 const char *path,
			 const char *track,
			 void attribute((unused)) *data) {
  const char *vec[4];
  char *env_track, *env_path;

  vec[1] = "-c";
  vec[3] = 0;
  switch(nparameters) {
  case 0:
    disorder_fatal(0, "missing argument to shell player module");
  case 1:
    vec[0] = "sh";
    vec[2] = parameters[0];
    break;
  case 2:
    vec[0] = parameters[0];
    vec[2] = parameters[1];
    break;
  default:
    disorder_fatal(0, "extra arguments to shell player module");
  }
  disorder_asprintf(&env_path, "TRACK=%s", path);
  if(putenv(env_path) < 0) disorder_fatal(errno, "error calling putenv");
  disorder_asprintf(&env_track, "TRACK_UTF8=%s", track);
  if(putenv(env_track) < 0) disorder_fatal(errno, "error calling putenv");
  execvp(vec[0], (char **)vec);
  disorder_fatal(errno, "error executing %s", vec[0]);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:5e4b82a463c04d36c6cb9142db00ed07 */
