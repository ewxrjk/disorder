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
/** @file lib/uaudio-rtp.c
 * @brief Support for RTP network play backend */
#include "common.h"

#include <errno.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <gcrypt.h>
#include <unistd.h>
#include <time.h>
#include <sys/uio.h>

#include "uaudio.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"
#include "rtp.h"
#include "addr.h"
#include "ifreq.h"
#include "timeval.h"

/** @brief Bytes to send per network packet
 *
 * This is the maximum number of bytes we pass to write(2); to determine actual
 * packet sizes, add a UDP header and an IP header (and a link layer header if
 * it's the link layer size you care about).
 *
 * Don't make this too big or arithmetic will start to overflow.
 */
#define NETWORK_BYTES (1500-8/*UDP*/-40/*IP*/-8/*conservatism*/)

/** @brief RTP payload type */
static int rtp_payload;

/** @brief RTP output socket */
static int rtp_fd;

/** @brief RTP SSRC */
static uint32_t rtp_id;

/** @brief RTP sequence number */
static uint16_t rtp_sequence;

/** @brief Network error count
 *
 * If too many errors occur in too short a time, we give up.
 */
static int rtp_errors;

/** @brief Delay threshold in microseconds
 *
 * rtp_play() never attempts to introduce a delay shorter than this.
 */
static int64_t rtp_delay_threshold;

static const char *const rtp_options[] = {
  "rtp-destination",
  "rtp-destination-port",
  "rtp-source",
  "rtp-source-port",
  "multicast-ttl",
  "multicast-loop",
  "delay-threshold",
  NULL
};

static size_t rtp_play(void *buffer, size_t nsamples) {
  struct rtp_header header;
  struct iovec vec[2];
  
  /* We do as much work as possible before checking what time it is */
  /* Fill out header */
  header.vpxcc = 2 << 6;              /* V=2, P=0, X=0, CC=0 */
  header.seq = htons(rtp_sequence++);
  header.ssrc = rtp_id;
  header.mpt = (uaudio_schedule_reactivated ? 0x80 : 0x00) | rtp_payload;
#if !WORDS_BIGENDIAN
  /* Convert samples to network byte order */
  uint16_t *u = buffer, *const limit = u + nsamples;
  while(u < limit) {
    *u = htons(*u);
    ++u;
  }
#endif
  vec[0].iov_base = (void *)&header;
  vec[0].iov_len = sizeof header;
  vec[1].iov_base = buffer;
  vec[1].iov_len = nsamples * uaudio_sample_size;
  uaudio_schedule_synchronize();
  header.timestamp = htonl((uint32_t)uaudio_schedule_timestamp);
  int written_bytes;
  do {
    written_bytes = writev(rtp_fd, vec, 2);
  } while(written_bytes < 0 && errno == EINTR);
  if(written_bytes < 0) {
    error(errno, "error transmitting audio data");
    ++rtp_errors;
    if(rtp_errors == 10)
      fatal(0, "too many audio tranmission errors");
    return 0;
  } else
    rtp_errors /= 2;                    /* gradual decay */
  written_bytes -= sizeof (struct rtp_header);
  const size_t written_samples = written_bytes / uaudio_sample_size;
  uaudio_schedule_update(written_samples);
  return written_samples;
}

