/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2008 Richard Kettlewell
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
/** @file server/speaker-network.c
 * @brief Support for @ref BACKEND_NETWORK */

#include "common.h"

#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <gcrypt.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include <netinet/in.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "addr.h"
#include "timeval.h"
#include "rtp.h"
#include "ifreq.h"
#include "speaker-protocol.h"
#include "speaker.h"

/** @brief Network socket
 *
 * This is the file descriptor to write to for @ref BACKEND_NETWORK.
 */
static int bfd = -1;

/** @brief RTP timestamp
 *
 * This counts the number of samples played (NB not the number of frames
 * played).
 *
 * The timestamp in the packet header is only 32 bits wide.  With 44100Hz
 * stereo, that only gives about half a day before wrapping, which is not
 * particularly convenient for certain debugging purposes.  Therefore the
 * timestamp is maintained as a 64-bit integer, giving around six million years
 * before wrapping, and truncated to 32 bits when transmitting.
 */
static uint64_t rtp_time;

/** @brief RTP base timestamp
 *
 * This is the real time correspoding to an @ref rtp_time of 0.  It is used
 * to recalculate the timestamp after idle periods.
 */
static struct timeval rtp_time_0;

/** @brief RTP packet sequence number */
static uint16_t rtp_seq;

/** @brief RTP SSRC */
static uint32_t rtp_id;

/** @brief Error counter */
static int audio_errors;

/** @brief Network backend initialization */
static void network_init(void) {
  struct addrinfo *res, *sres;
  static const struct addrinfo pref = {
    .ai_flags = 0,
    .ai_family = PF_INET,
    .ai_socktype = SOCK_DGRAM,
    .ai_protocol = IPPROTO_UDP,
  };
  static const struct addrinfo prefbind = {
    .ai_flags = AI_PASSIVE,
    .ai_family = PF_INET,
    .ai_socktype = SOCK_DGRAM,
    .ai_protocol = IPPROTO_UDP,
  };
  static const int one = 1;
  int sndbuf, target_sndbuf = 131072;
  socklen_t len;
  char *sockname, *ssockname;

  res = get_address(&config->broadcast, &pref, &sockname);
  if(!res) exit(-1);
  if(config->broadcast_from.n) {
    sres = get_address(&config->broadcast_from, &prefbind, &ssockname);
    if(!sres) exit(-1);
  } else
    sres = 0;
  if((bfd = socket(res->ai_family,
                   res->ai_socktype,
                   res->ai_protocol)) < 0)
    fatal(errno, "error creating broadcast socket");
  if(multicast(res->ai_addr)) {
    /* Multicasting */
    switch(res->ai_family) {
    case PF_INET: {
      const int mttl = config->multicast_ttl;
      if(setsockopt(bfd, IPPROTO_IP, IP_MULTICAST_TTL, &mttl, sizeof mttl) < 0)
        fatal(errno, "error setting IP_MULTICAST_TTL on multicast socket");
      if(setsockopt(bfd, IPPROTO_IP, IP_MULTICAST_LOOP,
                    &config->multicast_loop, sizeof one) < 0)
        fatal(errno, "error setting IP_MULTICAST_LOOP on multicast socket");
      break;
    }
    case PF_INET6: {
      const int mttl = config->multicast_ttl;
      if(setsockopt(bfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                    &mttl, sizeof mttl) < 0)
        fatal(errno, "error setting IPV6_MULTICAST_HOPS on multicast socket");
      if(setsockopt(bfd, IPPROTO_IP, IPV6_MULTICAST_LOOP,
                    &config->multicast_loop, sizeof (int)) < 0)
        fatal(errno, "error setting IPV6_MULTICAST_LOOP on multicast socket");
      break;
    }
    default:
      fatal(0, "unsupported address family %d", res->ai_family);
    }
    info("multicasting on %s", sockname);
  } else {
    struct ifaddrs *ifs;

    if(getifaddrs(&ifs) < 0)
      fatal(errno, "error calling getifaddrs");
    while(ifs) {
      /* (At least on Darwin) IFF_BROADCAST might be set but ifa_broadaddr
       * still a null pointer.  It turns out that there's a subsequent entry
       * for he same interface which _does_ have ifa_broadaddr though... */
      if((ifs->ifa_flags & IFF_BROADCAST)
         && ifs->ifa_broadaddr
         && sockaddr_equal(ifs->ifa_broadaddr, res->ai_addr))
        break;
      ifs = ifs->ifa_next;
    }
    if(ifs) {
      if(setsockopt(bfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one) < 0)
        fatal(errno, "error setting SO_BROADCAST on broadcast socket");
      info("broadcasting on %s (%s)", sockname, ifs->ifa_name);
    } else
      info("unicasting on %s", sockname);
  }
  len = sizeof sndbuf;
  if(getsockopt(bfd, SOL_SOCKET, SO_SNDBUF,
                &sndbuf, &len) < 0)
    fatal(errno, "error getting SO_SNDBUF");
  if(target_sndbuf > sndbuf) {
    if(setsockopt(bfd, SOL_SOCKET, SO_SNDBUF,
                  &target_sndbuf, sizeof target_sndbuf) < 0)
      error(errno, "error setting SO_SNDBUF to %d", target_sndbuf);
    else
      info("changed socket send buffer size from %d to %d",
           sndbuf, target_sndbuf);
  } else
    info("default socket send buffer is %d",
         sndbuf);
  /* We might well want to set additional broadcast- or multicast-related
   * options here */
  if(sres && bind(bfd, sres->ai_addr, sres->ai_addrlen) < 0)
    fatal(errno, "error binding broadcast socket to %s", ssockname);
  if(connect(bfd, res->ai_addr, res->ai_addrlen) < 0)
    fatal(errno, "error connecting broadcast socket to %s", sockname);
  /* Select an SSRC */
  gcry_randomize(&rtp_id, sizeof rtp_id, GCRY_STRONG_RANDOM);
}

