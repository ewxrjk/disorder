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
/** @file lib/rate.h
 * @brief Rate tracking
 *
 * Track the rate at which some repeating event occurs.
 */
#ifndef RATE_H
#define RATE_H

/** @brief Rate tracking context */
struct rate {
  /** @brief Start of interval */
  struct timespec start;

  /** @brief Events so far in this interval */
  long long count;
};

/** @brief Initialize a rate tracking context
 * @param r Pointer to rate tracking context
 */
void rate_init(struct rate *r);

/** @brief Update a rate tracking context
 * @param r Pointer to rate tracking context
 * @param delta Number of events that have just happened
 * @return -ve, or events per second
 *
 * Call this function each time an event has occurred, with the number
 * of times it has occurred since last time.
 *
 * A return value of -1 means nothing to report.
 *
 * A return value of 0 or more means the event average that many
 * events per second recently.
 */
long long rate_update(struct rate *r, long long delta);

#endif /* RATE_H */
