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
 * The program runs (at least) three threads.  listen_thread() is responsible
 * for reading RTP packets off the wire and adding them to the linked list @ref
 * received_packets, assuming they are basically sound.  queue_thread() takes
 * packets off this linked list and adds them to @ref packets (an operation
 * which might be much slower due to contention for @ref lock).
 *
 * The main thread is responsible for actually playing audio.  In ALSA this
 * means it waits until ALSA says it's ready for more audio which it then
 * plays.  See @ref clients/playrtp-alsa.c.
 *
 * In Core Audio the main thread is only responsible for starting and stopping
 * play: the system does the actual playback in its own private thread, and
 * calls adioproc() to fetch the audio data.  See @ref
 * clients/playrtp-coreaudio.c.
 *
 * Sometimes it happens that there is no audio available to play.  This may
 * because the server went away, or a packet was dropped, or the server
 * deliberately did not send any sound because it encountered a silence.
 *
 * Assumptions:
 * - it is safe to read uint32_t values without a lock protecting them
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
#include <assert.h>
#include <errno.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "addr.h"
#include "syscalls.h"
#include "rtp.h"
#include "defs.h"
#include "vector.h"
#include "heap.h"
#include "timeval.h"
#include "playrtp.h"

#define readahead linux_headers_are_borked

/** @brief RTP socket */
static int rtpfd;

/** @brief Log output */
static FILE *logfp;

/** @brief Output device */
const char *device;

/** @brief Minimum low watermark
 *
 * We'll stop playing if there's only this many samples in the buffer. */
unsigned minbuffer = 2 * 44100 / 10;  /* 0.2 seconds */

/** @brief Buffer high watermark
 *
 * We'll only start playing when this many samples are available. */
static unsigned readahead = 2 * 2 * 44100;

/** @brief Maximum buffer size
 *
 * We'll stop reading from the network if we have this many samples. */
static unsigned maxbuffer;

/** @brief Received packets
 * Protected by @ref receive_lock
 *
 * Received packets are added to this list, and queue_thread() picks them off
 * it and adds them to @ref packets.  Whenever a packet is added to it, @ref
 * receive_cond is signalled.
 */
struct packet *received_packets;

/** @brief Tail of @ref received_packets
 * Protected by @ref receive_lock
 */
struct packet **received_tail = &received_packets;

/** @brief Lock protecting @ref received_packets 
 *
 * Only listen_thread() and queue_thread() ever hold this lock.  It is vital
 * that queue_thread() not hold it any longer than it strictly has to. */
pthread_mutex_t receive_lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Condition variable signalled when @ref received_packets is updated
 *
 * Used by listen_thread() to notify queue_thread() that it has added another
 * packet to @ref received_packets. */
pthread_cond_t receive_cond = PTHREAD_COND_INITIALIZER;

/** @brief Length of @ref received_packets */
uint32_t nreceived;

/** @brief Binary heap of received packets */
struct pheap packets;

/** @brief Total number of samples available
 *
 * We make this volatile because we inspect it without a protecting lock,
 * so the usual pthread_* guarantees aren't available.
 */
volatile uint32_t nsamples;

/** @brief Timestamp of next packet to play.
 *
 * This is set to the timestamp of the last packet, plus the number of
 * samples it contained.  Only valid if @ref active is nonzero.
 */
uint32_t next_timestamp;

/** @brief True if actively playing
 *
 * This is true when playing and false when just buffering. */
int active;

/** @brief Lock protecting @ref packets */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Condition variable signalled whenever @ref packets is changed */
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

#if API_ALSA
# define DEFAULT_BACKEND playrtp_alsa
#elif HAVE_SYS_SOUNDCARD_H
# define DEFAULT_BACKEND playrtp_oss
#elif HAVE_COREAUDIO_AUDIOHARDWARE_H
# define DEFAULT_BACKEND playrtp_coreaudio
#else
# error No known backend
#endif

/** @brief Backend to play with */
static void (*backend)(void) = &DEFAULT_BACKEND;

HEAP_DEFINE(pheap, struct packet *, lt_packet);

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "debug", no_argument, 0, 'd' },
  { "device", required_argument, 0, 'D' },
  { "min", required_argument, 0, 'm' },
  { "max", required_argument, 0, 'x' },
  { "buffer", required_argument, 0, 'b' },
  { "rcvbuf", required_argument, 0, 'R' },
  { "multicast", required_argument, 0, 'M' },
#if HAVE_SYS_SOUNDCARD_H
  { "oss", no_argument, 0, 'o' },
#endif
#if API_ALSA
  { "alsa", no_argument, 0, 'a' },
#endif
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
  { "core-audio", no_argument, 0, 'c' },
#endif
  { 0, 0, 0, 0 }
};

/** @brief Drop the first packet
 *
 * Assumes that @ref lock is held. 
 */
static void drop_first_packet(void) {
  if(pheap_count(&packets)) {
    struct packet *const p = pheap_remove(&packets);
    nsamples -= p->nsamples;
    playrtp_free_packet(p);
    pthread_cond_broadcast(&cond);
  }
}

/** @brief Background thread adding packets to heap
 *
 * This just transfers packets from @ref received_packets to @ref packets.  It
 * is important that it holds @ref receive_lock for as little time as possible,
 * in order to minimize the interval between calls to read() in
 * listen_thread().
 */
