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

#include <config.h>
#include "types.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "addr.h"
#include "syscalls.h"
#include "rtp.h"
#include "debug.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
# include <CoreAudio/AudioHardware.h>
#endif

static int rtpfd;

#define MAXSAMPLES 2048                 /* max samples/frame we'll support */
/* NB two channels = two samples in this program! */
#define MINBUFFER 8820                  /* when to stop playing */
#define READAHEAD 88200                 /* how far to read ahead */
#define MAXBUFFER (3 * 88200)           /* maximum buffer contents */

struct frame {
  struct frame *next;                   /* another frame */
  int nsamples;                         /* number of samples */
  int nused;                            /* number of samples used so far */
  uint32_t timestamp;                   /* timestamp from packet */
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
  float samples[MAXSAMPLES];            /* converted sample data */
#endif
};

static unsigned long nsamples;          /* total samples available */

static struct frame *frames;            /* received frames in ascending order
                                         * of timestamp */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* lock protecting frame list */

static pthread_cond_t cond = PTHREAD_CONDVAR_INITIALIZER;
/* signalled whenever we add a new frame */

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "debug", no_argument, 0, 'd' },
  { 0, 0, 0, 0 }
};

/* Return true iff a > b in sequence-space arithmetic */
static inline int gt(const struct frame *a, const struct frame *b) {
  return (uint32_t)(a->timestamp - b->timestamp) < 0x80000000;
}

/* Background thread that reads frames over the network and add them to the
 * list */
static listen_thread(void attribute((unused)) *arg) {
  struct frame *f = 0, **ff;
  int n, i;
  union {
    struct rtp_header header;
    uint8_t bytes[sizeof(uint16_t) * MAXSAMPLES + sizeof (struct rtp_header)];
  } packet;
  const uint16_t *const samples = (uint16_t *)(packet.bytes
                                               + sizeof (struct rtp_header));

  for(;;) {
    if(!f)
      f = xmalloc(sizeof *f);
    n = read(rtpfd, packet.bytes, sizeof packet.bytes);
    if(n < 0) {
      switch(errno) {
      case EINTR:
        continue;
      default:
        fatal(errno, "error reading from socket");
      }
    }
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
    /* Convert to target format */
    switch(packet.header.mtp & 0x7F) {
    case 10:
      f->nsamples = (n - sizeof (struct rtp_header)) / sizeof(uint16_t);
      for(i = 0; i < f->nsamples; ++i)
        f->samples[i] = (int16_t)ntohs(samples[i]) * (0.5f / 32767);
      break;
      /* TODO support other RFC3551 media types (when the speaker does) */
    default:
      fatal(0, "unsupported RTP payload type %d", 
            packet.header.mpt & 0x7F);
    }
#endif
    f->used = 0;
    f->timestamp = ntohl(packet.header.timestamp);
    pthread_mutex_lock(&lock);
    /* Stop reading if we've reached the maximum */
    while(nsamples >= MAXBUFFER)
      pthread_cond_wait(&cond, &lock);
    for(ff = &frames; *ff && !gt(*ff, f); ff = &(*ff)->next)
      ;
    f->next = *ff;
    *ff = f;
    nsamples += f->nsamples;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    f = 0;
  }
}

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
static OSStatus adioproc(AudioDeviceID inDevice,
                         const AudioTimeStamp *inNow,
                         const AudioBufferList *inInputData,
                         const AudioTimeStamp *inInputTime,
                         AudioBufferList *outOutputData,
                         const AudioTimeStamp *inOutputTime,
                         void *inClientData) {
  UInt32 nbuffers = outOutputData->mNumberBuffers;
  AudioBuffer *ab = outOutputData->mBuffers;
  float *samplesOut;                    /* where to write samples to */
  size_t samplesOutLeft;                /* space left */
  size_t samplesInLeft;
  size_t samplesToCopy;

  pthread_mutex_lock(&lock);  
  samplesOut = ab->data;
  samplesOutLeft = ab->mDataByteSize / sizeof (float);
  while(frames && nbuffers > 0) {
    if(frames->used == frames->nsamples) {
      /* TODO if we dropped a packet then we should introduce a gap here */
      struct frame *const f = frames;
      frames = f->next;
      free(f);
      pthread_cond_broadcast(&cond);
      continue;
    }
    if(samplesOutLeft == 0) {
      --nbuffers;
      ++ab;
      samplesOut = ab->data;
      samplesOutLeft = ab->mDataByteSize / sizeof (float);
      continue;
    }
    /* Now: (1) there is some data left to read
     *      (2) there is some space to put it */
    samplesInLeft = frames->nsamples - frames->used;
    samplesToCopy = (samplesInLeft < samplesOutLeft
                     ? samplesInLeft : samplesOutLeft);
    memcpy(samplesOut, frame->samples + frames->used, samplesToCopy);
    frames->used += samplesToCopy;
    samplesOut += samplesToCopy;
    samesOutLeft -= samplesToCopy;
  }
  pthread_mutex_unlock(&lock);
  return 0;
}
#endif

