/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
 * Portions copyright (C) 2007 Ross Younger
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
/** @file clients/playrtp-oss.c
 * @brief RTP player - OSS and empeg support
 */

#include <config.h>

#if HAVE_SYS_SOUNDCARD_H || EMPEG_HOST
#include "types.h"

#include <poll.h>
#include <sys/ioctl.h>
#if !EMPEG_HOST
#include <sys/soundcard.h>
#endif
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "heap.h"
#include "syscalls.h"
#include "playrtp.h"

/** @brief /dev/dsp (or whatever) */
static int playrtp_oss_fd = -1;

/** @brief Audio buffer */
static char *playrtp_oss_buffer;

/** @brief Size of @ref playrtp_oss_buffer in bytes */
static int playrtp_oss_bufsize;

/** @brief Number of bytes used in @ref playrtp_oss_buffer */
static int playrtp_oss_bufused;

/** @brief Open and configure the OSS audio device */
static void playrtp_oss_enable(void) {
  if(playrtp_oss_fd == -1) {
#if EMPEG_HOST
    /* empeg audio driver only knows /dev/audio, only supports the equivalent
     * of AFMT_S16_NE, has a fixed buffer size, and does not support the
     * SNDCTL_ ioctls. */
    if(!device)
      device = "/dev/audio";
    if((playrtp_oss_fd = open(device, O_WRONLY)) < 0)
      fatal(errno, "error opening %s", device);
    playrtp_oss_bufsize = 4608;
#else
    int rate = 44100, stereo = 1, format = AFMT_S16_BE;
    if(!device) {
      if(access("/dev/dsp", W_OK) == 0)
	device = "/dev/dsp";
      else if(access("/dev/audio", W_OK) == 0)
	device = "/dev/audio";
      else
	fatal(0, "cannot determine default audio device");
    }
    if((playrtp_oss_fd = open(device, O_WRONLY)) < 0)
      fatal(errno, "error opening %s", device);
    if(ioctl(playrtp_oss_fd, SNDCTL_DSP_SETFMT, &format) < 0)
      fatal(errno, "ioctl SNDCTL_DSP_SETFMT");
    if(ioctl(playrtp_oss_fd, SNDCTL_DSP_STEREO, &stereo) < 0)
      fatal(errno, "ioctl SNDCTL_DSP_STEREO");
    if(ioctl(playrtp_oss_fd, SNDCTL_DSP_SPEED, &rate) < 0)
      fatal(errno, "ioctl SNDCTL_DSP_SPEED");
    if(rate != 44100)
      error(0, "asking for 44100Hz, got %dHz", rate);
    if(ioctl(playrtp_oss_fd, SNDCTL_DSP_GETBLKSIZE, &playrtp_oss_bufsize) < 0)
      fatal(errno, "ioctl SNDCTL_DSP_GETBLKSIZE");
    info("OSS buffer size %d", playrtp_oss_bufsize);
#endif
    playrtp_oss_buffer = xmalloc(playrtp_oss_bufsize);
    playrtp_oss_bufused = 0;
    nonblock(playrtp_oss_fd);
  }
}

/** @brief Flush the OSS output buffer
 * @return 0 on success, non-0 on error
 */
static int playrtp_oss_flush(void) {
  int nbyteswritten;

  if(!playrtp_oss_bufused)
    return 0;                           /* nothing to do */
  /* 0 out the unused portion of the buffer */
  memset(playrtp_oss_buffer + playrtp_oss_bufused, 0,
         playrtp_oss_bufsize - playrtp_oss_bufused);
#if EMPEG_HOST 
  /* empeg audio driver insists on native-endian samples */
  {
    uint16_t *ptr,
      *const limit = (uint16_t *)(playrtp_oss_buffer + playrtp_oss_bufused);

    for(ptr = (uint16_t *)playrtp_oss_buffer; ptr < limit; ++ptr)
      *ptr = ntohs(*ptr);
  }
#endif
  for(;;) {
    nbyteswritten = write(playrtp_oss_fd,
                          playrtp_oss_buffer, playrtp_oss_bufsize);
    if(nbyteswritten < 0) {
      switch(errno) {
      case EINTR:
        break;                          /* try again */
      case EAGAIN:
        return 0;                       /* try later */
      default:
        error(errno, "error writing to %s", device);
        return -1;
      }
    } else {
      if(nbyteswritten < playrtp_oss_bufsize)
        error(0, "%s: short write (%d/%d)",
              device, nbyteswritten, playrtp_oss_bufsize);
      playrtp_oss_bufused = 0;
      return 0;
    }
  }
}

