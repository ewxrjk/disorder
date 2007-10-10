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
/** @file server/speaker-oss.c
 * @brief Support for @ref BACKEND_OSS */

#include <config.h>

#if HAVE_SYS_SOUNDCARD_H

#include "types.h"

#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "speaker-protocol.h"
#include "speaker.h"

/** @brief Current device */
static int ossfd = -1;

/** @brief OSS backend initialization */
static void oss_init(void) {
  info("selected OSS backend");
}

/** @brief OSS deactivation */
static void oss_deactivate(void) {
  if(ossfd != -1) {
    xclose(ossfd);
    ossfd = -1;
    device_state = device_closed;
    D(("released audio device"));
  }
}

/** @brief OSS backend activation */
static void oss_activate(void) {
  int stereo, format, rate;
  const char *device;

  if(ossfd == -1) {
    /* Try to pick a device */
    if(!strcmp(config->device, "default")) {
      if(access("/dev/dsp", W_OK) == 0)
	device = "/dev/dsp";
      else if(access("/dev/audio", W_OK) == 0)
	device = "/dev/audio";
      else {
        error(0, "cannot determine default OSS device");
        goto failed;
      }
    } else
      device = config->device;	/* just believe the user */
    /* Open the device */
    if((ossfd = open(device, O_WRONLY, 0)) < 0) {
      error(errno, "error opening %s", device);
      goto failed;
    }
    /* Set the audio format */
    stereo = (config->sample_format.channels == 2);
    if(ioctl(ossfd, SNDCTL_DSP_STEREO, &stereo) < 0) {
      error(errno, "error calling ioctl SNDCTL_DSP_STEREO %d", stereo);
      goto failed;
    }
    if(config->sample_format.bits == 8)
      format = AFMT_U8;
    else if(config->sample_format.bits == 16)
      format = (config->sample_format.endian == ENDIAN_LITTLE
		? AFMT_S16_LE : AFMT_S16_BE);
    else {
      error(0, "unsupported sample_format for oss backend"); 
      goto failed;
    }
    if(ioctl(ossfd, SNDCTL_DSP_SETFMT, &format) < 0) {
      error(errno, "error calling ioctl SNDCTL_DSP_SETFMT %#x", format);
      goto failed;
    }
    rate = config->sample_format.rate;
    if(ioctl(ossfd, SNDCTL_DSP_SPEED, &rate) < 0) {
      error(errno, "error calling ioctl SNDCTL_DSP_SPEED %d", rate);
      goto failed;
    }
    if((unsigned)rate != config->sample_format.rate)
      error(0, "asked for %luHz, got %dHz",
	    (unsigned long)config->sample_format.rate, rate);
    nonblock(ossfd);
    device_state = device_open;
  }
  return;
failed:
  device_state = device_error;
  if(ossfd >= 0) {
    xclose(ossfd);
    ossfd = -1;
  }
}

/** @brief Play via OSS */
static size_t oss_play(size_t frames) {
  const size_t bytes_to_play = frames * bpf;
  ssize_t bytes_written;

  bytes_written = write(ossfd, playing->buffer + playing->start,
			bytes_to_play);
  if(bytes_written < 0)
    switch(errno) {
    case EINTR:			/* interruped */
    case EAGAIN:		/* overrun */
      return 0;			/* try again later */
    default:
      fatal(errno, "error writing to audio device");
    }
  return bytes_written / bpf;
}

static int oss_slot;

/** @brief Fill in poll fd array for OSS */
static void oss_beforepoll(int attribute((unused)) *timeoutp) {
  oss_slot = addfd(ossfd, POLLOUT|POLLERR);
}

/** @brief Process poll() results for OSS */
static int oss_ready(void) {
  return !!(fds[oss_slot].revents & (POLLOUT|POLLERR));
}

const struct speaker_backend oss_backend = {
  BACKEND_OSS,
  0,
  oss_init,
  oss_activate,
  oss_play,
  oss_deactivate,
  oss_beforepoll,
  oss_ready
};

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
