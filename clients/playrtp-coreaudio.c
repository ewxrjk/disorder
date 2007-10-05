/*
 * This file is part of DisOrder.
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
/** @file clients/playrtp-coreaudio.c
 * @brief RTP player - Core Audio support
 */

#include <config.h>

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
#include "types.h"

#include <assert.h>
#include <pthread.h>
#include <CoreAudio/AudioHardware.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "heap.h"
#include "playrtp.h"

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
  uint32_t samples_available;

  pthread_mutex_lock(&lock);
  while(nbuffers > 0) {
    float *samplesOut = ab->mData;
    size_t samplesOutLeft = ab->mDataByteSize / sizeof (float);

    while(samplesOutLeft > 0) {
      const struct packet *p = playrtp_next_packet();
      if(p && contains(p, next_timestamp)) {
        /* This packet is ready to play */
        const uint32_t packet_end = p->timestamp + p->nsamples;
        const uint32_t offset = next_timestamp - p->timestamp;
        const uint16_t *ptr = (void *)(p->samples_raw + offset);

        samples_available = packet_end - next_timestamp;
        if(samples_available > samplesOutLeft)
          samples_available = samplesOutLeft;
        next_timestamp += samples_available;
        samplesOutLeft -= samples_available;
        while(samples_available-- > 0)
          *samplesOut++ = (int16_t)ntohs(*ptr++) * (0.5 / 32767);
        /* We don't bother junking the packet - that'll be dealt with next time
         * round */
      } else {
        /* No packet is ready to play (and there might be no packet at all) */
        samples_available = p ? p->timestamp - next_timestamp
                              : samplesOutLeft;
        if(samples_available > samplesOutLeft)
          samples_available = samplesOutLeft;
        //info("infill by %"PRIu32, samples_available);
        /* Conveniently the buffer is 0 to start with */
        next_timestamp += samples_available;
        samplesOut += samples_available;
        samplesOutLeft -= samples_available;
      }
    }
    ++ab;
    --nbuffers;
  }
  pthread_mutex_unlock(&lock);
  return 0;
}

void playrtp_coreaudio(void) {
  OSStatus status;
  UInt32 propertySize;
  AudioDeviceID adid;
  AudioStreamBasicDescription asbd;

  /* If this looks suspiciously like libao's macosx driver there's an
   * excellent reason for that... */

  /* TODO report errors as strings not numbers */
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
  pthread_mutex_lock(&lock);
  for(;;) {
    /* Wait for the buffer to fill up a bit */
    playrtp_fill_buffer();
    /* Start playing now */
    info("Playing...");
    next_timestamp = pheap_first(&packets)->timestamp;
    active = 1;
    status = AudioDeviceStart(adid, adioproc);
    if(status)
      fatal(0, "AudioDeviceStart: %d", (int)status);
    /* Wait until the buffer empties out */
    while(nsamples >= minbuffer
	  || (nsamples > 0
	      && contains(pheap_first(&packets), next_timestamp)))
      pthread_cond_wait(&cond, &lock);
    /* Stop playing for a bit until the buffer re-fills */
    status = AudioDeviceStop(adid, adioproc);
    if(status)
      fatal(0, "AudioDeviceStop: %d", (int)status);
    active = 0;
    /* Go back round */
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