/** @brief Wait until the audio device can accept more data */
static void playrtp_oss_wait(void) {
  struct pollfd fds[1];
  int n;

  do {
    fds[0].fd = playrtp_oss_fd;
    fds[0].events = POLLOUT;
    while((n = poll(fds, 1, -1)) < 0 && errno == EINTR)
      ;
    if(n < 0)
      fatal(errno, "calling poll");
  } while(!(fds[0].revents & (POLLOUT|POLLERR)));
}

/** @brief Close the OSS output device
 * @param hard If nonzero, drop pending data
 */
static void playrtp_oss_disable(int hard) {
  if(hard) {
#if !EMPEG_HOST
    /* No SNDCTL_DSP_ ioctls on empeg */
    if(ioctl(playrtp_oss_fd, SNDCTL_DSP_RESET, 0) < 0)
      error(errno, "ioctl SNDCTL_DSP_RESET");
#endif
  } else
    playrtp_oss_flush();
  xclose(playrtp_oss_fd);
  playrtp_oss_fd = -1;
  free(playrtp_oss_buffer);
  playrtp_oss_buffer = 0;
}

/** @brief Write samples to OSS output device
 * @param data Pointer to sample data
 * @param nsamples Number of samples
 * @return 0 on success, non-0 on error
 */
static int playrtp_oss_write(const char *data, size_t samples) {
  long bytes = samples * sizeof(int16_t);
  while(bytes > 0) {
    int n = playrtp_oss_bufsize - playrtp_oss_bufused;

    if(n > bytes)
      n = bytes;
    memcpy(playrtp_oss_buffer + playrtp_oss_bufused, data, n);
    bytes -= n;
    data += n;
    playrtp_oss_bufused += n;
    if(playrtp_oss_bufused == playrtp_oss_bufsize)
      if(playrtp_oss_flush())
        return -1;
  }
  next_timestamp += samples;
  return 0;
}

/** @brief Play some data from packet @p p
 *
 * @p p is assumed to contain @ref next_timestamp.
 */
static int playrtp_oss_play(const struct packet *p) {
  return playrtp_oss_write
    ((const char *)(p->samples_raw + next_timestamp - p->timestamp),
     (p->timestamp + p->nsamples) - next_timestamp);
}

/** @brief Play some silence before packet @p p
 *
 * @p p is assumed to be entirely before @ref next_timestamp.
 */
static int playrtp_oss_infill(const struct packet *p) {
  static const char zeros[INFILL_SAMPLES * sizeof(int16_t)];
  size_t samples_available = INFILL_SAMPLES;

  if(p && samples_available > p->timestamp - next_timestamp)
    samples_available = p->timestamp - next_timestamp;
  return playrtp_oss_write(zeros, samples_available);
}

/** @brief OSS backend for playrtp */
void playrtp_oss(void) {
  int escape;
  const struct packet *p;

  pthread_mutex_lock(&lock);
  for(;;) {
    /* Wait for the buffer to fill up a bit */
    playrtp_fill_buffer();
    playrtp_oss_enable();
    escape = 0;
    info("Playing...");
    /* Keep playing until the buffer empties out, we get an error */
    while((nsamples >= minbuffer
	   || (nsamples > 0
	       && contains(pheap_first(&packets), next_timestamp)))
	  && !escape) {
      /* Wait until we can play more */
      pthread_mutex_unlock(&lock);
      playrtp_oss_wait();
      pthread_mutex_lock(&lock);
      /* Device is ready for more data, find something to play */
      p = playrtp_next_packet();
      /* Play it or play some silence */
      if(contains(p, next_timestamp))
	escape = playrtp_oss_play(p);
      else
	escape = playrtp_oss_infill(p);
    }
    active = 0;
    /* We stop playing for a bit until the buffer re-fills */
    pthread_mutex_unlock(&lock);
    playrtp_oss_disable(escape);
    pthread_mutex_lock(&lock);
  }
}

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
