/*
 * This file is part of DisOrder.
 * Copyright (C) 2007-2009, 2013 Richard Kettlewell
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
/** @file lib/timeval.h
 * @brief Arithmetic on timeval structures
 */
#ifndef TIMEVAL_H
#define TIMEVAL_H

#include <time.h>
#include <sys/time.h>
#include <math.h>

static inline struct timeval tvsub(const struct timeval a,
                                   const struct timeval b) {
  struct timeval r;

  r.tv_sec = a.tv_sec - b.tv_sec;
  r.tv_usec = a.tv_usec - b.tv_usec;
  if(r.tv_usec < 0) {
    r.tv_usec += 1000000;
    r.tv_sec--;
  }
  if(r.tv_usec > 999999) {
    r.tv_usec -= 1000000;
    r.tv_sec++;
  }
  return r;
}

static inline struct timeval tvadd(const struct timeval a,
                                   const struct timeval b) {
  struct timeval r;

  r.tv_sec = a.tv_sec + b.tv_sec;
  r.tv_usec = a.tv_usec + b.tv_usec;
  if(r.tv_usec < 0) {
    r.tv_usec += 1000000;
    r.tv_sec--;
  }
  if(r.tv_usec > 999999) {
    r.tv_usec -= 1000000;
    r.tv_sec++;
  }
  return r;
}

static inline double tvdouble(const struct timeval a) {
  return a.tv_sec + a.tv_usec / 1000000.0;
}

static inline int64_t tvsub_us(const struct timeval a,
                               const struct timeval b) {
  return (((uint64_t)a.tv_sec * 1000000 + a.tv_usec)
          - ((uint64_t)b.tv_sec * 1000000 + b.tv_usec));
}

/** @brief Great-than comparison for timevals */
static inline int tvgt(const struct timeval *a, const struct timeval *b) {
  if(a->tv_sec > b->tv_sec)
    return 1;
  if(a->tv_sec == b->tv_sec
     && a->tv_usec > b->tv_usec)
    return 1;
  return 0;
}

/** @brief Less-than
    comparison for timevals */
static inline int tvlt(const struct timeval *a, const struct timeval *b) {
  if(a->tv_sec < b->tv_sec)
    return 1;
  if(a->tv_sec == b->tv_sec
     && a->tv_usec < b->tv_usec)
    return 1;
  return 0;
}

/** @brief Greater-than-or-equal comparison for timevals */
static inline int tvge(const struct timeval *a, const struct timeval *b) {
  return !tvlt(a, b);
}

/** @brief Less-than-or-equal comparison for timevals */
static inline int tvle(const struct timeval *a, const struct timeval *b) {
  return !tvgt(a, b);
}

/** @brief Return the sum of two timespecs */
static inline struct timespec tsadd(const struct timespec a,
                                    const struct timespec b) {
  struct timespec r;

  r.tv_sec = a.tv_sec + b.tv_sec;
  r.tv_nsec = a.tv_nsec + b.tv_nsec;
  if(r.tv_nsec < 0) {
    r.tv_nsec += 1000000;
    r.tv_sec--;
  }
  if(r.tv_nsec > 999999) {
    r.tv_nsec -= 1000000;
    r.tv_sec++;
  }
  return r;
}

/** @brief Subtract one timespec from another */
static inline struct timespec tssub(const struct timespec a,
                                    const struct timespec b) {
  struct timespec r;

  r.tv_sec = a.tv_sec - b.tv_sec;
  r.tv_nsec = a.tv_nsec - b.tv_nsec;
  if(r.tv_nsec < 0) {
    r.tv_nsec += 1000000;
    r.tv_sec--;
  }
  if(r.tv_nsec > 999999) {
    r.tv_nsec -= 1000000;
    r.tv_sec++;
  }
  return r;
}

/** @brief Convert a timespec to a double */
static inline double ts_to_double(const struct timespec ts) {
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

/** @brief Convert a double to a timespec */
static inline struct timespec double_to_ts(double n) {
  double i, f;
  struct timespec r;
  f = modf(n, &i);
  r.tv_sec = i;
  r.tv_nsec = 1000000000 * f;
  return r;
}

#endif /* TIMEVAL_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
