/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2005, 2007, 2008, 2013 Richard Kettlewell
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
/** @file lib/table.h
 * @brief Generic binary search
 */
#ifndef TABLE_H
#define TABLE_H

#include <stddef.h>

#if _WIN32
#define OFFSETOF_IN_OBJECT(OBJECT,FIELD) ((char *)&(OBJECT).FIELD - (char *)&(OBJECT))
#else
#define OFFSETOF_IN_OBJECT(OBJECT,FIELD) offsetof(typeof(OBJECT),FIELD)
#endif

#define TABLE_FIND(TABLE, FIELD, NAME)			\
  table_find((void *)TABLE,				\
             OFFSETOF_IN_OBJECT(TABLE[0], FIELD),       \
	     sizeof ((TABLE)[0]),			\
	     sizeof TABLE / sizeof ((TABLE)[0]),	\
	     NAME)
/* Search TABLE[] for an element where TABLE[N].FIELD matches NAME
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
