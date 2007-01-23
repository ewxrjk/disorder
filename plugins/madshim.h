/*
 * This file is part of DisOrder.
 * Copyright (C) 2004 Richard Kettlewell
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

#ifndef MADSHIM_H
#define MADSHIM_H

/* shim to integrate code from mpg123 */

typedef struct {
  int num_frames;
  mad_timer_t duration;
} buffer;

void scan_mp3(void const *ptr, ssize_t len, buffer *buf);

#endif /* MADSHIM_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