static void rtp_open(void) {
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
  struct stringlist dst, src;
  
  /* Get configuration */
  dst.n = 2;
  dst.s = xcalloc(2, sizeof *dst.s);
  dst.s[0] = uaudio_get("rtp-destination", NULL);
  dst.s[1] = uaudio_get("rtp-destination-port", NULL);
  src.n = 2;
  src.s = xcalloc(2, sizeof *dst.s);
  src.s[0] = uaudio_get("rtp-source", NULL);
  src.s[1] = uaudio_get("rtp-source-port", NULL);
  if(!dst.s[0])
    fatal(0, "'rtp-destination' not set");
  if(!dst.s[1])
    fatal(0, "'rtp-destination-port' not set");
  if(src.s[0]) {
    if(!src.s[1])
      fatal(0, "'rtp-source-port' not set");
    src.n = 2;
  } else
    src.n = 0;
  rtp_delay_threshold = atoi(uaudio_get("rtp-delay-threshold", "1000"));
  /* ...microseconds */

  /* Resolve addresses */
  res = get_address(&dst, &pref, &sockname);
  if(!res) exit(-1);
  if(src.n) {
    sres = get_address(&src, &prefbind, &ssockname);
    if(!sres) exit(-1);
  } else
    sres = 0;
  /* Create the socket */
  if((rtp_fd = socket(res->ai_family,
                      res->ai_socktype,
                      res->ai_protocol)) < 0)
    fatal(errno, "error creating broadcast socket");
  if(multicast(res->ai_addr)) {
    /* Enable multicast options */
    const int ttl = atoi(uaudio_get("multicast-ttl", "1"));
    const int loop = !strcmp(uaudio_get("multicast-loop", "yes"), "yes");
    switch(res->ai_family) {
    case PF_INET: {
      if(setsockopt(rtp_fd, IPPROTO_IP, IP_MULTICAST_TTL,
                    &ttl, sizeof ttl) < 0)
        fatal(errno, "error setting IP_MULTICAST_TTL on multicast socket");
      if(setsockopt(rtp_fd, IPPROTO_IP, IP_MULTICAST_LOOP,
                    &loop, sizeof loop) < 0)
        fatal(errno, "error setting IP_MULTICAST_LOOP on multicast socket");
      break;
    }
    case PF_INET6: {
      if(setsockopt(rtp_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                    &ttl, sizeof ttl) < 0)
        fatal(errno, "error setting IPV6_MULTICAST_HOPS on multicast socket");
      if(setsockopt(rtp_fd, IPPROTO_IP, IPV6_MULTICAST_LOOP,
                    &loop, sizeof loop) < 0)
        fatal(errno, "error setting IPV6_MULTICAST_LOOP on multicast socket");
      break;
    }
    default:
      fatal(0, "unsupported address family %d", res->ai_family);
    }
    info("multicasting on %s TTL=%d loop=%s", 
         sockname, ttl, loop ? "yes" : "no");
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
      if(setsockopt(rtp_fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one) < 0)
        fatal(errno, "error setting SO_BROADCAST on broadcast socket");
      info("broadcasting on %s (%s)", sockname, ifs->ifa_name);
    } else
      info("unicasting on %s", sockname);
  }
  /* Enlarge the socket buffer */
  len = sizeof sndbuf;
  if(getsockopt(rtp_fd, SOL_SOCKET, SO_SNDBUF,
                &sndbuf, &len) < 0)
    fatal(errno, "error getting SO_SNDBUF");
  if(target_sndbuf > sndbuf) {
    if(setsockopt(rtp_fd, SOL_SOCKET, SO_SNDBUF,
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
  if(sres && bind(rtp_fd, sres->ai_addr, sres->ai_addrlen) < 0)
    fatal(errno, "error binding broadcast socket to %s", ssockname);
  if(connect(rtp_fd, res->ai_addr, res->ai_addrlen) < 0)
    fatal(errno, "error connecting broadcast socket to %s", sockname);
}

static void rtp_start(uaudio_callback *callback,
                      void *userdata) {
  /* We only support L16 (but we do stereo and mono and will convert sign) */
  if(uaudio_channels == 2
     && uaudio_bits == 16
     && uaudio_rate == 44100)
    rtp_payload = 10;
  else if(uaudio_channels == 1
     && uaudio_bits == 16
     && uaudio_rate == 44100)
    rtp_payload = 11;
  else
    fatal(0, "asked for %d/%d/%d 16/44100/1 and 16/44100/2",
          uaudio_bits, uaudio_rate, uaudio_channels); 
  /* Various fields are required to have random initial values by RFC3550.  The
   * packet contents are highly public so there's no point asking for very
   * strong randomness. */
  gcry_create_nonce(&rtp_id, sizeof rtp_id);
  gcry_create_nonce(&rtp_sequence, sizeof rtp_sequence);
  rtp_open();
  uaudio_schedule_init();
  uaudio_thread_start(callback,
                      userdata,
                      rtp_play,
                      256 / uaudio_sample_size,
                      (NETWORK_BYTES - sizeof(struct rtp_header))
                      / uaudio_sample_size);
}

static void rtp_stop(void) {
  uaudio_thread_stop();
  close(rtp_fd);
  rtp_fd = -1;
}

static void rtp_activate(void) {
  uaudio_schedule_reactivated = 1;
  uaudio_thread_activate();
}

static void rtp_deactivate(void) {
  uaudio_thread_deactivate();
}

const struct uaudio uaudio_rtp = {
  .name = "rtp",
  .options = rtp_options,
  .start = rtp_start,
  .stop = rtp_stop,
  .activate = rtp_activate,
  .deactivate = rtp_deactivate
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
