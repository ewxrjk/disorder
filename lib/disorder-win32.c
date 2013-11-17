/*
 * This file is part of DisOrder.
 * Copyright (C) 2013 Richard Kettlewell
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
/** @file lib/disorder-win32.c
 * @brief Windows support
 */
#include "common.h"
#include "mem.h"
#include "log.h"
#include "vector.h"

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  FILETIME ft;
  unsigned long long cns;
  GetSystemTimeAsFileTime(&ft);
  cns = ((unsigned long long)ft.dwHighDateTime << 32) + ft.dwLowDateTime;
  /* This gives the count of 100ns intervals since the start of 1601.
   * WP thinks that this is interpreted according to the proleptic Gregorian
   * calendar though MS do not say.
   */
  tv->tv_usec = (cns % 10000000) / 10;
  tv->tv_sec = (long)(cns / 10000000 - 86400LL * ((1970 - 1601) * 365
                                                  + (1970 - 1601) / 4
                                                  - (1970 - 1601) / 100));
  return 0;
}

char *win_wtomb(const wchar_t *ws) {
  char *s;
  size_t converted;
  int rc;
  if((rc = wcstombs_s(&converted, NULL, 0, ws, 0)))
    disorder_fatal(rc, "wcstombs_s");
  s  = xmalloc(converted);
  if((rc = wcstombs_s(&converted, s, converted, ws, converted)))
    disorder_fatal(rc, "wcstombs_s");
  return s;
}

void network_init(void) {
  WSADATA wsadata;
  int rc;
  if((rc = WSAStartup(MAKEWORD(2, 2), &wsadata)))
    disorder_fatal(0, "WSAStartup: %d", rc);
}