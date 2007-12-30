/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <sys/ioctl.h>

#include "configuration.h"
#include "mixer.h"
#include "log.h"
#include "syscalls.h"

static const struct mixer *mixers[] = {
#if HAVE_SYS_SOUNDCARD_H
  &mixer_oss
#endif
};

#define NMIXERS (sizeof mixers / sizeof *mixers)

static const struct mixer *find_mixer(int api) {
  size_t n;

  for(n = 0; n < NMIXERS; ++n)
    if(mixers[n]->api == api)
      return mixers[n];
  return 0;
}

int mixer_valid_channel(int api, const char *c) {
  const struct mixer *const m = find_mixer(api);

  return m ? m->validate_channel(c) : 1;
}

int mixer_valid_device(int api, const char *d) {
  const struct mixer *const m = find_mixer(api);

  return m ? m->validate_device(d) : 1;
}

int mixer_control(int *left, int *right, int set) {
  const struct mixer *const m = find_mixer(config->api);

  if(m) {
    if(set)
      return m->set(left, right);
    else
      return m->get(left, right);
  } else {
    static int reported;

    if(!reported) {
      error(0, "don't know how to get/set volume with this api");
      reported = 1;
    }
    return -1;
  }
}

const char *mixer_default_device(int api) {
  const struct mixer *const m = find_mixer(api);

  return m ? m->device : "";
}

const char *mixer_default_channel(int api) {
  const struct mixer *const m = find_mixer(api);

  return m ? m->channel : "";
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
