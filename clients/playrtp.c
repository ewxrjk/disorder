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
#include <locale.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "addr.h"
#include "syscalls.h"
#include "rtp.h"
#include "defs.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
# include <CoreAudio/AudioHardware.h>
#endif
#if API_ALSA
#include <alsa/asoundlib.h>
#endif

#define readahead linux_headers_are_borked

/** @brief RTP socket */
static int rtpfd;

/** @brief Output device */
static const char *device;

/** @brief Maximum samples per packet we'll support
 *
 * NB that two channels = two samples in this program.
 */
#define MAXSAMPLES 2048

/** @brief Minimum low watermark
 *
 * We'll stop playing if there's only this many samples in the buffer. */
static unsigned minbuffer = 2 * 44100 / 10;  /* 0.2 seconds */

/** @brief Maximum sample size
 *
 * The maximum supported size (in bytes) of one sample. */
#define MAXSAMPLESIZE 2

/** @brief Buffer high watermark
 *
 * We'll only start playing when this many samples are available. */
static unsigned readahead = 2 * 2 * 44100;

/** @brief Maximum buffer size
 *
 * We'll stop reading from the network if we have this many samples. */
static unsigned maxbuffer;

/** @brief Number of samples to infill by in one go */
#define INFILL_SAMPLES (44100 * 2)      /* 1s */

/** @brief Received packet
 *
 * Packets are recorded in an ordered linked list. */
struct packet {
  /** @brief Pointer to next packet
   * The next packet might not be immediately next: if packets are dropped
   * or mis-ordered there may be gaps at any given moment. */
  struct packet *next;
  /** @brief Number of samples in this packet */
  uint32_t nsamples;
  /** @brief Timestamp from RTP packet
   *
   * NB that "timestamps" are really sample counters.*/
  uint32_t timestamp;
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
  /** @brief Converted sample data */
  float samples_float[MAXSAMPLES];
#else
  /** @brief Raw sample data */
  unsigned char samples_raw[MAXSAMPLES * MAXSAMPLESIZE];
#endif
};

/** @brief Total number of samples available */
static unsigned long nsamples;

/** @brief Linked list of packets
 *
 * In ascending order of timestamp.  Really this should be a heap for more
 * efficient access. */
static struct packet *packets;

/** @brief Timestamp of next packet to play.
 *
 * This is set to the timestamp of the last packet, plus the number of
 * samples it contained.  Only valid if @ref active is nonzero.
 */
static uint32_t next_timestamp;

/** @brief True if actively playing
 *
 * This is true when playing and false when just buffering. */
static int active;

/** @brief Lock protecting @ref packets */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Condition variable signalled whenever @ref packets is changed */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "debug", no_argument, 0, 'd' },
  { "device", required_argument, 0, 'D' },
  { "min", required_argument, 0, 'm' },
  { "max", required_argument, 0, 'x' },
  { "buffer", required_argument, 0, 'b' },
  { 0, 0, 0, 0 }
};

/** @brief Return true iff a < b in sequence-space arithmetic */
static inline int lt(uint32_t a, uint32_t b) {
  return (uint32_t)(a - b) & 0x80000000;
}

/** @brief Return true iff a >= b in sequence-space arithmetic */
static inline int ge(uint32_t a, uint32_t b) {
  return !lt(a, b);
}

/** @brief Return true iff a > b in sequence-space arithmetic */
static inline int gt(uint32_t a, uint32_t b) {
  return lt(b, a);
}

/** @brief Return true iff a <= b in sequence-space arithmetic */
static inline int le(uint32_t a, uint32_t b) {
  return !lt(b, a);
}

/** @brief Drop the packet at the head of the queue */
static void drop_first_packet(void) {
  struct packet *const p = packets;
  packets = p->next;
  nsamples -= p->nsamples;
  free(p);
  pthread_cond_broadcast(&cond);
}

/** @brief Background thread collecting samples
 *
 * This function collects samples, perhaps converts them to the target format,
 * and adds them to the packet list. */
