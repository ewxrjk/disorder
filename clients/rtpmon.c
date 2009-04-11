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
/** @file clients/rtpmon.c
 * @brief RTP monitor
 *
 * This progam monitors the rate at which data arrives by RTP and
 * constantly display it.  It is intended for debugging only.
 *
 * TODO de-dupe with playrtp.
 */
#include "common.h"

#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/uio.h>

#include "syscalls.h"
#include "timeval.h"
#include "mem.h"
#include "log.h"
#include "version.h"
#include "addr.h"
#include "configuration.h"
#include "rtp.h"

/** @brief Record of one packet */
struct entry {
  /** @brief When packet arrived */
  struct timeval when;

  /** @brief Serial number of first sample */
  uint32_t serial;
};

/** @brief Bytes per frame */
static unsigned bpf = 4;

/** @brief Frame serial number */
static uint32_t serial;

/** @brief Size of ring buffer */
#define RINGSIZE 131072

/** @brief Ring buffer */
static struct entry ring[RINGSIZE];

/** @brief Where new packets join the ring */
static unsigned ringtail;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "bpf", required_argument, 0, 'b' },
  { 0, 0, 0, 0 }
};

static void help(void) {
  xprintf("Usage:\n"
	  "  rtpmon [OPTIONS] [ADDRESS] PORT\n"
	  "Options:\n"
          "  --bpf, -b               Bytes/frame (default 4)\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
          );
  xfclose(stdout);
  exit(0);
}

/** @brief Compute the rate by sampling at two points in the ring buffer */
static double rate(unsigned earlier, unsigned later) {
  const uint32_t frames = ring[later].serial - ring[earlier].serial;
  const int64_t us = tvsub_us(ring[later].when, ring[earlier].when);

  if(us)  
    return 1000000.0 * frames / us;
  else
    return 0.0;
}

/** @brief Called to say we received some bytes
 * @param when When we received them
 * @param n How many frames of audio data we received
 */
static void frames(const struct timeval *when, size_t n) {
  const time_t prev = ring[(ringtail - 1) % RINGSIZE].when.tv_sec;

  ring[ringtail].when = *when;
  ring[ringtail].serial = serial;
  serial += n;
  ringtail = (ringtail + 1) % RINGSIZE;
  // Report once a second
  if(prev != when->tv_sec) {
    if(printf("%8.2f  %8.2f  %8.2f  %8.2f  %8.2f  %8.2f  %8.2f\n",
              rate((ringtail - RINGSIZE / 128) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 64) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 32) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 16) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 8) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 4) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE),
              rate((ringtail - RINGSIZE / 2) % RINGSIZE,
                   (ringtail - 1) % RINGSIZE)) < 0
       || fflush(stdout) < 0)
      fatal(errno, "stdout");
  }
}

