/*
 * This file is part of DisOrder
 * Copyright (C) 2004 Richard Kettlewell
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

#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>

/* documentation does not match implementation! */
#ifndef SOUND_MIXER_READ
# define SOUND_MIXER_READ(x) MIXER_READ(x)
#endif
#ifndef SOUND_MIXER_WRITE
# define SOUND_MIXER_WRITE(x) MIXER_WRITE(x)
#endif

static const char *channels[] = SOUND_DEVICE_NAMES;

int mixer_channel(const char *c) {
  unsigned n;
  
  if(!c[strspn(c, "0123456789")])
    return atoi(c);
  else {
    for(n = 0; n < sizeof channels / sizeof *channels; ++n)
      if(!strcmp(channels[n], c))
	return n;
    return -1;
  }
}

int mixer_control(int *left, int *right, int set) {
  int fd, ch, r;
    
  if(config->mixer
     && config->channel
     && (ch = mixer_channel(config->channel)) != -1) {
    if((fd = open(config->mixer, O_RDWR, 0)) < 0) {
      error(errno, "error opening %s", config->mixer);
      return -1;
    }
    if(set) {
      r = (*left & 0xff) + (*right & 0xff) * 256;
      if(ioctl(fd, SOUND_MIXER_WRITE(ch), &r) == -1) {
	error(errno, "error changing %s channel %s",
	      config->mixer, config->channel);
	xclose(fd);
	return -1;
      }
    }
    if(ioctl(fd, SOUND_MIXER_READ(ch), &r) == -1) {
      error(errno, "error reading %s channel %s",
	    config->mixer, config->channel);
      xclose(fd);
      return -1;
    }
    *left = r & 0xff;
    *right = (r >> 8) & 0xff;
    xclose(fd);
    return 0;
  } else
    return -1;
}
#else
int mixer_channel(const char attribute((unused)) *c) {
  return 0;
}

int mixer_control(int attribute((unused)) *left, 
		  int attribute((unused)) *right,
		  int attribute((unused)) set) {
  static int reported;

  if(!reported) {
    error(0, "don't know how to set volume on this platform");
    reported = 1;
  }
  return -1;
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
