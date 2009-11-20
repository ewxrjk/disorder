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
#include <time.h>

#include "mem.h"
#include "log.h"
#include "uaudio.h"
#include "configuration.h"

#ifndef AFMT_U16_NE
# if BYTE_ORDER == BIG_ENDIAN
#  define AFMT_U16_NE AFMT_U16_BE
# else
#  define AFMT_U16_NE AFMT_U16_LE
# endif
#endif

/* documentation does not match implementation! */
#ifndef SOUND_MIXER_READ
# define SOUND_MIXER_READ(x) MIXER_READ(x)
#endif
#ifndef SOUND_MIXER_WRITE
# define SOUND_MIXER_WRITE(x) MIXER_WRITE(x)
#endif

static int oss_fd = -1;
static int oss_mixer_fd = -1;
static int oss_mixer_channel;

static const char *const oss_options[] = {
  "device",
  "mixer-device",
  "mixer-channel",
  NULL
};

/** @brief Open the OSS sound device */
static void oss_open(void) {
  const char *device = uaudio_get("device", NULL);

#if EMPEG_HOST
  if(!device || !*device || !strcmp(device, "default"))
    device = "/dev/audio";
#else
  if(!device || !*device || !strcmp(device, "default")) {
    if(access("/dev/dsp", W_OK) == 0)
      device = "/dev/dsp";
    else
      device = "/dev/audio";
  }
#endif
  if((oss_fd = open(device, O_WRONLY, 0)) < 0)
    disorder_fatal(errno, "error opening %s", device);
#if !EMPEG_HOST
  int stereo = (uaudio_channels == 2), format;
  if(ioctl(oss_fd, SNDCTL_DSP_STEREO, &stereo) < 0)
    disorder_fatal(errno, "error calling ioctl SNDCTL_DSP_STEREO %d", stereo);
  if(uaudio_bits == 16)
    format = uaudio_signed ? AFMT_S16_NE : AFMT_U16_NE;
  else
    format = uaudio_signed ? AFMT_S8 : AFMT_U8;
  if(ioctl(oss_fd, SNDCTL_DSP_SETFMT, &format) < 0)
    disorder_fatal(errno, "error calling ioctl SNDCTL_DSP_SETFMT %#x", format);
  int rate = uaudio_rate;
  if(ioctl(oss_fd, SNDCTL_DSP_SPEED, &rate) < 0)
    disorder_fatal(errno, "error calling ioctl SNDCTL_DSP_SPEED %d", rate);
  if(rate != uaudio_rate)
    error(0, "asked for %dHz, got %dHz", uaudio_rate, rate);
#endif
}

/** @brief Close the OSS sound device */
static void oss_close(void) {
  if(oss_fd != -1) {
    close(oss_fd);
    oss_fd = -1;
  }
}

/** @brief Actually play sound via OSS */
static size_t oss_play(void *buffer, size_t samples, unsigned flags) {
  /* cf uaudio-alsa.c:alsa-play() */
  if(flags & UAUDIO_PAUSED) {
    if(flags & UAUDIO_PAUSE)
      oss_close();
    if(samples > 64)
      samples /= 2;
    const uint64_t ns = ((uint64_t)samples * 1000000000
                         / (uaudio_rate * uaudio_channels));
    struct timespec ts[1];
    ts->tv_sec = ns / 1000000000;
    ts->tv_nsec = ns % 1000000000;
    while(nanosleep(ts, ts) < 0 && errno == EINTR)
      ;
    return samples;
  }
  if(flags & UAUDIO_RESUME)
    oss_open();
  const size_t bytes = samples * uaudio_sample_size;
  int rc = write(oss_fd, buffer, bytes);
  if(rc < 0)
    disorder_fatal(errno, "error writing to sound device");
  return rc / uaudio_sample_size;
}

static void oss_start(uaudio_callback *callback,
                      void *userdata) {
  if(uaudio_channels != 1 && uaudio_channels != 2)
    disorder_fatal(0, "asked for %d channels but only support 1 or 2",
          uaudio_channels); 
  if(uaudio_bits != 8 && uaudio_bits != 16)
    disorder_fatal(0, "asked for %d bits/channel but only support 8 or 16",
          uaudio_bits); 
#if EMPEG_HOST
  /* Very specific buffer size requirements here apparently */
  uaudio_thread_start(callback, userdata, oss_play, 
                      4608 / uaudio_sample_size,
                      4608 / uaudio_sample_size,
                      0);
#else
  /* We could SNDCTL_DSP_GETBLKSIZE but only when the device is already open,
   * which is kind of inconvenient.  We go with 1-4Kbyte for now. */
  uaudio_thread_start(callback, userdata, oss_play, 
                      32 / uaudio_sample_size,
                      4096 / uaudio_sample_size,
                      0);
#endif
}

static void oss_stop(void) {
  uaudio_thread_stop();
  oss_close();                          /* might not have been paused */
}

/** @brief Channel names */
static const char *oss_channels[] = SOUND_DEVICE_NAMES;

static int oss_mixer_find_channel(const char *channel) {
  if(!channel[strspn(channel, "0123456789")])
    return atoi(channel);
  else {
    for(unsigned n = 0; n < sizeof oss_channels / sizeof *oss_channels; ++n)
      if(!strcmp(oss_channels[n], channel))
	return n;
    return -1;
  }
}  

static void oss_open_mixer(void) {
  const char *mixer = uaudio_get("mixer-device", "/dev/mixer");
  /* TODO infer mixer-device from device */
  if((oss_mixer_fd = open(mixer, O_RDWR, 0)) < 0)
    disorder_fatal(errno, "error opening %s", mixer);
  const char *channel = uaudio_get("mixer-channel", "pcm");
  oss_mixer_channel = oss_mixer_find_channel(channel);
  if(oss_mixer_channel < 0)
    disorder_fatal(0, "no such channel as '%s'", channel);
}

static void oss_close_mixer(void) {
  close(oss_mixer_fd);
  oss_mixer_fd = -1;
}

static void oss_get_volume(int *left, int *right) {
  int r;

  *left = *right = 0;
  if(ioctl(oss_mixer_fd, SOUND_MIXER_READ(oss_mixer_channel), &r) < 0)
    disorder_error(errno, "error getting volume");
  else {
    *left = r & 0xff;
    *right = (r >> 8) & 0xff;
  }
}

static void oss_set_volume(int *left, int *right) {
  int r =  (*left & 0xff) + (*right & 0xff) * 256;
  if(ioctl(oss_mixer_fd, SOUND_MIXER_WRITE(oss_mixer_channel), &r) == -1)
    disorder_error(errno, "error setting volume");
  else if(ioctl(oss_mixer_fd, SOUND_MIXER_READ(oss_mixer_channel), &r) < 0)
    disorder_error(errno, "error getting volume");
  else {
    *left = r & 0xff;
    *right = (r >> 8) & 0xff;
  }
}

static void oss_configure(void) {
  uaudio_set("device", config->device);
  uaudio_set("mixer-device", config->mixer);
  uaudio_set("mixer-channel", config->channel);
}

const struct uaudio uaudio_oss = {
  .name = "oss",
  .options = oss_options,
  .start = oss_start,
  .stop = oss_stop,
  .activate = uaudio_thread_activate,
  .deactivate = uaudio_thread_deactivate,
  .open_mixer = oss_open_mixer,
  .close_mixer = oss_close_mixer,
  .get_volume = oss_get_volume,
  .set_volume = oss_set_volume,
  .configure = oss_configure,
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
