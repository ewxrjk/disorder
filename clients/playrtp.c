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
/** @file clients/playrtp.c
 * @brief RTP player
 *
 * This player supports Linux (<a href="http://www.alsa-project.org/">ALSA</a>)
 * and Apple Mac (<a
 * href="http://developer.apple.com/audio/coreaudio.html">Core Audio</a>)
 * systems.  There is no support for Microsoft Windows yet, and that will in
 * fact probably an entirely separate program.
 *
 * The program runs (at least) two threads.  listen_thread() is responsible for
 * reading RTP packets off the wire and adding them to the binary heap @ref
 * packets, assuming they are basically sound.
 *
 * The main thread is responsible for actually playing audio.  In ALSA this
 * means it waits until ALSA says it's ready for more audio which it then
 * plays.
 *
 * InCore Audio the main thread is only responsible for starting and stopping
 * play: the system does the actual playback in its own private thread, and
 * calls adioproc() to fetch the audio data.
 *
 * Sometimes it happens that there is no audio available to play.  This may
 * because the server went away, or a packet was dropped, or the server
 * deliberately did not send any sound because it encountered a silence.
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
#include <sys/uio.h>
#include <string.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "addr.h"
#include "syscalls.h"
#include "rtp.h"
#include "defs.h"
#include "vector.h"
#include "heap.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
# include <CoreAudio/AudioHardware.h>
#endif
#if API_ALSA
#include <alsa/asoundlib.h>
#endif

#define readahead linux_headers_are_borked

/** @brief RTP socket */
static int rtpfd;

/** @brief Log output */
static FILE *logfp;

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

/** @brief Buffer high watermark
 *
 * We'll only start playing when this many samples are available. */
static unsigned readahead = 2 * 2 * 44100;

/** @brief Maximum buffer size
 *
 * We'll stop reading from the network if we have this many samples. */
static unsigned maxbuffer;

/** @brief Number of samples to infill by in one go
 *
 * This is an upper bound - in practice we expect the underlying audio API to
 * only ask for a much smaller number of samples in any one go.
 */
#define INFILL_SAMPLES (44100 * 2)      /* 1s */

/** @brief Received packet
 *
 * Received packets are kept in a binary heap (see @ref pheap) ordered by
 * timestamp.
 */
struct packet {
  /** @brief Number of samples in this packet */
  uint32_t nsamples;

  /** @brief Timestamp from RTP packet
   *
   * NB that "timestamps" are really sample counters.  Use lt() or lt_packet()
   * to compare timestamps. 
   */
  uint32_t timestamp;

  /** @brief Flags
   *
   * Valid values are:
   * - @ref IDLE - the idle bit was set in the RTP packet
   */
  unsigned flags;
/** @brief idle bit set in RTP packet*/
#define IDLE 0x0001

  /** @brief Raw sample data
   *
   * Only the first @p nsamples samples are defined; the rest is uninitialized
   * data.
   */
  uint16_t samples_raw[MAXSAMPLES];
};

/** @brief Return true iff \f$a < b\f$ in sequence-space arithmetic
 *
 * Specifically it returns true if \f$(a-b) mod 2^{32} < 2^{31}\f$.
 *
 * See also lt_packet().
 */
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

/** @brief Ordering for packets, used by @ref pheap */
static inline int lt_packet(const struct packet *a, const struct packet *b) {
  return lt(a->timestamp, b->timestamp);
}

/** @struct pheap 
 * @brief Binary heap of packets ordered by timestamp */
HEAP_TYPE(pheap, struct packet *, lt_packet);

/** @brief Binary heap of received packets */
static struct pheap packets;

/** @brief Total number of samples available */
static unsigned long nsamples;

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

/** @brief Structure of free packet list */
union free_packet {
  struct packet p;
  union free_packet *next;
};

/** @brief Linked list of free packets
 *
 * This is a linked list of formerly used packets.  For preference we re-use
 * packets that have already been used rather than unused ones, to limit the
 * size of the program's working set.  If there are no free packets in the list
 * we try @ref next_free_packet instead.
 *
 * Must hold @ref lock when accessing this.
 */
static union free_packet *free_packets;

/** @brief Array of new free packets 
 *
 * There are @ref count_free_packets ready to use at this address.  If there
 * are none left we allocate more memory.
 *
 * Must hold @ref lock when accessing this.
 */
