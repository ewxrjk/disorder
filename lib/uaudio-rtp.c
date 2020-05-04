/*
 * This file is part of DisOrder.
 * Copyright (C) 2009, 2013 Richard Kettlewell
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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <gcrypt.h>
#include <unistd.h>
#include <time.h>
#include <sys/uio.h>
#include <pthread.h>

#include "uaudio.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"
#include "rtp.h"
#include "addr.h"
#include "ifreq.h"
#include "timeval.h"
#include "configuration.h"

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

/** @brief RTP broadcast/multicast output socket */
static int rtp_fd = -1;

/** @brief RTP unicast output socket (IPv4) */
static int rtp_fd4 = -1;

/** @brief RTP unicast output socket (IPv6) */
static int rtp_fd6 = -1;

/** @brief RTP SSRC */
static uint32_t rtp_id;

/** @brief Base for timestamp */
static uint32_t rtp_base;

/** @brief RTP sequence number */
static uint16_t rtp_sequence;

/** @brief Network error count
 *
 * If too many errors occur in too short a time, we give up.
 */
static int rtp_errors;

/** @brief RTP mode */
static int rtp_mode;

#define RTP_BROADCAST 1
#define RTP_MULTICAST 2
#define RTP_UNICAST 3
#define RTP_REQUEST 4
#define RTP_AUTO 5

/** @brief A unicast client */
struct rtp_recipient {
  struct rtp_recipient *next;
  struct sockaddr_storage sa;
};

/** @brief List of unicast clients */
static struct rtp_recipient *rtp_recipient_list;

/** @brief Mutex protecting data structures */
static pthread_mutex_t rtp_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *const rtp_options[] = {
  "rtp-destination",
  "rtp-destination-port",
  "rtp-source",
  "rtp-source-port",
  "multicast-ttl",
  "multicast-loop",
  "rtp-mode",
  NULL
};

static void rtp_get_netconfig(const char *af,
                              const char *addr,
                              const char *port,
                              struct netaddress *na) {
  char *vec[3];
  
  vec[0] = uaudio_get(af, NULL);
  vec[1] = uaudio_get(addr, NULL);
  vec[2] = uaudio_get(port, NULL);
  if(!*vec)
    na->af = -1;
  else
    if(netaddress_parse(na, 3, vec))
      disorder_fatal(0, "invalid RTP address");
}

static void rtp_set_netconfig(const char *af,
                              const char *addr,
                              const char *port,
                              const struct netaddress *na) {
  uaudio_set(af, NULL);
  uaudio_set(addr, NULL);
  uaudio_set(port, NULL);
  if(na->af != -1) {
    int nvec;
    char **vec;

    netaddress_format(na, &nvec, &vec);
    if(nvec > 0) {
      uaudio_set(af, vec[0]);
      xfree(vec[0]);
    }
    if(nvec > 1) {
      uaudio_set(addr, vec[1]);
      xfree(vec[1]);
    }
    if(nvec > 2) {
      uaudio_set(port, vec[2]);
      xfree(vec[2]);
    }
    xfree(vec);
  }
}

