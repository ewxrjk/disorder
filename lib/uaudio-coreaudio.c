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
/** @file lib/uaudio-coreaudio.c
 * @brief Support for Core Audio backend */
#include "common.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H

#include "coreaudio.h"
#include "uaudio.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"

/** @brief Callback to request sample data */
static uaudio_callback *coreaudio_callback;

/** @brief Userdata for @ref coreaudio_callback */
static void *coreaudio_userdata;

/** @brief Core Audio device ID */
static AudioDeviceID coreaudio_adid;

/** @brief Core Audio option names */
static const char *const coreaudio_options[] = {
  "device",
  NULL
};

/** @brief Callback from Core Audio
 *
 * Core Audio demands floating point samples but we provide integers.
 * So there is a conversion step in here.
 */
static OSStatus coreaudio_adioproc
    (AudioDeviceID attribute((unused)) inDevice,
     const AudioTimeStamp attribute((unused)) *inNow,
     const AudioBufferList attribute((unused)) *inInputData,
     const AudioTimeStamp attribute((unused)) *inInputTime,
     AudioBufferList *outOutputData,
     const AudioTimeStamp attribute((unused)) *inOutputTime,
     void attribute((unused)) *inClientData) {
  /* Number of buffers we must fill */
  unsigned nbuffers = outOutputData->mNumberBuffers;
  /* Pointer to buffer to fill */
  AudioBuffer *ab = outOutputData->mBuffers;
  
  while(nbuffers > 0) {
    /* Where to store converted sample data */
    float *samples = ab->mData;
    /* Number of samples left to fill */
    size_t nsamples = ab->mDataByteSize / sizeof (float);

    while(nsamples > 0) {
      /* Integer-format input buffer */
      int16_t input[1024], *ptr = input;
      /* How many samples we'll ask for */
      const int ask = nsamples > 1024 ? 1024 : (int)nsamples;
      /* How many we get */
      int got;

      got = coreaudio_callback(input, ask, coreaudio_userdata);
      /* Convert the samples and store in the output buffer */
      nsamples -= got;
      while(got > 0) {
        --got;
        *samples++ = *ptr++ * (0.5 / 32767);
      }
    }
    /* Move on to the next buffer */
    ++ab;
    --nbuffers;
  }
  return 0;
}

static void coreaudio_start(uaudio_callback *callback,
                            void *userdata) {
  OSStatus status;
  UInt32 propertySize;
  AudioStreamBasicDescription asbd;
  const char *device;

  coreaudio_callback = callback;
  coreaudio_userdata = userdata;
  device = uaudio_get("device");
  coreaudio_adid = coreaudio_getdevice(device);
  propertySize = sizeof asbd;
  status = AudioDeviceGetProperty(coreaudio_adid, 0, false,
				  kAudioDevicePropertyStreamFormat,
				  &propertySize, &asbd);
  if(status)
    coreaudio_fatal(status, "AudioHardwareGetProperty");
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
    disorder_fatal(0, "audio device does not support kAudioFormatLinearPCM");
  status = AudioDeviceAddIOProc(coreaudio_adid, coreaudio_adioproc, 0);
  if(status)
    coreaudio_fatal(status, "AudioDeviceAddIOProc");
}

static void coreaudio_stop(void) {
}

static void coreaudio_activate(void) {
  OSStatus status;

  status = AudioDeviceStart(coreaudio_adid, coreaudio_adioproc);
  if(status)
    coreaudio_fatal(status, "AudioDeviceStart");
}

static void coreaudio_deactivate(void) {
  OSStatus status;

  status = AudioDeviceStop(coreaudio_adid, coreaudio_adioproc);
  if(status)
    coreaudio_fatal(status, "AudioDeviceStop");
}

const struct uaudio uaudio_coreaudio = {
  .name = "coreaudio",
  .options = coreaudio_options,
  .start = coreaudio_start,
  .stop = coreaudio_stop,
  .activate = coreaudio_activate,
  .deactivate = coreaudio_deactivate
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