static union free_packet *next_free_packet;

/** @brief Count of new free packets at @ref next_free_packet
 *
 * Must hold @ref lock when accessing this.
 */
static size_t count_free_packets;

/** @brief Lock protecting @ref packets 
 *
 * This also protects the packet memory allocation infrastructure, @ref
 * free_packets and @ref next_free_packet. */
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

/** @brief Return a new packet
 *
 * Assumes that @ref lock is held. */
static struct packet *new_packet(void) {
  struct packet *p;

  if(free_packets) {
    p = &free_packets->p;
    free_packets = free_packets->next;
  } else {
    if(!count_free_packets) {
      next_free_packet = xcalloc(1024, sizeof (union free_packet));
      count_free_packets = 1024;
    }
    p = &(next_free_packet++)->p;
    --count_free_packets;
  }
  return p;
}

/** @brief Free a packet
 *
 * Assumes that @ref lock is held. */
static void free_packet(struct packet *p) {
  union free_packet *u = (union free_packet *)p;
  u->next = free_packets;
  free_packets = u;
}

/** @brief Drop the first packet
 *
 * Assumes that @ref lock is held. 
 */
static void drop_first_packet(void) {
  if(pheap_count(&packets)) {
    struct packet *const p = pheap_remove(&packets);
    nsamples -= p->nsamples;
    free_packet(p);
    pthread_cond_broadcast(&cond);
  }
}

/** @brief Background thread collecting samples
 *
 * This function collects samples, perhaps converts them to the target format,
 * and adds them to the packet list.
 *
 * It is crucial that the gap between successive calls to read() is as small as
 * possible: otherwise packets will be dropped.
 *
 * We use a binary heap to ensure that the unavoidable effort is at worst
 * logarithmic in the total number of packets - in fact if packets are mostly
 * received in order then we will largely do constant work per packet since the
 * newest packet will always be last.
 *
 * Of more concern is that we must acquire the lock on the heap to add a packet
 * to it.  If this proves a problem in practice then the answer would be
 * (probably doubly) linked list with new packets added the end and a second
 * thread which reads packets off the list and adds them to the heap.
 *
 * We keep memory allocation (mostly) very fast by keeping pre-allocated
 * packets around; see @ref new_packet().
 */
static void *listen_thread(void attribute((unused)) *arg) {
  struct packet *p = 0;
  int n;
  struct rtp_header header;
  uint16_t seq;
  uint32_t timestamp;
  struct iovec iov[2];

  for(;;) {
    if(!p) {
      pthread_mutex_lock(&lock);
      p = new_packet();
      pthread_mutex_unlock(&lock);
    }
    iov[0].iov_base = &header;
    iov[0].iov_len = sizeof header;
    iov[1].iov_base = p->samples_raw;
    iov[1].iov_len = sizeof p->samples_raw / sizeof *p->samples_raw;
    n = readv(rtpfd, iov, 2);
    if(n < 0) {
      switch(errno) {
      case EINTR:
        continue;
      default:
        fatal(errno, "error reading from socket");
      }
    }
    /* Ignore too-short packets */
    if((size_t)n <= sizeof (struct rtp_header)) {
      info("ignored a short packet");
      continue;
    }
    timestamp = htonl(header.timestamp);
    seq = htons(header.seq);
    /* Ignore packets in the past */
    if(active && lt(timestamp, next_timestamp)) {
      info("dropping old packet, timestamp=%"PRIx32" < %"PRIx32,
           timestamp, next_timestamp);
      continue;
    }
    pthread_mutex_lock(&lock);
    p->flags = 0;
    p->timestamp = timestamp;
    /* Convert to target format */
    if(header.mpt & 0x80)
      p->flags |= IDLE;
    switch(header.mpt & 0x7F) {
    case 10:
      p->nsamples = (n - sizeof header) / sizeof(uint16_t);
      break;
      /* TODO support other RFC3551 media types (when the speaker does) */
    default:
      fatal(0, "unsupported RTP payload type %d",
            header.mpt & 0x7F);
    }
    if(logfp)
      fprintf(logfp, "sequence %u timestamp %"PRIx32" length %"PRIx32" end %"PRIx32"\n",
              seq, timestamp, p->nsamples, timestamp + p->nsamples);
    /* Stop reading if we've reached the maximum.
     *
     * This is rather unsatisfactory: it means that if packets get heavily
     * out of order then we guarantee dropouts.  But for now... */
    if(nsamples >= maxbuffer) {
      info("Buffer full");
      while(nsamples >= maxbuffer)
        pthread_cond_wait(&cond, &lock);
    }
    /* Add the packet to the heap */
    pheap_insert(&packets, p);
    nsamples += p->nsamples;
    /* We'll need a new packet */
    p = 0;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
  }
}