static void *listen_thread(void attribute((unused)) *arg) {
  struct packet *p = 0, **pp;
  int n;
  union {
    struct rtp_header header;
    uint8_t bytes[sizeof(uint16_t) * MAXSAMPLES + sizeof (struct rtp_header)];
  } packet;
  const uint16_t *const samples = (uint16_t *)(packet.bytes
                                               + sizeof (struct rtp_header));

  for(;;) {
    if(!p)
      p = xmalloc(sizeof *p);
    n = read(rtpfd, packet.bytes, sizeof packet.bytes);
    if(n < 0) {
      switch(errno) {
      case EINTR:
        continue;
      default:
        fatal(errno, "error reading from socket");
      }
    }
    /* Ignore too-short packets */
    if((size_t)n <= sizeof (struct rtp_header))
      continue;
    p->timestamp = ntohl(packet.header.timestamp);
    /* Ignore packets in the past */
    if(active && lt(p->timestamp, next_timestamp)) {
      info("dropping old packet, timestamp=%"PRIx32" < %"PRIx32,
           p->timestamp, next_timestamp);
      continue;
    }
    /* Convert to target format */
    switch(packet.header.mpt & 0x7F) {
    case 10:
      p->nsamples = (n - sizeof (struct rtp_header)) / sizeof(uint16_t);
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
      /* Convert to what Core Audio expects */
      {
        size_t i;

        for(i = 0; i < p->nsamples; ++n)
          p->samples_float[i] = (int16_t)ntohs(samples[i]) * (0.5f / 32767);
      }
#else
      /* ALSA can do any necessary conversion itself (though it might be better
       * to do any necessary conversion in the background) */
      memcpy(p->samples_raw, samples, n - sizeof (struct rtp_header));
#endif
      break;
      /* TODO support other RFC3551 media types (when the speaker does) */
    default:
      fatal(0, "unsupported RTP payload type %d",
            packet.header.mpt & 0x7F);
    }
    pthread_mutex_lock(&lock);
    /* Stop reading if we've reached the maximum.
     *
     * This is rather unsatisfactory: it means that if packets get heavily
     * out of order then we guarantee dropouts.  But for now... */
    while(nsamples >= maxbuffer)
      pthread_cond_wait(&cond, &lock);
    for(pp = &packets;
        *pp && lt((*pp)->timestamp, p->timestamp);
        pp = &(*pp)->next)
      ;
    /* So now either !*pp or *pp >= p */
    if(*pp && p->timestamp == (*pp)->timestamp) {
      /* *pp == p; a duplicate.  Ideally we avoid the translation step here,
       * but we'll worry about that another time. */
      info("dropped a duplicated");
    } else {
      if(*pp)
        info("receiving packets out of order");
      p->next = *pp;
      *pp = p;
      nsamples += p->nsamples;
      pthread_cond_broadcast(&cond);
      p = 0;                            /* we've consumed this packet */
    }
    pthread_mutex_unlock(&lock);
  }
}

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
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

  pthread_mutex_lock(&lock);
  while(nbuffers > 0) {
    float *samplesOut = ab->mData;
    size_t samplesOutLeft = ab->mDataByteSize / sizeof (float);
    
    while(samplesOutLeft > 0) {
      if(packets) {
        /* There's a packet */
        const uint32_t packet_start = packets->timestamp;
        const uint32_t packet_end = packets->timestamp + packets->nsamples;
        
        if(le(packet_end, next_timestamp)) {
          /* This packet is in the past */
          info("dropping buffered past packet %"PRIx32" < %"PRIx32,
               packets->timestamp, next_timestamp);
          continue;
        }
        if(ge(next_timestamp, packet_start)
           && lt(next_timestamp, packet_end)) {
          /* This packet is suitable */
          const uint32_t offset = next_timestamp - packets->timestamp;
          uint32_t samples_available = packet_end - next_timestamp;
          if(samples_available > samplesOutLeft)
            samples_available = samplesOutLeft;
          memcpy(samplesOut,
                 packets->samples_float + offset,
                 samples_available * sizeof(float));
          samplesOut += samples_available;
          next_timestamp += samples_available;
          if(ge(next_timestamp, packet_end))
            drop_first_packet();
          continue;
        }
      }
      /* We didn't find a suitable packet (though there might still be
       * unsuitable ones).  We infill with 0s. */
      if(packets) {
        /* There is a next packet, only infill up to that point */
        uint32_t samples_available = packets->timestamp - next_timestamp;
        
        if(samples_available > samplesOutLeft)
          samples_available = samplesOutLeft;
        /* Convniently the buffer is 0 to start with */
        next_timestamp += samples_available;
        samplesOut += samples_available;
        samplesOutLeft -= samples_available;
        /* TODO log infill */
      } else {
        /* There's no next packet at all */
        next_timestamp += samplesOutLeft;
        samplesOut += samplesOutLeft;
        samplesOutLeft = 0;
        /* TODO log infill */
      }
    }
    ++ab;
    --nbuffers;
  }
  pthread_mutex_unlock(&lock);
  return 0;
}
#endif

/** @brief Play an RTP stream
 *
 * This is the guts of the program.  It is responsible for:
 * - starting the listening thread
 * - opening the audio device
 * - reading ahead to build up a buffer
 * - arranging for audio to be played
 * - detecting when the buffer has got too small and re-buffering
 */
