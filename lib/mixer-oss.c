/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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
/** @file lib/mixer-oss.c
 * @brief OSS mixer support
 *
 * Mono output devices aren't explicitly supported (but may work
 * nonetheless).
 */

#include "common.h"

#if HAVE_SYS_SOUNDCARD_H

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "configuration.h"
#include "mixer.h"
#include "log.h"
#include "syscalls.h"
#include "mem.h"

/* documentation does not match implementation! */
#ifndef SOUND_MIXER_READ
# define SOUND_MIXER_READ(x) MIXER_READ(x)
#endif
#ifndef SOUND_MIXER_WRITE
# define SOUND_MIXER_WRITE(x) MIXER_WRITE(x)
#endif

/** @brief Channel names */
static const char *channels[] = SOUND_DEVICE_NAMES;

/** @brief Convert channel name to number */
static int mixer_channel(const char *c) {
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

/** @brief Open the OSS mixer device and return its fd */
static int oss_do_open(void) {
  int fd;
  
  if((fd = open(config->mixer, O_RDWR, 0)) < 0) {
    static char *reported;

    if(!reported || strcmp(reported, config->mixer)) {
      if(reported)
	xfree(reported);
      reported = xstrdup(config->mixer);
      error(errno, "error opening %s", config->mixer);
    }
  }
  return fd;
}

/** @brief Get the OSS mixer setting */
static int oss_do_get(int *left, int *right, int fd, int ch) {
  int r;
  
  if(ioctl(fd, SOUND_MIXER_READ(ch), &r) == -1) {
    error(errno, "error reading %s channel %s",
	  config->mixer, config->channel);
    return -1;
  }
  *left = r & 0xff;
  *right = (r >> 8) & 0xff;
  return 0;
}

/** @brief Get OSS volume */
static int oss_get(int *left, int *right) {
  int ch, fd;

  if(config->mixer
     && config->channel
     && (ch = mixer_channel(config->channel)) != -1) {
    if((fd = oss_do_open()) < 0)
      return -1;
    if(oss_do_get(left, right, fd, ch) < 0) {
      xclose(fd);
      return -1;
    }
    xclose(fd);
    return 0;
  } else
    return -1;
}

/** @brief Set OSS volume */
static int oss_set(int *left, int *right) {
  int ch, fd, r;

  if(config->mixer
     && config->channel
     && (ch = mixer_channel(config->channel)) != -1) {
    if((fd = oss_do_open()) < 0)
      return -1;
    r = (*left & 0xff) + (*right & 0xff) * 256;
    if(ioctl(fd, SOUND_MIXER_WRITE(ch), &r) == -1) {
      error(errno, "error changing %s channel %s",
	    config->mixer, config->channel);
      xclose(fd);
      return -1;
    }
    if(oss_do_get(left, right, fd, ch) < 0) {
      xclose(fd);
      return -1;
    }
    xclose(fd);
    return 0;
  } else
    return -1;
}

/** @brief OSS mixer vtable */
const struct mixer mixer_oss = {
  BACKEND_OSS,
  oss_get,
  oss_set,
  "/dev/mixer",
  "pcm"
};
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