/** @brief Play over the network */
static size_t network_play(size_t frames) {
  struct rtp_header header;
  struct iovec vec[2];
  size_t bytes = frames * bpf, written_frames;
  int written_bytes;
  /* We transmit using RTP (RFC3550) and attempt to conform to the internet
   * AVT profile (RFC3551). */

  /* If we're starting then initialize the base time */
  if(!rtp_time)
    xgettimeofday(&rtp_time_0, 0);
  if(idled) {
    /* There may have been a gap.  Fix up the RTP time accordingly. */
    struct timeval now;
    uint64_t delta;
    uint64_t target_rtp_time;

    /* Find the current time */
    xgettimeofday(&now, 0);
    /* Find the number of microseconds elapsed since rtp_time=0 */
    delta = tvsub_us(now, rtp_time_0);
    if(delta > UINT64_MAX / 88200)
      fatal(0, "rtp_time=%"PRIu64" now=%ld.%06ld rtp_time_0=%ld.%06ld delta=%"PRIu64" (%"PRId64")",
            rtp_time,
            (long)now.tv_sec, (long)now.tv_usec,
            (long)rtp_time_0.tv_sec, (long)rtp_time_0.tv_usec,
            delta, delta);
    target_rtp_time = (delta * config->sample_format.rate
                       * config->sample_format.channels) / 1000000;
    /* Overflows at ~6 years uptime with 44100Hz stereo */

    /* rtp_time is the number of samples we've played.  NB that we play
     * RTP_AHEAD_MS ahead of ourselves, so it may legitimately be ahead of
     * the value we deduce from time comparison.
     *
     * Suppose we have 1s track started at t=0, and another track begins to
     * play at t=2s.  Suppose 44100Hz stereo.  We send 1s of audio over the
     * next (about) one second, giving rtp_time=88200.  rtp_time stops at this
     * point.
     *
     * At t=2s we'll have calculated target_rtp_time=176400.  In this case we
     * set rtp_time=176400 and the player can correctly conclude that it
     * should leave 1s between the tracks.
     *
     * It's never right to reduce rtp_time, for that would imply packets with
     * overlapping timestamp ranges, which does not make sense.
     */
    target_rtp_time &= ~(uint64_t)1;    /* stereo! */
    if(target_rtp_time > rtp_time) {
      /* More time has elapsed than we've transmitted samples.  That implies
       * we've been 'sending' silence.  */
      info("advancing rtp_time by %"PRIu64" samples",
           target_rtp_time - rtp_time);
      rtp_time = target_rtp_time;
    } else if(target_rtp_time < rtp_time) {
      info("would reverse rtp_time by %"PRIu64" samples",
           rtp_time - target_rtp_time);
    }
  }
  header.vpxcc = 2 << 6;              /* V=2, P=0, X=0, CC=0 */
  header.seq = htons(rtp_seq++);
  header.timestamp = htonl((uint32_t)rtp_time);
  header.ssrc = rtp_id;
  header.mpt = (idled ? 0x80 : 0x00) | 10;
  /* 10 = L16 = 16-bit x 2 x 44100KHz.  We ought to deduce this value from
   * the sample rate (in a library somewhere so that configuration.c can rule
   * out invalid rates).
   */
  idled = 0;
  if(bytes > NETWORK_BYTES - sizeof header) {
    bytes = NETWORK_BYTES - sizeof header;
    /* Always send a whole number of frames */
    bytes -= bytes % bpf;
  }
  /* "The RTP clock rate used for generating the RTP timestamp is independent
   * of the number of channels and the encoding; it equals the number of
   * sampling periods per second.  For N-channel encodings, each sampling
   * period (say, 1/8000 of a second) generates N samples. (This terminology
   * is standard, but somewhat confusing, as the total number of samples
   * generated per second is then the sampling rate times the channel
   * count.)"
   */
  vec[0].iov_base = (void *)&header;
  vec[0].iov_len = sizeof header;
  vec[1].iov_base = playing->buffer + playing->start;
  vec[1].iov_len = bytes;
  do {
    written_bytes = writev(bfd, vec, 2);
  } while(written_bytes < 0 && errno == EINTR);
  if(written_bytes < 0) {
    error(errno, "error transmitting audio data");
    ++audio_errors;
    if(audio_errors == 10)
      fatal(0, "too many audio errors");
    return 0;
  } else
    audio_errors /= 2;
  written_bytes -= sizeof (struct rtp_header);
  written_frames = written_bytes / bpf;
  /* Advance RTP's notion of the time */
  rtp_time += written_frames * config->sample_format.channels;
  return written_frames;
}