static void play_rtp(void) {
  pthread_t ltid;

  /* We receive and convert audio data in a background thread */
  pthread_create(&ltid, 0, listen_thread, 0);
#if API_ALSA
  {
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    /* Only support one format for now */
    const int sample_format = SND_PCM_FORMAT_S16_BE;
    unsigned rate = 44100;
    const int channels = 2;
    const int samplesize = channels * sizeof(uint16_t);
    snd_pcm_uframes_t pcm_bufsize = MAXSAMPLES * samplesize * 3;
    /* If we can write more than this many samples we'll get a wakeup */
    const int avail_min = 256;
    snd_pcm_sframes_t frames_written;
    size_t samples_written;
    int prepared = 1;
    int err;
    int infilling = 0, escape = 0;
    time_t logged, now;
    uint32_t packet_start, packet_end;

    /* Open ALSA */
    if((err = snd_pcm_open(&pcm,
                           device ? device : "default",
                           SND_PCM_STREAM_PLAYBACK,
                           SND_PCM_NONBLOCK)))
      fatal(0, "error from snd_pcm_open: %d", err);
    /* Set up 'hardware' parameters */
    snd_pcm_hw_params_alloca(&hwparams);
    if((err = snd_pcm_hw_params_any(pcm, hwparams)) < 0)
      fatal(0, "error from snd_pcm_hw_params_any: %d", err);
    if((err = snd_pcm_hw_params_set_access(pcm, hwparams,
                                           SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_access: %d", err);
    if((err = snd_pcm_hw_params_set_format(pcm, hwparams,
                                           sample_format)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_format (%d): %d",
            sample_format, err);
    if((err = snd_pcm_hw_params_set_rate_near(pcm, hwparams, &rate, 0)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_rate (%d): %d",
            rate, err);
    if((err = snd_pcm_hw_params_set_channels(pcm, hwparams,
                                             channels)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_channels (%d): %d",
            channels, err);
    if((err = snd_pcm_hw_params_set_buffer_size_near(pcm, hwparams,
                                                     &pcm_bufsize)) < 0)
      fatal(0, "error from snd_pcm_hw_params_set_buffer_size (%d): %d",
            MAXSAMPLES * samplesize * 3, err);
    if((err = snd_pcm_hw_params(pcm, hwparams)) < 0)
      fatal(0, "error calling snd_pcm_hw_params: %d", err);
    /* Set up 'software' parameters */
    snd_pcm_sw_params_alloca(&swparams);
    if((err = snd_pcm_sw_params_current(pcm, swparams)) < 0)
      fatal(0, "error calling snd_pcm_sw_params_current: %d", err);
    if((err = snd_pcm_sw_params_set_avail_min(pcm, swparams, avail_min)) < 0)
      fatal(0, "error calling snd_pcm_sw_params_set_avail_min %d: %d",
            avail_min, err);
    if((err = snd_pcm_sw_params(pcm, swparams)) < 0)
      fatal(0, "error calling snd_pcm_sw_params: %d", err);

    /* Ready to go */

    time(&logged);
    pthread_mutex_lock(&lock);
    for(;;) {
      /* Wait for the buffer to fill up a bit */
      logged = now;
      info("%lu samples in buffer (%lus)", nsamples,
           nsamples / (44100 * 2));
      info("Buffering...");
      while(nsamples < readahead)
        pthread_cond_wait(&cond, &lock);
      if(!prepared) {
        if((err = snd_pcm_prepare(pcm)))
          fatal(0, "error calling snd_pcm_prepare: %d", err);
        prepared = 1;
      }
      /* Start at the first available packet */
      next_timestamp = packets->timestamp;
      active = 1;
      infilling = 0;
      escape = 0;
      logged = now;
      info("%lu samples in buffer (%lus)", nsamples,
           nsamples / (44100 * 2));
      info("Playing...");
      /* Wait until the buffer empties out */
      while(nsamples >= minbuffer && !escape) {
        time(&now);
        if(now > logged + 10) {
          logged = now;
          info("%lu samples in buffer (%lus)", nsamples,
               nsamples / (44100 * 2));
        }
        if(packets
           && ge(next_timestamp, packets->timestamp + packets->nsamples)) {
          info("dropping buffered past packet %"PRIx32" < %"PRIx32,
               packets->timestamp, next_timestamp);
          drop_first_packet();
          continue;
        }
        /* Wait for ALSA to ask us for more data */
        pthread_mutex_unlock(&lock);
        write(2, ".", 1);               /* TODO remove me sometime */
        switch(err = snd_pcm_wait(pcm, -1)) {
        case 0:
          info("snd_pcm_wait timed out");
          break;
        case 1:
          break;
        default:
          fatal(0, "snd_pcm_wait returned %d", err);
        }
        pthread_mutex_lock(&lock);
        /* ALSA is ready for more data */
        packet_start = packets->timestamp;
        packet_end = packets->timestamp + packets->nsamples;
        if(ge(next_timestamp, packet_start)
           && lt(next_timestamp, packet_end)) {
          /* The target timestamp is somewhere in this packet */
          const uint32_t offset = next_timestamp - packets->timestamp;
          const uint32_t samples_available = (packets->timestamp + packets->nsamples) - next_timestamp;
          const size_t frames_available = samples_available / 2;

          frames_written = snd_pcm_writei(pcm,
                                          packets->samples_raw + offset,
                                          frames_available);
          if(frames_written < 0) {
            switch(frames_written) {
            case -EAGAIN:
              info("snd_pcm_wait() returned but we got -EAGAIN!");
              break;
            case -EPIPE:
              error(0, "error calling snd_pcm_writei: %ld",
                    (long)frames_written);
              escape = 1;
              break;
            default:
              fatal(0, "error calling snd_pcm_writei: %ld",
                    (long)frames_written);
            }
          } else {
            samples_written = frames_written * 2;
            next_timestamp += samples_written;
            if(ge(next_timestamp, packet_end))
              drop_first_packet();
            infilling = 0;
          }
        } else {
          /* We don't have anything to play!  We'd better play some 0s. */
          static const uint16_t zeros[INFILL_SAMPLES];
          size_t samples_available = INFILL_SAMPLES, frames_available;

          /* If the maximum infill would take us past the start of the next
           * packet then we truncate the infill to the right amount. */
          if(lt(packets->timestamp,
                next_timestamp + samples_available))
            samples_available = packets->timestamp - next_timestamp;
          if((int)samples_available < 0) {
            info("packets->timestamp: %"PRIx32"  next_timestamp: %"PRIx32"  next+max: %"PRIx32"  available: %"PRIx32,
                 packets->timestamp, next_timestamp,
                 next_timestamp + INFILL_SAMPLES, samples_available);
          }
          frames_available = samples_available / 2;
          if(!infilling) {
            info("Infilling %d samples, next=%"PRIx32" packet=[%"PRIx32",%"PRIx32"]",
                 samples_available, next_timestamp,
                 packets->timestamp, packets->timestamp + packets->nsamples);
            //infilling++;
          }
          frames_written = snd_pcm_writei(pcm,
                                          zeros,
                                          frames_available);
          if(frames_written < 0) {
            switch(frames_written) {
            case -EAGAIN:
              info("snd_pcm_wait() returned but we got -EAGAIN!");
              break;
            case -EPIPE:
              error(0, "error calling snd_pcm_writei: %ld",
                    (long)frames_written);
              escape = 1;
              break;
            default:
              fatal(0, "error calling snd_pcm_writei: %ld",
                    (long)frames_written);
            }
          } else {
            samples_written = frames_written * 2;
            next_timestamp += samples_written;
          }
        }
      }
      active = 0;
      /* We stop playing for a bit until the buffer re-fills */
      pthread_mutex_unlock(&lock);
      if((err = snd_pcm_nonblock(pcm, 0)))
        fatal(0, "error calling snd_pcm_nonblock: %d", err);
      if(escape) {
        if((err = snd_pcm_drop(pcm)))
          fatal(0, "error calling snd_pcm_drop: %d", err);
        escape = 0;
      } else
        if((err = snd_pcm_drain(pcm)))
          fatal(0, "error calling snd_pcm_drain: %d", err);
      if((err = snd_pcm_nonblock(pcm, 1)))
        fatal(0, "error calling snd_pcm_nonblock: %d", err);
      prepared = 0;
      pthread_mutex_lock(&lock);
    }

  }
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
      while(nsamples < readahead)
        pthread_cond_wait(&cond, &lock);
      /* Start playing now */
      status = AudioDeviceStart(adid, adioproc);
      if(status)
        fatal(0, "AudioDeviceStart: %d", (int)status);
      /* Wait until the buffer empties out */
      while(nsamples >= minbuffer)
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
          "  --device, -D DEVICE     Output device\n"
          "  --min, -m FRAMES        Buffer low water mark\n"
          "  --buffer, -b FRAMES     Buffer high water mark\n"
          "  --max, -x FRAMES        Buffer maximum size\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
          );
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
  char *sockname;

  static const struct addrinfo prefs = {
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
  while((n = getopt_long(argc, argv, "hVdD:m:b:x:", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    case 'D': device = optarg; break;
    case 'm': minbuffer = 2 * atol(optarg); break;
    case 'b': readahead = 2 * atol(optarg); break;
    case 'x': maxbuffer = 2 * atol(optarg); break;
    default: fatal(0, "invalid option");
    }
  }
  if(!maxbuffer)
    maxbuffer = 4 * readahead;
  argc -= optind;
  argv += optind;
  if(argc < 1 || argc > 2)
    fatal(0, "usage: disorder-playrtp [OPTIONS] ADDRESS [PORT]");
  sl.n = argc;
  sl.s = argv;
  /* Listen for inbound audio data */
  if(!(res = get_address(&sl, &prefs, &sockname)))
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