static size_t rtp_play(void *buffer, size_t nsamples, unsigned flags) {
  struct rtp_header header;
  struct iovec vec[2];

#if 0
  if(flags & (UAUDIO_PAUSE|UAUDIO_RESUME))
    fprintf(stderr, "rtp_play %zu samples%s%s%s%s\n", nsamples,
            flags & UAUDIO_PAUSE ? " UAUDIO_PAUSE" : "",
            flags & UAUDIO_RESUME ? " UAUDIO_RESUME" : "",
            flags & UAUDIO_PLAYING ? " UAUDIO_PLAYING" : "",
            flags & UAUDIO_PAUSED ? " UAUDIO_PAUSED" : "");
#endif
          
  /* We do as much work as possible before checking what time it is */
  /* Fill out header */
  header.vpxcc = 2 << 6;              /* V=2, P=0, X=0, CC=0 */
  header.seq = htons(rtp_sequence++);
  header.ssrc = rtp_id;
  header.mpt = rtp_payload;
  /* If we've come out of a pause, set the marker bit */
  if(flags & UAUDIO_RESUME)
    header.mpt |= 0x80;
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
  const uint32_t timestamp = uaudio_schedule_sync();
  header.timestamp = htonl(rtp_base + (uint32_t)timestamp);

  /* We send ~120 packets a second with current arrangements.  So if we log
   * once every 8192 packets we log about once a minute. */

  if(!(ntohs(header.seq) & 8191)
     && config->rtp_verbose)
    disorder_info("RTP: seq %04"PRIx16" %08"PRIx32"+%08"PRIx32"=%08"PRIx32" ns %zu%s",
                  ntohs(header.seq),
                  rtp_base,
                  timestamp,
                  header.timestamp,
                  nsamples,
                  flags & UAUDIO_PAUSED ? " [paused]" : "");

  /* If we're paused don't actually end a packet, we just pretend */
  if(flags & UAUDIO_PAUSED) {
    uaudio_schedule_sent(nsamples);
    return nsamples;
  }
  if(rtp_mode == RTP_REQUEST) {
    struct rtp_recipient *r;
    struct msghdr m;
    memset(&m, 0, sizeof m);
    m.msg_iov = vec;
    m.msg_iovlen = 2;
    pthread_mutex_lock(&rtp_lock);
    for(r = rtp_recipient_list; r; r = r->next) {
      m.msg_name = &r->sa;
      m.msg_namelen = r->sa.ss_family == AF_INET ? 
        sizeof(struct sockaddr_in) : sizeof (struct sockaddr_in6);
      sendmsg(r->sa.ss_family == AF_INET ? rtp_fd4 : rtp_fd6,
              &m, MSG_DONTWAIT|MSG_NOSIGNAL);
      // TODO similar error handling to other case?
    }
    pthread_mutex_unlock(&rtp_lock);
  } else {
    int written_bytes;
    do {
      written_bytes = writev(rtp_fd, vec, 2);
    } while(written_bytes < 0 && errno == EINTR);
    if(written_bytes < 0) {
      disorder_error(errno, "error transmitting audio data");
      ++rtp_errors;
      if(rtp_errors == 10)
        disorder_fatal(0, "too many audio transmission errors");
      return 0;
    } else
      rtp_errors /= 2;                    /* gradual decay */
  }
  /* TODO what can we sensibly do about short writes here?  Really that's just
   * an error and we ought to be using smaller packets. */
  uaudio_schedule_sent(nsamples);
  return nsamples;
}

static void hack_send_buffer_size(int fd, const char *what) {
  int sndbuf, target_sndbuf = 131072;
  socklen_t len = sizeof sndbuf;

  if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                &sndbuf, &len) < 0)
    disorder_fatal(errno, "error getting SO_SNDBUF on %s socket", what);
  if(target_sndbuf > sndbuf) {
    if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                  &target_sndbuf, sizeof target_sndbuf) < 0)
      disorder_error(errno, "error setting SO_SNDBUF on %s socket to %d",
                     what, target_sndbuf);
    else
      disorder_info("changed socket send buffer size on %socket from %d to %d",
                    what, sndbuf, target_sndbuf);
  } else
    disorder_info("default socket send buffer on %s socket is %d",
                  what, sndbuf);
}

