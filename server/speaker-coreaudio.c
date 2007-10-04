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
/** @file server/speaker-coreaudio.c
 * @brief Support for @ref BACKEND_COREAUDIO
 *
 * Core Audio likes to make callbacks from a separate player thread
 * which then fill in the required number of bytes of audio.  We fit
 * this into the existing architecture by means of a pipe between the
 * threads.
 *
 * We currently only support 16-bit 44100Hz stereo (and enforce this
 * in @ref lib/configuration.c.)  There are some nasty bodges in this
 * code which depend on this and on further assumptions still...
 *
 * @todo support @ref config::device
 */

#include <config.h>

#if HAVE_COREAUDIO_AUDIOHARDWARE_H

#include "types.h"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <CoreAudio/AudioHardware.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "speaker-protocol.h"
#include "speaker.h"

/** @brief Core Audio Device ID */
static AudioDeviceID adid;

/** @brief Pipe between main and player threads
 *
 * We'll write samples to pfd[1] and read them from pfd[0].
 */
static int pfd[2];

/** @brief Slot number in poll array */
static int pfd_slot;

/** @brief Callback from Core Audio */
static OSStatus adioproc
    (AudioDeviceID attribute((unused)) inDevice,
     const AudioTimeStamp attribute((unused)) *inNow,
     const AudioBufferList attribute((unused)) *inInputData,
     const AudioTimeStamp attribute((unused)) *inInputTime,
     AudioBufferList *outOutputData,
     const AudioTimeStamp attribute((unused)) *inOutputTime,
     void attribute((unused)) *inClientData) {
  UInt32 nbuffers = outOutputData->mNumberBuffers;
  AudioBuffer *ab = outOutputData->mBuffers;

  while(nbuffers > 0) {
    float *samplesOut = ab->mData;
    size_t samplesOutLeft = ab->mDataByteSize / sizeof (float);
    int16_t input[1024], *ptr;
    size_t bytes;
    ssize_t bytes_read;
    size_t samples;

    while(samplesOutLeft > 0) {
      /* Read some more data */
      bytes = samplesOutLeft * sizeof (int16_t);
      if(bytes > sizeof input)
	bytes = sizeof input;
      
      bytes_read = read(pfd[0], input, bytes);
      if(bytes_read < 0)
	switch(errno) {
	case EINTR:
	  continue;		/* just try again */
	case EAGAIN:
	  return 0;		/* underrun - just play 0s */
	default:
	  fatal(errno, "read error in core audio thread");
	}
      assert(bytes_read % 4 == 0); /* TODO horrible bodge! */
      samples = bytes_read / sizeof (int16_t);
      assert(samples <= samplesOutLeft);
      ptr = input;
      samplesOutLeft -= samples;
      while(samples-- > 0)
	*samplesOut++ = *ptr++ * (0.5 / 32767);
    }
    ++ab;
    --nbuffers;
  }
  return 0;
}

/** @brief Core Audio backend initialization */
static void coreaudio_init(void) {
  OSStatus status;
  UInt32 propertySize;
  AudioStreamBasicDescription asbd;

  propertySize = sizeof adid;
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
				    &propertySize, &adid);
  if(status)
    fatal(0, "AudioHardwareGetProperty: %d", (int)status);
  if(adid == kAudioDeviceUnknown)
    fatal(0, "no output device");
  propertySize = sizeof asbd;
  status = AudioDeviceGetProperty(adid, 0, false,
				  kAudioDevicePropertyStreamFormat,
				  &propertySize, &asbd);
  if(status)
    fatal(0, "AudioHardwareGetProperty: %d", (int)status);
  D(("mSampleRate       %f", asbd.mSampleRate));
  D(("mFormatID         %08lx", asbd.mFormatID));
  D(("mFormatFlags      %08lx", asbd.mFormatFlags));
  D(("mBytesPerPacket   %08lx", asbd.mBytesPerPacket));
  D(("mFramesPerPacket  %08lx", asbd.mFramesPerPacket));
  D(("mBytesPerFrame    %08lx", asbd.mBytesPerFrame));
  D(("mChannelsPerFrame %08lx", asbd.mChannelsPerFrame));
  D(("mBitsPerChannel   %08lx", asbd.mBitsPerChannel));
  D(("mReserved         %08lx", asbd.mReserved));
  if(asbd.mFormatID != kAudioFormatLinearPCM)
    fatal(0, "audio device does not support kAudioFormatLinearPCM");
  status = AudioDeviceAddIOProc(adid, adioproc, 0);
  if(status)
    fatal(0, "AudioDeviceAddIOProc: %d", (int)status);
  if(socketpair(PF_UNIX, SOCK_STREAM, 0, pfd) < 0)
    fatal(errno, "error calling socketpair");
  nonblock(pfd[0]);
  nonblock(pfd[1]);
  info("selected Core Audio backend");
}

/** @brief Core Audio deactivation */
static void coreaudio_deactivate(void) {
  const OSStatus status = AudioDeviceStop(adid, adioproc);
  if(status) {
    error(0, "AudioDeviceStop: %d", (int)status);
    device_state = device_error;
  } else
    device_state = device_closed;
}

/** @brief Core Audio backend activation */
static void coreaudio_activate(void) {
  const OSStatus status = AudioDeviceStart(adid, adioproc);

  if(status) {
    error(0, "AudioDeviceStart: %d", (int)status);
    device_state = device_error;  
  }
  device_state = device_open;
}

/** @brief Play via Core Audio */
static size_t coreaudio_play(size_t frames) {
  static size_t leftover;

  size_t bytes = frames * bpf + leftover;
  ssize_t bytes_written;

  if(leftover)
    /* There is a partial frame left over from an earlier write.  Try
     * and finish that off before doing anything else. */
    bytes = leftover;
  bytes_written = write(pfd[1], playing->buffer + playing->start, bytes);
  if(bytes_written < 0)
    switch(errno) {
    case EINTR:			/* interrupted */
    case EAGAIN:		/* buffer full */
      return 0;			/* try later */
    default:
      fatal(errno, "error writing to core audio player thread");
    }
  if(leftover) {
    /* We were dealing the leftover bytes of a partial frame */
    leftover -= bytes_written;
    return !leftover;
  }
  leftover = bytes_written % bpf;
  return bytes_written / bpf;
}

/** @brief Fill in poll fd array for Core Audio */
static void coreaudio_beforepoll(void) {
  pfd_slot = addfd(pfd[1], POLLOUT);
}

/** @brief Process poll() results for Core Audio */
static int coreaudio_ready(void) {
  return !!(fds[pfd_slot].revents & (POLLOUT|POLLERR));
}

/** @brief Backend definition for Core Audio */
const struct speaker_backend coreaudio_backend = {
  BACKEND_COREAUDIO,
  0,
  coreaudio_init,
  coreaudio_activate,
  coreaudio_play,
  coreaudio_deactivate,
  coreaudio_beforepoll,
  coreaudio_ready
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
