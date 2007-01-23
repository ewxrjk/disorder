/*
 * This file is part of DisOrder
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

#ifndef MIXER_H
#define MIXER_H

int mixer_channel(const char *c);
/* convert @c@ to a channel number, or return -1 if it does not match */

int mixer_control(int *left, int *right, int set);
/* get/set the current level.  If @set@ is true then the level is set
 * according to the pointers.  In either case the eventual level is
 * returned via the pointers.
 *
 * Returns 0 on success and -1 on error.
 */

#endif /* MIXER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:d70a8b1bc1e79efcad02d20259246454 */