void play_rtp(void) {
  pthread_t lt;

  /* We receive and convert audio data in a background thread */
  pthread_create(&lt, 0, listen_thread, 0);
#if API_ALSA
  assert(!"implemented");
#elif HAVE_COREAUDIO_AUDIOHARDWARE_H
  {
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
    D(("mFormatID         %08"PRIx32, asbd.mFormatID));
    D(("mFormatFlags      %08"PRIx32, asbd.mFormatFlags));
    D(("mBytesPerPacket   %08"PRIx32, asbd.mBytesPerPacket));
    D(("mFramesPerPacket  %08"PRIx32, asbd.mFramesPerPacket));
    D(("mBytesPerFrame    %08"PRIx32, asbd.mBytesPerFrame));
    D(("mChannelsPerFrame %08"PRIx32, asbd.mChannelsPerFrame));
    D(("mBitsPerChannel   %08"PRIx32, asbd.mBitsPerChannel));
    D(("mReserved         %08"PRIx32, asbd.mReserved));
    if(asbd.mFormatID != kAudioFormatLinearPCM)
      fatal(0, "audio device does not support kAudioFormatLinearPCM");
    status = AudioDeviceAddIOProc(adid, adioproc, 0);
    if(status)
      fatal(0, "AudioDeviceAddIOProc: %d", (int)status);
    pthread_mutex_lock(&lock);
    for(;;) {
      /* Wait for the buffer to fill up a bit */
      while(nsamples < READAHEAD)
        pthread_cond_wait(&cond, &lock);
      /* Start playing now */
      status = AudioDeviceStart(adid, adioproc);
      if(status)
        fatal(0, "AudioDeviceStart: %d", (int)status);
      /* Wait until the buffer empties out */
      while(nsamples >= MINBUFFER)
        pthread_cond_wait(&cond, &lock);
      /* Stop playing for a bit until the buffer re-fills */
      status = AudioDeviceStop(adid, adioproc);
      if(status)
        fatal(0, "AudioDeviceStop: %d", (int)status);
      /* Go back round */
    }
  }
#else
# error No known audio API
#endif
}

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-playrtp [OPTIONS] ADDRESS [PORT]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --debug, -d             Turn on debugging\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder-playrtp version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  struct addrinfo *res;
  struct stringlist sl;
  const char *sockname;

  static const struct addrinfo prefbind = {
    AI_PASSIVE,
    PF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    0,
    0,
    0,
    0
  };

  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVd", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  argc -= optind;
  argv += optind;
  if(argc < 1 || argc > 2)
    fatal(0, "usage: disorder-playrtp [OPTIONS] ADDRESS [PORT]");
  sl.n = argc;
  sl.s = argv;
  /* Listen for inbound audio data */
  if(!(res = get_address(&sl, &pref, &sockname)))
    exit(1);
  if((rtpfd = socket(res->ai_family,
                     res->ai_socktype,
                     res->ai_protocol)) < 0)
    fatal(errno, "error creating socket");
  if(bind(rtpfd, res->ai_addr, res->ai_addrlen) < 0)
    fatal(errno, "error binding socket to %s", sockname);
  play_rtp();
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
