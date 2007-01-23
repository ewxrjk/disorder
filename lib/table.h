/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef TABLE_H
#define TABLE_H

#define TABLE_FIND(TABLE, TYPE, FIELD, NAME)	\
  table_find((void *)TABLE,		       	\
	     offsetof(TYPE, FIELD),		\
	     sizeof (TYPE),			\
	     sizeof TABLE / sizeof (TYPE),	\
	     NAME)
/* Search TYPE TABLE[] for an element where TABLE[N].FIELD matches NAME
 * Returns the index N on success or -1 if not found
 * The table must be lexically sorted on FIELD
 */

int table_find(const void *table, size_t offset, size_t eltsize, size_t nelts,
	       const char *name);

#endif /* TABLE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