static int bfd_slot;

/** @brief Set up poll array for network play */
static void network_beforepoll(int *timeoutp) {
  struct timeval now;
  uint64_t target_us;
  uint64_t target_rtp_time;
  const int64_t samples_per_second = config->sample_format.rate
                                   * config->sample_format.channels;
  int64_t lead, ahead_ms;
  
  /* If we're starting then initialize the base time */
  if(!rtp_time)
    xgettimeofday(&rtp_time_0, 0);
  /* We send audio data whenever we would otherwise get behind */
  xgettimeofday(&now, 0);
  target_us = tvsub_us(now, rtp_time_0);
  if(target_us > UINT64_MAX / 88200)
    fatal(0, "rtp_time=%"PRIu64" rtp_time_0=%ld.%06ld now=%ld.%06ld target_us=%"PRIu64" (%"PRId64")\n",
          rtp_time,
          (long)rtp_time_0.tv_sec, (long)rtp_time_0.tv_usec,
          (long)now.tv_sec, (long)now.tv_usec,
          target_us, target_us);
  target_rtp_time = (target_us * config->sample_format.rate
                               * config->sample_format.channels)
                     / 1000000;
  /* Lead is how far ahead we are */
  lead = rtp_time - target_rtp_time;
  if(lead <= 0)
    /* We're behind or even, so we'll need to write as soon as we can */
    bfd_slot = addfd(bfd, POLLOUT);
  else {
    /* We've ahead, we can afford to wait a bit even if the IP stack thinks it
     * can accept more. */
    ahead_ms = 1000 * lead / samples_per_second;
    if(ahead_ms < *timeoutp)
      *timeoutp = ahead_ms;
  }
}

/** @brief Process poll() results for network play */
static int network_ready(void) {
  if(fds[bfd_slot].revents & (POLLOUT | POLLERR))
    return 1;
  else
    return 0;
}

const struct speaker_backend network_backend = {
  BACKEND_NETWORK,
  0,
  network_init,
  0,                                    /* activate */
  network_play,
  0,                                    /* deactivate */
  network_beforepoll,
  network_ready
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
