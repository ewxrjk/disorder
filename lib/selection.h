/*
 * This file is part of DisOrder
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file lib/selection.h
 * @brief Select management for Disobedience
 */

#ifndef SELECTION_H
#define SELECTION_H

/* Represent a selection using a hash */

hash *selection_new(void);

void selection_set(hash *h, const char *key, int selected);
/* Set the selection status of KEY */

void selection_flip(hash *h, const char *key);
/* Flip the selection status of KEY */

int selection_selected(hash *h, const char *key);
/* Test whether KEY is selected.  Always returns 0 or 1. */

void selection_live(hash *h, const char *key);
/* Mark KEY as live.  Ignored if KEY is not selected. */

void selection_cleanup(hash *h);
/* Discard dead items (and mark everything left as dead) */

void selection_empty(hash *h);
/* Empty the selection */

#endif /* SELECTION_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