/** @brief Return true if @p p contains @p timestamp
 *
 * Containment implies that a sample @p timestamp exists within the packet.
 */
static inline int contains(const struct packet *p, uint32_t timestamp) {
  const uint32_t packet_start = p->timestamp;
  const uint32_t packet_end = p->timestamp + p->nsamples;

  return (ge(timestamp, packet_start)
          && lt(timestamp, packet_end));
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
  const struct packet *p;
  uint32_t samples_available;
  struct timeval in, out;

  gettimeofday(&in, 0);
  pthread_mutex_lock(&lock);
  while(nbuffers > 0) {
    float *samplesOut = ab->mData;
    size_t samplesOutLeft = ab->mDataByteSize / sizeof (float);

    while(samplesOutLeft > 0) {
      /* Look for a suitable packet, dropping any unsuitable ones along the
       * way.  Unsuitable packets are ones that are in the past. */
      while(pheap_count(&packets)) {
        p = pheap_first(&packets);
        if(le(p->timestamp + p->nsamples, next_timestamp))
          /* This packet is in the past.  Drop it and try another one. */
          drop_first_packet();
        else
          /* This packet is NOT in the past.  (It might be in the future
           * however.) */
          break;
      }
      p = pheap_count(&packets) ? pheap_first(&packets) : 0;
      if(p && contains(p, next_timestamp)) {
        if(p->flags & IDLE)
          fprintf(stderr, "\nIDLE\n");
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
        write(2, ".", 1);
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
        write(2, "?", 1);
      }
    }
    ++ab;
    --nbuffers;
  }
  pthread_mutex_unlock(&lock);
  gettimeofday(&out, 0);
  {
    static double max;
    double thistime = (out.tv_sec - in.tv_sec) + (out.tv_usec - in.tv_usec) / 1000000.0;
    if(thistime > max)
      fprintf(stderr, "adioproc: %8.8fs\n", max = thistime);
  }
  return 0;
}
#endif


#if API_ALSA
/** @brief PCM handle */
static snd_pcm_t *pcm;

/** @brief True when @ref pcm is up and running */
static int alsa_prepared = 1;

/** @brief Initialize @ref pcm */
static void setup_alsa(void) {
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
  int err;
  
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
}

/** @brief Wait until ALSA wants some audio */
static void wait_alsa(void) {
  struct pollfd fds[64];
  int nfds, err;
  unsigned short events;

  for(;;) {
    do {
      if((nfds = snd_pcm_poll_descriptors(pcm,
                                          fds, sizeof fds / sizeof *fds)) < 0)
        fatal(0, "error calling snd_pcm_poll_descriptors: %d", nfds);
    } while(poll(fds, nfds, -1) < 0 && errno == EINTR);
    if((err = snd_pcm_poll_descriptors_revents(pcm, fds, nfds, &events)))
      fatal(0, "error calling snd_pcm_poll_descriptors_revents: %d", err);
    if(events & POLLOUT)
      return;
  }
}

/** @brief Play some sound via ALSA
 * @param s Pointer to sample data
 * @param n Number of samples
 * @return 0 on success, -1 on non-fatal error
 */
static int alsa_writei(const void *s, size_t n) {
  /* Do the write */
  const snd_pcm_sframes_t frames_written = snd_pcm_writei(pcm, s, n / 2);
  if(frames_written < 0) {
    /* Something went wrong */
    switch(frames_written) {
    case -EAGAIN:
      write(2, "#", 1);
      return 0;
    case -EPIPE:
      error(0, "error calling snd_pcm_writei: %ld",
            (long)frames_written);
      return -1;
    default:
      fatal(0, "error calling snd_pcm_writei: %ld",
            (long)frames_written);
    }
  } else {
    /* Success */
    next_timestamp += frames_written * 2;
    return 0;
  }
}

