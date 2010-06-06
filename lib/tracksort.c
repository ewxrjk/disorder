/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file lib/tracksort.c
 * @brief Track ordering
 */
#include "common.h"

#include "trackname.h"
#include "mem.h"

/** @brief Compare two @ref entry objects */
static int tracksort_compare(const void *a, const void *b) {
  const struct tracksort_data *ea = a, *eb = b;

  return compare_tracks(ea->sort, eb->sort,
			ea->display, eb->display,
			ea->track, eb->track);
}

struct tracksort_data *tracksort_init(int ntracks,
                                      char **tracks,
                                      const char *type) {
  struct tracksort_data *td = xcalloc(ntracks, sizeof *td);
  for(int n = 0; n < ntracks; ++n) {
    td[n].track = tracks[n];
    td[n].sort = trackname_transform(type, tracks[n], "sort");
    td[n].display = trackname_transform(type, tracks[n], "display");
  }
  qsort(td, ntracks, sizeof *td, tracksort_compare);
  return td;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
