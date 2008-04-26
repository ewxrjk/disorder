/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
#include "test.h"

void test_addr(void) {
  struct stringlist a;
  const char *s[3];
  struct addrinfo *ai;
  char *name;
  const struct sockaddr_in *sin4;
  struct sockaddr_in s4;
  struct sockaddr_un su;

  static const struct addrinfo pref = {
    AI_PASSIVE,
    PF_INET,
    SOCK_STREAM,
    0,
    0,
    0,
    0,
    0
  };

  struct sockaddr_in a1 = {
    .sin_port = ntohs(25),
    .sin_addr = { .s_addr = 0}
  };
  struct addrinfo p1 = {
    .ai_family = PF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
    .ai_addrlen = sizeof a1,
    .ai_addr = (struct sockaddr *)&a1,
  };

  struct sockaddr_in a2 = {
    .sin_port = ntohs(119),
    .sin_addr = { .s_addr = htonl(0x7F000001) }
  };
  struct addrinfo p2 = {
    .ai_family = PF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
    .ai_addrlen = sizeof a2,
    .ai_addr = (struct sockaddr *)&a2,
  };

  printf("test_addr\n");

  insist(addrinfocmp(&p1, &p2) < 0);

  a.n = 1;
  a.s = (char **)s;
  s[0] = "smtp";
  ai = get_address(&a, &pref, &name);
  insist(ai != 0);
  check_integer(ai->ai_family, PF_INET);
  check_integer(ai->ai_socktype, SOCK_STREAM);
  check_integer(ai->ai_protocol, IPPROTO_TCP);
  check_integer(ai->ai_addrlen, sizeof(struct sockaddr_in));
  sin4 = (const struct sockaddr_in *)ai->ai_addr;
  check_integer(sin4->sin_family, AF_INET);
  check_integer(sin4->sin_addr.s_addr, 0);
  check_integer(ntohs(sin4->sin_port), 25);
  check_string(name, "host * service smtp");
  insist(addrinfocmp(ai, &p1) == 0);
  insist(addrinfocmp(ai, &p2) < 0);

  a.n = 2;
  s[0] = "localhost";
  s[1] = "nntp";
  ai = get_address(&a, &pref, &name);
  insist(ai != 0);
  check_integer(ai->ai_family, PF_INET);
  check_integer(ai->ai_socktype, SOCK_STREAM);
  check_integer(ai->ai_protocol, IPPROTO_TCP);
  check_integer(ai->ai_addrlen, sizeof(struct sockaddr_in));
  sin4 = (const struct sockaddr_in *)ai->ai_addr;
  check_integer(sin4->sin_family, AF_INET);
  check_integer(ntohl(sin4->sin_addr.s_addr), 0x7F000001);
  check_integer(ntohs(sin4->sin_port), 119);
  check_string(name, "host localhost service nntp");
  insist(addrinfocmp(ai, &p2) == 0);
  insist(addrinfocmp(ai, &p1) > 0);

  a.n = 2;
  s[0] = "no.such.domain.really.i.mean.it.greenend.org.uk";
  s[1] = "nntp";
  insist(get_address(&a, &pref, &name) == 0);

  a.n = 3;
  s[0] = 0;
  s[1] = 0;
  s[2] = 0;
  insist(get_address(&a, &pref, &name) == 0);

  memset(&s4, 0, sizeof s4);
  s4.sin_family = AF_INET;
  s4.sin_addr.s_addr = 0;
  s4.sin_port = 0;
  check_string(format_sockaddr((struct sockaddr *)&s4),
               "0.0.0.0");
  check_integer(multicast((struct sockaddr *)&s4), 0);
  s4.sin_addr.s_addr = htonl(0x7F000001);
  s4.sin_port = htons(1000);
  check_string(format_sockaddr((struct sockaddr *)&s4),
               "127.0.0.1 port 1000");
  check_integer(multicast((struct sockaddr *)&s4), 0);
  s4.sin_addr.s_addr = htonl(0xE0000001);
  check_string(format_sockaddr((struct sockaddr *)&s4),
               "224.0.0.1 port 1000");
  check_integer(multicast((struct sockaddr *)&s4), 1);

  memset(&su, 0, sizeof su);
  su.sun_family = AF_UNIX;
  strcpy(su.sun_path, "/wibble/wobble");
  check_string(format_sockaddr((struct sockaddr *)&su),
               "/wibble/wobble");
  check_integer(multicast((struct sockaddr *)&su), 0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
