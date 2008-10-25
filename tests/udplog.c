/*
 * This file is part of DisOrder
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
/** @file tests/udplog.c
 * @brief UDP logging utility
 *
 * Intended for low-level debugging.
 */
#include "common.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "addr.h"
#include "defs.h"
#include "mem.h"

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "output", required_argument, 0, 'o' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-udplog [OPTIONS] ADDRESS PORT\n"
	  "Options:\n"
	  "  --output, -o PATH       Output to PATH (default: stdout)\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
          "\n"
          "UDP packet receiver.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("%s", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n, fd, err, i, j;
  struct addrinfo *ai;
  struct stringlist a;
  char *name, h[4096], s[4096];
  uint8_t buffer[4096];
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } sa;
  socklen_t len;
  fd_set fds;
  struct timeval tv;
  static const struct addrinfo pref = {
    .ai_flags = 0,
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_DGRAM,
    .ai_protocol = IPPROTO_UDP,
  };
  
  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVo:", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'o':
      if(!freopen(optarg, "w", stdout))
	fatal(errno, "%s", optarg);
      break;
    default: fatal(0, "invalid option");
    }
  }
  if(optind + 2 != argc)
    fatal(0, "missing arguments");
  a.n = 2;
  a.s = &argv[optind];
  if(!(ai = get_address(&a, &pref, &name)))
    exit(1);
  fd = xsocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  nonblock(fd);
  if(bind(fd, ai->ai_addr, ai->ai_addrlen) < 0)
    fatal(errno, "error binding to %s", name);
  while(getppid() != 1) {
    /* Wait for something to happen.  We don't just block forever in recvfrom()
     * as otherwise we'd never die if the parent terminated uncontrolledly. */
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    select(fd + 1, &fds, 0, 0, &tv);
    len = sizeof sa;
    n = recvfrom(fd, buffer, sizeof buffer, 0, &sa.sa, &len);
    if(n < 0) {
      if(errno == EINTR || errno == EAGAIN)
	continue;
      fatal(errno, "%s: recvfrom", name);
    }
    if((err = getnameinfo(&sa.sa, len, h, sizeof h, s, sizeof s,
			  NI_NUMERICHOST|NI_NUMERICSERV|NI_DGRAM)))
      fatal(0, "getnameinfo: %s", gai_strerror(err));
    xprintf("from host %s service %s: %d bytes\n", h, s, n);
    for(i = 0; i < n; i += 16) {
      for(j = i; j < n && j < i + 16; ++j)
	xprintf(" %02x", buffer[j]);
      for(; j < i + 16; ++j)
	xprintf("   ");
      xprintf("  ");
      for(j = i; j < n && j < i + 16; ++j)
	xprintf("%c", buffer[j] < 128 && isprint(buffer[j]) ? buffer[j] : '.');
      xprintf("\n");
      if(fflush(stdout) < 0)
	fatal(errno, "stdout");
    }
  }
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