int main(int argc, char **argv) {
  int n;
  struct addrinfo *res;
  struct stringlist sl;
  struct ip_mreq mreq;
  struct ipv6_mreq mreq6;
  int rtpfd;
  char *sockname;
  int is_multicast;
  union any_sockaddr {
    struct sockaddr sa;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
  };
  union any_sockaddr mgroup;

  static const struct addrinfo prefs = {
    .ai_flags = AI_PASSIVE,
    .ai_family = PF_INET,
    .ai_socktype = SOCK_DGRAM,
    .ai_protocol = IPPROTO_UDP
  };
  static const int one = 1;

  mem_init();
  if(!setlocale(LC_CTYPE, "")) 
    fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVb:", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("rtpmon");
    case 'b': bpf = atoi(optarg); break;
    default: fatal(0, "invalid option");
    }
  }
  argc -= optind;
  argv += optind;
  switch(argc) {
  case 1:
  case 2:
    /* Use command-line ADDRESS+PORT or just PORT */
    sl.n = argc;
    sl.s = argv;
    break;
  default:
    fatal(0, "usage: rtpmon [OPTIONS] [ADDRESS] PORT");
  }
  if(!(res = get_address(&sl, &prefs, &sockname)))
    exit(1);
  /* Create the socket */
  if((rtpfd = socket(res->ai_family,
                     res->ai_socktype,
                     res->ai_protocol)) < 0)
    fatal(errno, "error creating socket");
  /* Allow multiple listeners */
  xsetsockopt(rtpfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  is_multicast = multicast(res->ai_addr);
  /* The multicast and unicast/broadcast cases are different enough that they
   * are totally split.  Trying to find commonality between them causes more
   * trouble that it's worth. */
  if(is_multicast) {
    /* Stash the multicast group address */
    memcpy(&mgroup, res->ai_addr, res->ai_addrlen);
    switch(res->ai_addr->sa_family) {
    case AF_INET:
      mgroup.in.sin_port = 0;
      break;
    case AF_INET6:
      mgroup.in6.sin6_port = 0;
      break;
    default:
      fatal(0, "unsupported family %d", (int)res->ai_addr->sa_family);
    }
    /* Bind to to the multicast group address */
    if(bind(rtpfd, res->ai_addr, res->ai_addrlen) < 0)
      fatal(errno, "error binding socket to %s", format_sockaddr(res->ai_addr));
    /* Add multicast group membership */
    switch(mgroup.sa.sa_family) {
    case PF_INET:
      mreq.imr_multiaddr = mgroup.in.sin_addr;
      mreq.imr_interface.s_addr = 0;      /* use primary interface */
      if(setsockopt(rtpfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    &mreq, sizeof mreq) < 0)
        fatal(errno, "error calling setsockopt IP_ADD_MEMBERSHIP");
      break;
    case PF_INET6:
      mreq6.ipv6mr_multiaddr = mgroup.in6.sin6_addr;
      memset(&mreq6.ipv6mr_interface, 0, sizeof mreq6.ipv6mr_interface);
      if(setsockopt(rtpfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                    &mreq6, sizeof mreq6) < 0)
        fatal(errno, "error calling setsockopt IPV6_JOIN_GROUP");
      break;
    default:
      fatal(0, "unsupported address family %d", res->ai_family);
    }
    /* Report what we did */
    info("listening on %s multicast group %s",
         format_sockaddr(res->ai_addr), format_sockaddr(&mgroup.sa));
  } else {
    /* Bind to 0/port */
    switch(res->ai_addr->sa_family) {
    case AF_INET: {
      struct sockaddr_in *in = (struct sockaddr_in *)res->ai_addr;
      
      memset(&in->sin_addr, 0, sizeof (struct in_addr));
      if(bind(rtpfd, res->ai_addr, res->ai_addrlen) < 0)
        fatal(errno, "error binding socket to 0.0.0.0 port %d",
              ntohs(in->sin_port));
      break;
    }
    case AF_INET6: {
      struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)res->ai_addr;
      
      memset(&in6->sin6_addr, 0, sizeof (struct in6_addr));
      break;
    }
    default:
      fatal(0, "unsupported family %d", (int)res->ai_addr->sa_family);
    }
    if(bind(rtpfd, res->ai_addr, res->ai_addrlen) < 0)
      fatal(errno, "error binding socket to %s", format_sockaddr(res->ai_addr));
    /* Report what we did */
    info("listening on %s", format_sockaddr(res->ai_addr));
  }
  for(;;) {
    struct rtp_header header;
    char buffer[4096];
    struct iovec iov[2];
    struct timeval when;

    iov[0].iov_base = &header;
    iov[0].iov_len = sizeof header;
    iov[1].iov_base = buffer;
    iov[1].iov_len = sizeof buffer;
    n = readv(rtpfd, iov, 2);
    gettimeofday(&when, 0);
    if(n < 0) {
      switch(errno) {
      case EINTR:
        continue;
      default:
        fatal(errno, "error reading from socket");
      }
    }
    if((size_t)n <= sizeof (struct rtp_header)) {
      info("ignored a short packet");
      continue;
    }
    frames(&when, (n - sizeof header) / bpf);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