static void rtp_open(void) {
  struct addrinfo *dres, *sres;
  static const int one = 1;
  struct netaddress dst[1], src[1];
  const char *mode;
  
  /* Get the mode */
  mode = uaudio_get("rtp-mode", "auto");
  if(!strcmp(mode, "broadcast")) rtp_mode = RTP_BROADCAST;
  else if(!strcmp(mode, "multicast")) rtp_mode = RTP_MULTICAST;
  else if(!strcmp(mode, "unicast")) rtp_mode = RTP_UNICAST;
  else if(!strcmp(mode, "request")) rtp_mode = RTP_REQUEST;
  else rtp_mode = RTP_AUTO;
  /* Get the source and destination addresses (which might be missing) */
  rtp_get_netconfig("rtp-destination-af",
                    "rtp-destination",
                    "rtp-destination-port",
                    dst);
  rtp_get_netconfig("rtp-source-af",
                    "rtp-source",
                    "rtp-source-port",
                    src);
  if(dst->af != -1) {
    dres = netaddress_resolve(dst, 0, IPPROTO_UDP);
    if(!dres)
      exit(-1);
  } else
    dres = 0;
  if(src->af != -1) {
    sres = netaddress_resolve(src, 1, IPPROTO_UDP);
    if(!sres)
      exit(-1);
  } else
    sres = 0;
  /* _AUTO inspects the destination address and acts accordingly */
  if(rtp_mode == RTP_AUTO) {
    if(!dres)
      rtp_mode = RTP_REQUEST;
    else if(multicast(dres->ai_addr))
      rtp_mode = RTP_MULTICAST;
    else {
      struct ifaddrs *ifs;

      if(getifaddrs(&ifs) < 0)
        disorder_fatal(errno, "error calling getifaddrs");
      while(ifs) {
        /* (At least on Darwin) IFF_BROADCAST might be set but ifa_broadaddr
         * still a null pointer.  It turns out that there's a subsequent entry
         * for he same interface which _does_ have ifa_broadaddr though... */
        if((ifs->ifa_flags & IFF_BROADCAST)
           && ifs->ifa_broadaddr
           && sockaddr_equal(ifs->ifa_broadaddr, dres->ai_addr))
          break;
        ifs = ifs->ifa_next;
      }
      if(ifs) 
        rtp_mode = RTP_BROADCAST;
      else
        rtp_mode = RTP_UNICAST;
    }
  }
  /* Create the sockets */
  if(rtp_mode != RTP_REQUEST) {
    if((rtp_fd = socket(dres->ai_family,
                        dres->ai_socktype,
                        dres->ai_protocol)) < 0)
      disorder_fatal(errno, "error creating RTP transmission socket");
  }
  if((rtp_fd4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    disorder_fatal(errno, "error creating v4 RTP transmission socket");
  if((rtp_fd6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    disorder_fatal(errno, "error creating v6 RTP transmission socket");
  /* Configure the socket according to the desired mode */
  switch(rtp_mode) {
  case RTP_MULTICAST: {
    /* Enable multicast options */
    const int ttl = atoi(uaudio_get("multicast-ttl", "1"));
    const int loop = !strcmp(uaudio_get("multicast-loop", "yes"), "yes");
    switch(dres->ai_family) {
    case PF_INET: {
      if(setsockopt(rtp_fd, IPPROTO_IP, IP_MULTICAST_TTL,
                    &ttl, sizeof ttl) < 0)
        disorder_fatal(errno, "error setting IP_MULTICAST_TTL on multicast socket");
      if(setsockopt(rtp_fd, IPPROTO_IP, IP_MULTICAST_LOOP,
                    &loop, sizeof loop) < 0)
        disorder_fatal(errno, "error setting IP_MULTICAST_LOOP on multicast socket");
      break;
    }
    case PF_INET6: {
      if(setsockopt(rtp_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                    &ttl, sizeof ttl) < 0)
        disorder_fatal(errno, "error setting IPV6_MULTICAST_HOPS on multicast socket");
      if(setsockopt(rtp_fd, IPPROTO_IP, IPV6_MULTICAST_LOOP,
                    &loop, sizeof loop) < 0)
        disorder_fatal(errno, "error setting IPV6_MULTICAST_LOOP on multicast socket");
      break;
    }
    default:
      disorder_fatal(0, "unsupported address family %d", dres->ai_family);
    }
    disorder_info("multicasting on %s TTL=%d loop=%s", 
                  format_sockaddr(dres->ai_addr), ttl, loop ? "yes" : "no");
    break;
  }
  case RTP_UNICAST: {
    disorder_info("unicasting on %s", format_sockaddr(dres->ai_addr));
    break;
  }
  case RTP_BROADCAST: {
    if(setsockopt(rtp_fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one) < 0)
      disorder_fatal(errno, "error setting SO_BROADCAST on broadcast socket");
    disorder_info("broadcasting on %s", 
                  format_sockaddr(dres->ai_addr));
    break;
  }
  case RTP_REQUEST: {
    disorder_info("will transmit on request");
    break;
  }
  }
  /* Enlarge the socket buffers */
  if (rtp_fd != -1) hack_send_buffer_size(rtp_fd, "master socket");
  hack_send_buffer_size(rtp_fd4, "IPv4 on-demand socket");
  hack_send_buffer_size(rtp_fd6, "IPv6 on-demand socket");
  /* We might well want to set additional broadcast- or multicast-related
   * options here */
  if(rtp_mode != RTP_REQUEST) {
    if(sres && bind(rtp_fd, sres->ai_addr, sres->ai_addrlen) < 0)
      disorder_fatal(errno, "error binding broadcast socket to %s", 
                     format_sockaddr(sres->ai_addr));
    if(connect(rtp_fd, dres->ai_addr, dres->ai_addrlen) < 0)
      disorder_fatal(errno, "error connecting broadcast socket to %s", 
                     format_sockaddr(dres->ai_addr));
  }
  if(config->rtp_verbose)
    disorder_info("RTP: prepared socket");
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
    disorder_fatal(0, "asked for %d/%d/%d 16/44100/1 and 16/44100/2",
                   uaudio_bits, uaudio_rate, uaudio_channels); 
  if(config->rtp_verbose)
    disorder_info("RTP: %d channels %d bits %d Hz payload type %d",
                  uaudio_channels, uaudio_bits, uaudio_rate, rtp_payload);
  /* Various fields are required to have random initial values by RFC3550.  The
   * packet contents are highly public so there's no point asking for very
   * strong randomness. */
  gcry_create_nonce(&rtp_id, sizeof rtp_id);
  gcry_create_nonce(&rtp_base, sizeof rtp_base);
  gcry_create_nonce(&rtp_sequence, sizeof rtp_sequence);
  if(config->rtp_verbose)
    disorder_info("RTP: id %08"PRIx32" base %08"PRIx32" initial seq %08"PRIx16,
                  rtp_id, rtp_base, rtp_sequence);
  rtp_open();
  uaudio_schedule_init();
  if(config->rtp_verbose)
    disorder_info("RTP: initialized schedule");
  uaudio_thread_start(callback,
                      userdata,
                      rtp_play,
                      256 / uaudio_sample_size,
                      (NETWORK_BYTES - sizeof(struct rtp_header))
                      / uaudio_sample_size,
                      0);
  if(config->rtp_verbose)
    disorder_info("RTP: created thread");
}

static void rtp_stop(void) {
  uaudio_thread_stop();
  if(rtp_fd >= 0) { close(rtp_fd); rtp_fd = -1; }
  if(rtp_fd4 >= 0) { close(rtp_fd4); rtp_fd4 = -1; }
  if(rtp_fd6 >= 0) { close(rtp_fd6); rtp_fd6 = -1; }
}

static void rtp_configure(void) {
  char buffer[64];

  uaudio_set("rtp-mode", config->rtp_mode);
  rtp_set_netconfig("rtp-destination-af",
                    "rtp-destination",
                    "rtp-destination-port", &config->broadcast);
  rtp_set_netconfig("rtp-source-af",
                    "rtp-source",
                    "rtp-source-port", &config->broadcast_from);
  snprintf(buffer, sizeof buffer, "%ld", config->multicast_ttl);
  uaudio_set("multicast-ttl", buffer);
  uaudio_set("multicast-loop", config->multicast_loop ? "yes" : "no");
  if(config->rtp_verbose)
    disorder_info("RTP: configured");
}

/** @brief Add an RTP recipient address
 * @param sa Pointer to recipient address
 * @return 0 on success, -1 on error
 */
int rtp_add_recipient(const struct sockaddr_storage *sa) {
  struct rtp_recipient *r;
  int rc;
  pthread_mutex_lock(&rtp_lock);
  for(r = rtp_recipient_list;
      r && sockaddrcmp((struct sockaddr *)sa,
                       (struct sockaddr *)&r->sa);
      r = r->next)
    ;
  if(r)
    rc = -1;
  else {
    r = xmalloc(sizeof *r);
    memcpy(&r->sa, sa, sizeof *sa);
    r->next = rtp_recipient_list;
    rtp_recipient_list = r;
    rc = 0;
  }
  pthread_mutex_unlock(&rtp_lock);
  return rc;
}

/** @brief Remove an RTP recipient address
 * @param sa Pointer to recipient address
 * @return 0 on success, -1 on error
 */
int rtp_remove_recipient(const struct sockaddr_storage *sa) {
  struct rtp_recipient *r, **rr;
  int rc;
  pthread_mutex_lock(&rtp_lock);
  for(rr = &rtp_recipient_list;
      (r = *rr) && sockaddrcmp((struct sockaddr *)sa,
                               (struct sockaddr *)&r->sa);
      rr = &r->next)
    ;
  if(r) {
    *rr = r->next;
    xfree(r);
    rc = 0;
  } else {
    disorder_error(0, "bogus rtp_remove_recipient");
    rc = -1;
  }
  pthread_mutex_unlock(&rtp_lock);
  return rc;
}

const struct uaudio uaudio_rtp = {
  .name = "rtp",
  .options = rtp_options,
  .start = rtp_start,
  .stop = rtp_stop,
  .activate = uaudio_thread_activate,
  .deactivate = uaudio_thread_deactivate,
  .configure = rtp_configure,
  .flags = UAUDIO_API_SERVER,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