static void *queue_thread(void attribute((unused)) *arg) {
  struct packet *p;

  for(;;) {
    /* Get the next packet */
    pthread_mutex_lock(&receive_lock);
    while(!received_packets)
      pthread_cond_wait(&receive_cond, &receive_lock);
    p = received_packets;
    received_packets = p->next;
    if(!received_packets)
      received_tail = &received_packets;
    --nreceived;
    pthread_mutex_unlock(&receive_lock);
    /* Add it to the heap */
    pthread_mutex_lock(&lock);
    pheap_insert(&packets, p);
    nsamples += p->nsamples;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
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
 * packets around; see @ref playrtp_new_packet().
 */
static void *listen_thread(void attribute((unused)) *arg) {
  struct packet *p = 0;
  int n;
  struct rtp_header header;
  uint16_t seq;
  uint32_t timestamp;
  struct iovec iov[2];

  for(;;) {
    if(!p)
      p = playrtp_new_packet();
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
    p->next = 0;
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
      pthread_mutex_lock(&lock);
      while(nsamples >= maxbuffer)
        pthread_cond_wait(&cond, &lock);
      pthread_mutex_unlock(&lock);
    }
    /* Add the packet to the receive queue */
    pthread_mutex_lock(&receive_lock);
    *received_tail = p;
    received_tail = &p->next;
    ++nreceived;
    pthread_cond_signal(&receive_cond);
    pthread_mutex_unlock(&receive_lock);
    /* We'll need a new packet */
    p = 0;
  }
}

/** @brief Wait until the buffer is adequately full
 *
 * Must be called with @ref lock held.
 */
void playrtp_fill_buffer(void) {
  while(nsamples)
    drop_first_packet();
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
struct packet *playrtp_next_packet(void) {
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
  /* We have a second thread to add received packets to the queue */
  pthread_create(&ltid, 0, queue_thread, 0);
  /* The rest of the work is backend-specific */
  backend();
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
          "  --rcvbuf, -R BYTES      Socket receive buffer size\n"
          "  --multicast, -M GROUP   Join multicast group\n"
#if API_ALSA
          "  --alsa, -a              Use ALSA to play audio\n"
#endif
#if HAVE_SYS_SOUNDCARD_H
          "  --oss, -o               Use OSS to play audio\n"
#endif
#if HAVE_COREAUDIO_AUDIOHARDWARE_H
          "  --core-audio, -c        Use Core Audio to play audio\n"
#endif
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
  int rcvbuf, target_rcvbuf = 131072;
  socklen_t len;
  char *multicast_group = 0;
  struct ip_mreq mreq;
  struct ipv6_mreq mreq6;

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
  while((n = getopt_long(argc, argv, "hVdD:m:b:x:L:R:M:aoc", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    case 'D': device = optarg; break;
    case 'm': minbuffer = 2 * atol(optarg); break;
    case 'b': readahead = 2 * atol(optarg); break;
    case 'x': maxbuffer = 2 * atol(optarg); break;
    case 'L': logfp = fopen(optarg, "w"); break;
    case 'R': target_rcvbuf = atoi(optarg); break;
    case 'M': multicast_group = optarg; break;
#if API_ALSA
    case 'a': backend = playrtp_alsa; break;
#endif
#if 0
#if HAVE_SYS_SOUNDCARD_H      
    case 'o': backend = playrtp_oss; break;
#endif
#if HAVE_COREAUDIO_AUDIOHARDWARE_H      
    case 'c': backend = playrtp_coreaudio; break;
#endif
#endif
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
  if(multicast_group) {
    if((n = getaddrinfo(multicast_group, 0, &prefs, &res)))
      fatal(0, "getaddrinfo %s: %s", multicast_group, gai_strerror(n));
    switch(res->ai_family) {
    case PF_INET:
      mreq.imr_multiaddr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
      mreq.imr_interface.s_addr = 0;      /* use primary interface */
      if(setsockopt(rtpfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    &mreq, sizeof mreq) < 0)
        fatal(errno, "error calling setsockopt IP_ADD_MEMBERSHIP");
      break;
    case PF_INET6:
      mreq6.ipv6mr_multiaddr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
      memset(&mreq6.ipv6mr_interface, 0, sizeof mreq6.ipv6mr_interface);
      if(setsockopt(rtpfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                    &mreq6, sizeof mreq6) < 0)
        fatal(errno, "error calling setsockopt IPV6_JOIN_GROUP");
      break;
    default:
      fatal(0, "unsupported address family %d", res->ai_family);
    }
  }
  len = sizeof rcvbuf;
  if(getsockopt(rtpfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &len) < 0)
    fatal(errno, "error calling getsockopt SO_RCVBUF");
  if(target_rcvbuf > rcvbuf) {
    if(setsockopt(rtpfd, SOL_SOCKET, SO_RCVBUF,
                  &target_rcvbuf, sizeof target_rcvbuf) < 0)
      error(errno, "error calling setsockopt SO_RCVBUF %d", 
            target_rcvbuf);
      /* We try to carry on anyway */
    else
      info("changed socket receive buffer from %d to %d",
           rcvbuf, target_rcvbuf);
  } else
    info("default socket receive buffer %d", rcvbuf);
  if(logfp)
    info("WARNING: -L option can impact performance");
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