/** @brief Play the relevant part of a packet
 * @param p Packet to play
 * @return 0 on success, -1 on non-fatal error
 */
static int alsa_play(const struct packet *p) {
  if(p->flags & IDLE)
    write(2, "I", 1);
  write(2, ".", 1);
  return alsa_writei(p->samples_raw + next_timestamp - p->timestamp,
                     (p->timestamp + p->nsamples) - next_timestamp);
}

/** @brief Play some silence
 * @param p Next packet or NULL
 * @return 0 on success, -1 on non-fatal error
 */
static int alsa_infill(const struct packet *p) {
  static const uint16_t zeros[INFILL_SAMPLES];
  size_t samples_available = INFILL_SAMPLES;

  if(p && samples_available > p->timestamp - next_timestamp)
    samples_available = p->timestamp - next_timestamp;
  write(2, "?", 1);
  return alsa_writei(zeros, samples_available);
}

/** @brief Reset ALSA state after we lost synchronization */
static void alsa_reset(int hard_reset) {
  int err;

  if((err = snd_pcm_nonblock(pcm, 0)))
    fatal(0, "error calling snd_pcm_nonblock: %d", err);
  if(hard_reset) {
    if((err = snd_pcm_drop(pcm)))
      fatal(0, "error calling snd_pcm_drop: %d", err);
  } else
    if((err = snd_pcm_drain(pcm)))
      fatal(0, "error calling snd_pcm_drain: %d", err);
  if((err = snd_pcm_nonblock(pcm, 1)))
    fatal(0, "error calling snd_pcm_nonblock: %d", err);
  alsa_prepared = 0;
}
#endif

/** @brief Wait until the buffer is adequately full
 *
 * Must be called with @ref lock held.
 */
static void fill_buffer(void) {
  info("Buffering...");
  while(nsamples < readahead)
    pthread_cond_wait(&cond, &lock);
  next_timestamp = pheap_first(&packets)->timestamp;
  active = 1;
}

/** @brief Find next packet
 * @return Packet to play or NULL if none found
 *
 * The return packet is merely guaranteed not to be in the past: it might be
 * the first packet in the future rather than one that is actually suitable to
 * play.
 *
 * Must be called with @ref lock held.
 */
static struct packet *next_packet(void) {
  while(pheap_count(&packets)) {
    struct packet *const p = pheap_first(&packets);
    if(le(p->timestamp + p->nsamples, next_timestamp)) {
      /* This packet is in the past.  Drop it and try another one. */
      drop_first_packet();
    } else
      /* This packet is NOT in the past.  (It might be in the future
       * however.) */
      return p;
  }
  return 0;
}

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
    struct packet *p;
    int escape, err;

    /* Open the sound device */
    setup_alsa();
    pthread_mutex_lock(&lock);
    for(;;) {
      /* Wait for the buffer to fill up a bit */
      fill_buffer();
      if(!alsa_prepared) {
        if((err = snd_pcm_prepare(pcm)))
          fatal(0, "error calling snd_pcm_prepare: %d", err);
        alsa_prepared = 1;
      }
      escape = 0;
      info("Playing...");
      /* Keep playing until the buffer empties out, or ALSA tells us to get
       * lost */
      while(nsamples >= minbuffer && !escape) {
        /* Wait for ALSA to ask us for more data */
        pthread_mutex_unlock(&lock);
        wait_alsa();
        pthread_mutex_lock(&lock);
        /* ALSA is ready for more data, find something to play */
        p = next_packet();
        /* Play it or play some silence */
        if(contains(p, next_timestamp))
          escape = alsa_play(p);
        else
          escape = alsa_infill(p);
      }
      active = 0;
      /* We stop playing for a bit until the buffer re-fills */
      pthread_mutex_unlock(&lock);
      alsa_reset(escape);
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
      fill_buffer();
      /* Start playing now */
      info("Playing...");
      next_timestamp = pheap_first(&packets)->timestamp;
      active = 1;
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
      active = 0;
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
  while((n = getopt_long(argc, argv, "hVdD:m:b:x:L:", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    case 'D': device = optarg; break;
    case 'm': minbuffer = 2 * atol(optarg); break;
    case 'b': readahead = 2 * atol(optarg); break;
    case 'x': maxbuffer = 2 * atol(optarg); break;
    case 'L': logfp = fopen(optarg, "w"); break;
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
