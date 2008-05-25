/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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

static void test_syscalls(void) {
  int p[2];
  char buf[128], *e;
  long n;
  long long nn;

  xpipe(p);
  nonblock(p[1]);
  memset(buf, 99, sizeof buf);
  errno = 0;
  while(write(p[1], buf, sizeof buf) > 0)
    errno = 0;
  insist(errno == EAGAIN);
  memset(buf, 0, sizeof buf);
  insist(read(p[0], buf, sizeof buf) == sizeof buf);
  insist(buf[0] == 99);
  insist(buf[(sizeof buf) - 1] == 99);

  xclose(p[0]);
  xclose(p[1]);
  errno = 0;
  insist(read(p[0], buf, sizeof buf) < 0);
  insist(errno == EBADF);
  errno = 0;
  insist(write(p[1], buf, sizeof buf) < 0);
  insist(errno == EBADF);

  n = 0;
  e = 0;
  sprintf(buf, "%ld", LONG_MAX);
  insist(xstrtol(&n, buf, &e, 0) == 0);
  insist(n == LONG_MAX);
  insist(e == buf + strlen(buf));

  n = 0;
  e = 0;
  sprintf(buf, "%ld0", LONG_MAX);
  insist(xstrtol(&n, buf, &e, 0) == ERANGE);
  insist(n == LONG_MAX);
  insist(e == buf + strlen(buf));

  n = 0;
  e = 0;
  sprintf(buf, "%ldxyzzy", LONG_MAX);
  insist(xstrtol(&n, buf, &e, 0) == 0);
  insist(n == LONG_MAX);
  insist(e != 0);
  check_string(e, "xyzzy");

#ifdef LLONG_MAX
  /* Debian's gcc 2.95 cannot easily be persuaded to define LLONG_MAX even in
   * extensions modes.  If your compiler is this broken you just don't get the
   * full set of tests.  Deal. */
  nn = 0;
  e = 0;
  sprintf(buf, "%lld", LLONG_MAX);
  insist(xstrtoll(&nn, buf, &e, 0) == 0);
  insist(nn == LLONG_MAX);
  insist(e == buf + strlen(buf));

  nn = 0;
  e = 0;
  sprintf(buf, "%lld0", LLONG_MAX);
  insist(xstrtoll(&nn, buf, &e, 0) == ERANGE);
  insist(nn == LLONG_MAX);
  insist(e == buf + strlen(buf));

  nn = 0;
  e = 0;
  sprintf(buf, "%lldxyzzy", LLONG_MAX);
  insist(xstrtoll(&nn, buf, &e, 0) == 0);
  insist(nn == LLONG_MAX);
  insist(e != 0);
  check_string(e, "xyzzy");
#endif
}

TEST(syscalls);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
