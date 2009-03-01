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
      unsigned char input[1024];
      const int maxsamples = sizeof input / uaudio_sample_size;
      /* How many samples we'll ask for */
      const int ask = nsamples > maxsamples ? maxsamples : (int)nsamples;
      /* How many we get */
      int got;

      got = coreaudio_callback(input, ask, coreaudio_userdata);
      /* Convert the samples and store in the output buffer */
      nsamples -= got;
      if(uaudio_signed) {
        if(uaudio_bits == 16) {
          const int16_t *ptr = input;
          while(got > 0) {
            --got;
            *samples++ = *ptr++ * (0.5 / 32767);
          }
        } else {
          const int8_t *ptr = input;
          while(got > 0) {
            --got;
            *samples++ = *ptr++ * (0.5 / 127);
          }
        }
      } else {
        if(uaudio_bits == 16) {
          const uint16_t *ptr = input;
          while(got > 0) {
            --got;
            *samples++ = ((int)*ptr++ - 32768) * (0.5 / 32767);
          }
        } else {
          const uint8_t *ptr = input;
          while(got > 0) {
            --got;
            *samples++ = ((int)*ptr++ - 128) * (0.5 / 127);
          }
        }
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

  if(uaudio_bits != 8 && uaudio_bits != 16)
    disorder_fatal("asked for %d bits/channel but only support 8 and 16",
                   uaudio_bits);
  coreaudio_callback = callback;
  coreaudio_userdata = userdata;
  device = uaudio_get("device");
  coreaudio_adid = coreaudio_getdevice(device);
  /* Get the device properties */
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
  /* Check that everything adds up */
  if(asbd.mFormatID != kAudioFormatLinearPCM)
    disorder_fatal(0, "audio device does not support kAudioFormatLinearPCM");
  if(asbd.mSampleRate != uaudio_rate
     || asbd.mChannelsPerFrame != uaudio_channels) {
    disorder_fatal(0, "want %dHz %d channels "
                      "but got %"PRIu32"Hz %"PRIu32" channels",
                   uaudio_rate,
                   uaudio_channels,
                   asbd.mSampleRate,
                   asbd.mChannelsPerFrame);
  }
  /* Add a collector callback */
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
