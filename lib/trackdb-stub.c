/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file lib/trackdb-stub.c
 * @brief Track database stubs
 *
 * Stubs for non-server builds
 */

#include <config.h>
#include "types.h"

#include <pcre.h>

#include "rights.h"
#include "trackdb.h"

const char *trackdb_get_password(const char attribute((unused)) *user)  {
  return NULL;
}

void trackdb_close(void) {
}

void trackdb_open(int attribute((unused)) flags) {
}

void trackdb_init(int attribute((unused)) flags) {
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
