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
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "mem.h"
#include "log.h"
#include "syscalls.h"

static int oss_fd = -1;
static pthread_t oss_thread;
static uaudio_callback *oss_callback;
static int oss_started;
static int oss_activated;
static int oss_going;
static pthread_mutex_t oss_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t oss_cond = PTHREAD_COND_INITIALIZER;
static int oss_bufsize;

static const char *const oss_options[] = {
  "device",
  NULL
};

static void *oss_thread(void *userdata) {
  int16_t *buffer;
  pthread_mutex_lock(&oss_lock);
  buffer = xmalloc(XXX);
  while(oss_started) {
    while(oss_started && !oss_activated)
      pthread_cond_wait(&oss_cond, &oss_lock);
    if(!oss_started)
      break;
    /* We are definitely active now */
    oss_going = 1;
    pthread_cond_signal(&oss_cond);
    while(oss_activated) {
      const int nsamples = oss_callback(buffer, oss_bufsizeXXX, userdata);;
      if(write(oss_fd, buffer, XXX) < 0)
        fatal(errno, "error playing audio");
      // TODO short writes!
    }
    oss_going = 0;
    pthread_cond_signal(&oss_cond);
  }
  pthread_mutex_unlock(&oss_lock);
  return NULL;
}

static void oss_start(uaudio_callback *callback,
                      void *userdata) {
  int e;
  oss_callback = callback;
  if((e = pthread_create(&oss_thread,
                         NULL,
                         oss_thread_fn,
                         userdata)))
    fatal(e, "pthread_create");
}

static void oss_stop(void) {
  void *result;

  oss_deactivate();
  pthread_mutex_lock(&oss_lock);
  oss_started = 0;
  pthread_cond_signal(&oss_cond);
  pthread_mutex_unlock(&oss_lock);
  pthread_join(oss_thread, &result);
}

static void oss_activate(void) {
  pthread_mutex_lock(&oss_lock);
  if(!oss_activated) {
    const char *device = uaudio_get(&uadiuo_oss, "device");

#if EMPEG_HOST
    if(!device || !*device || !strcmp(device, "default")) {
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
#if EMPEG_HOST
    /* empeg audio driver only knows /dev/audio, only supports the equivalent
     * of AFMT_S16_NE, has a fixed buffer size, and does not support the
     * SNDCTL_ ioctls. */
    oss_bufsize = 4608;
#else
    int stereo = (config->sample_format.channels == 2), format, rate;
    if(ioctl(ossfd, SNDCTL_DSP_STEREO, &stereo) < 0) {
      error(errno, "error calling ioctl SNDCTL_DSP_STEREO %d", stereo);
      goto failed;
    }
    /* TODO we need to think about where we decide this */
    if(config->sample_format.bits == 8)
      format = AFMT_U8;
    else if(config->sample_format.bits == 16)
      format = (config->sample_format.endian == ENDIAN_LITTLE
                ? AFMT_S16_LE : AFMT_S16_BE);
    else
      fatal(0, "unsupported sample_format for oss backend"); 
    if(ioctl(oss_fd, SNDCTL_DSP_SETFMT, &format) < 0)
      fatal(errno, "error calling ioctl SNDCTL_DSP_SETFMT %#x", format);
    rate = config->sample_format.rate;
    if(ioctl(oss_fd, SNDCTL_DSP_SPEED, &rate) < 0)
      fatal(errno, "error calling ioctl SNDCTL_DSP_SPEED %d", rate);
    if((unsigned)rate != config->sample_format.rate)
      error(0, "asked for %luHz, got %dHz",
	    (unsigned long)config->sample_format.rate, rate);
    if(ioctl(oss_fd, SNDCTL_DSP_GETBLKSIZE, &oss_bufsize) < 0) {
      error(errno, "ioctl SNDCTL_DSP_GETBLKSIZE");
      oss_bufsize = 2048;       /* guess */
    }
#endif
    oss_activated = 1;
    pthread_cond_signal(&oss_cond);
    while(!oss_going)
      pthread_cond_wait(&oss_cond, &oss_lock);
  }
  pthread_mutex_unlock(&oss_lock);
}

static void oss_deactivate(void) {
  pthread_mutex_lock(&oss_lock);
  if(oss_activated) {
    oss_activated = 0;
    pthread_cond_signal(&oss_cond);
    while(oss_going)
      pthread_cond_wait(&oss_cond, &oss_lock);
    close(oss_fd);
    oss_fd = -1;
  }
  pthread_mutex_unlock(&oss_lock);
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
