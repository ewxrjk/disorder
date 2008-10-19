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

/** @file lib/dateparse.c
 * @brief Date parsing
 */

#include "common.h"

#include <time.h>

#include "dateparse.h"
#include "log.h"

/** @brief Date parsing patterns
 *
 * This set of patterns is designed to parse a specific time of a specific day,
 * since that's what the scheduler needs.  Other requirements might need other
 * pattern lists.
 */
static const char *const datemsk[] = {
  /* ISO format */
  "%Y-%m-%d %H:%M:%S",
  /* Generic time, same day */
  "%H:%M:%S",
  "%H:%M",
  /* "%Y-%m-%d %H:%M:%S %Z" - no, not sensibly supported anywhere */
  /* Locale-specific date + time */
  "%c",
  "%Ec",
  /* Locale-specific time, same day */
  "%X",
  "%EX",
  NULL,
};

/** @brief Convert string to a @c time_t */
time_t dateparse(const char *s) {
  struct tm t;
  int rc;

  switch(rc = xgetdate_r(s, &t, datemsk)) {
  case 0:
    return mktime(&t);
  case 7:
    fatal(0, "date string '%s' not in a recognized format", s);
  case 8:
    fatal(0, "date string '%s' not representable", s);
  default:
    fatal(0, "date string '%s' produced unexpected error %d", s, rc);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
