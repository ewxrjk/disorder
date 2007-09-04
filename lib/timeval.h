/*
 * This file is part of DisOrder.
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

#ifndef TIMEVAL_H
#define TIMEVAL_H

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

#endif /* TIMEVAL_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
