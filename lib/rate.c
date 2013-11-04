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
/** @file lib/rate.c
 * @brief Rate tracking
 *
 */
#include "common.h"
#include <time.h>
#include "rate.h"
#include "syscalls.h"

void rate_init(struct rate *r) {
  xgettime(CLOCK_MONOTONIC, &r->start);
  r->count = 0;
}

long long rate_update(struct rate *r, long long delta) {
  struct timespec now;
  xgettime(CLOCK_MONOTONIC, &now);
  if(now.tv_sec == r->start.tv_sec) {
    r->count += delta;
    return -1;
  } else {
    long long ret = r->count;
    if(r->start.tv_nsec > 0)
      ret = ret / (1000000000.0 / (1000000000.0 - r->start.tv_nsec));
    r->start.tv_sec = now.tv_sec;
    r->start.tv_nsec = 0;
    r->count = delta;
    return ret;
  }
}
