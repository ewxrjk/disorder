/*
 * This file is part of DisOrder.
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file lib/uaudio-oss.c
 * @brief Support for OSS backend */
#include "common.h"

#if HAVE_SYS_SOUNDCARD_H || EMPEG_HOST

#if HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "mem.h"
#include "log.h"
#include "uaudio.h"

#ifndef AFMT_U16_NE
# if BYTE_ORDER == BIG_ENDIAN
#  define AFMT_U16_NE AFMT_U16_BE
# else
#  define AFMT_U16_NE AFMT_U16_LE
# endif
#endif

static int oss_fd = -1;

static const char *const oss_options[] = {
  "device",
  NULL
};

/** @brief Actually play sound via OSS */
static size_t oss_play(void *buffer, size_t samples) {
  const size_t bytes = samples * uaudio_sample_size;
  int rc = write(oss_fd, buffer, bytes);
  if(rc < 0)
    fatal(errno, "error writing to sound device");
  return rc / uaudio_sample_size;
}

/** @brief Open the OSS sound device */
static void oss_open(void) {
  const char *device = uaudio_get("device");

#if EMPEG_HOST
  if(!device || !*device || !strcmp(device, "default"))
    device "/dev/audio";
#else
  if(!device || !*device || !strcmp(device, "default")) {
    if(access("/dev/dsp", W_OK) == 0)
      device = "/dev/dsp";
    else
      device = "/dev/audio";
  }
#endif
  if((oss_fd = open(device, O_WRONLY, 0)) < 0)
    fatal(errno, "error opening %s", device);
#if !EMPEG_HOST
  int stereo = (uaudio_channels == 2), format;
  if(ioctl(oss_fd, SNDCTL_DSP_STEREO, &stereo) < 0)
    fatal(errno, "error calling ioctl SNDCTL_DSP_STEREO %d", stereo);
  if(uaudio_bits == 16)
    format = uaudio_signed ? AFMT_S16_NE : AFMT_U16_NE;
  else
    format = uaudio_signed ? AFMT_S8 : AFMT_U8;
  if(ioctl(oss_fd, SNDCTL_DSP_SETFMT, &format) < 0)
    fatal(errno, "error calling ioctl SNDCTL_DSP_SETFMT %#x", format);
  int rate = uaudio_rate;
  if(ioctl(oss_fd, SNDCTL_DSP_SPEED, &rate) < 0)
    fatal(errno, "error calling ioctl SNDCTL_DSP_SPEED %d", rate);
  if(rate != uaudio_rate)
    error(0, "asked for %dHz, got %dHz", uaudio_rate, rate);
#endif
}

static void oss_activate(void) {
  oss_open();
  uaudio_thread_activate();
}

static void oss_deactivate(void) {
  uaudio_thread_deactivate();
  close(oss_fd);
  oss_fd = -1;
}
  
static void oss_start(uaudio_callback *callback,
                      void *userdata) {
  if(uaudio_channels != 1 && uaudio_channels != 2)
    fatal(0, "asked for %d channels but only support 1 or 2",
          uaudio_channels); 
  if(uaudio_bits != 8 && uaudio_bits != 16)
    fatal(0, "asked for %d bits/channel but only support 8 or 16",
          uaudio_bits); 
#if EMPEG_HOST
  /* Very specific buffer size requirements here apparently */
  uaudio_thread_start(callback, userdata, oss_play, 
                      4608 / uaudio_sample_size,
                      4608 / uaudio_sample_size);
#else
  /* We could SNDCTL_DSP_GETBLKSIZE but only when the device is already open,
   * which is kind of inconvenient.  We go with 1-4Kbyte for now. */
  uaudio_thread_start(callback, userdata, oss_play, 
                      32 / uaudio_sample_size,
                      4096 / uaudio_sample_size);
#endif
}

static void oss_stop(void) {
  uaudio_thread_stop();
}

const struct uaudio uaudio_oss = {
  .name = "oss",
  .options = oss_options,
  .start = oss_start,
  .stop = oss_stop,
  .activate = oss_activate,
  .deactivate = oss_deactivate
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
