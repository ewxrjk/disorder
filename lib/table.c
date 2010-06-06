/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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
/** @file lib/table.c
 * @brief Generic binary search
 */
#include "common.h"

#include "table.h"

int table_find(const void *table, size_t offset, size_t eltsize, size_t nelts,
	       const char *name) {
  int l = 0, r = (int)nelts - 1, c, m;
  const char *k;

  while(l <= r) {
    k = *(const char **)((char *)table + offset + eltsize * (m = (l + r) / 2));
    if(!(c = strcmp(name, k)))
      return m;
    if(c < 0)
      r = m - 1;
    else
      l = m + 1;
  }
  return -1;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
