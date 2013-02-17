/*
 * This file is part of DisOrder.
 * Copyright (C) 2007, 2008 Richard Kettlewell
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
/** @file server/exports.c
 * @brief Server exports
 *
 * Force functions to be included in the server so they can be exported
 * to plugins.
 */
#include "disorder.h"

typedef void (*fptr)(void);

const fptr exported[] = {
  (fptr)disorder_malloc,
  (fptr)disorder_realloc,
  (fptr)disorder_malloc_noptr,
  (fptr)disorder_realloc_noptr,
  (fptr)disorder_strdup,
  (fptr)disorder_strndup,
  (fptr)disorder_asprintf,
  (fptr)disorder_snprintf,
  (fptr)disorder_error,
  (fptr)disorder_fatal,
  (fptr)disorder_info,
  (fptr)disorder_track_exists,
  (fptr)disorder_track_get_data,
  (fptr)disorder_track_set_data,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
