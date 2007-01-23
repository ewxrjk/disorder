/*
 * This file is part of DisOrder.
 * Copyright (C) 2005 Richard Kettlewell
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

#ifndef BASEN_H
#define BASEN_H

int basen(unsigned long *v,
	  int nwords,
	  char buffer[],
	  size_t bufsize,
	  unsigned base);
/* convert the big-endian value at @v@ composed of @nwords@ 32-bit words
 * into a base-@base@ string and store in @buffer@.
 * Returns 0 on success or -1 on overflow.
 */

#endif /* BASEN_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
