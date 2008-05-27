/*
 * This file is part of DisOrder.
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
#include "test.h"
#include <time.h>
#include "dateparse.h"

static void check_date(time_t when,
		       const char *fmt,
		       struct tm *(*convert)(const time_t *)) {
  char buffer[128];
  time_t parsed;

  strftime(buffer, sizeof buffer, fmt, convert(&when));
  parsed = dateparse(buffer);
  check_integer(parsed, when);
  if(parsed != when)
    fprintf(stderr, "format=%s formatted=%s\n", fmt, buffer);
}

static void test_dateparse(void) {
  time_t now = time(0);
  check_date(now, "%Y-%m-%d %H:%M:%S", localtime);
#if 0	       /* see dateparse.c */
  check_date(now, "%Y-%m-%d %H:%M:%S %Z", localtime);
  check_date(now, "%Y-%m-%d %H:%M:%S %Z", gmtime);
#endif
  check_date(now, "%c", localtime);
  check_date(now, "%Ec", localtime);
  check_date(now, "%X", localtime);
  check_date(now, "%EX", localtime);
  check_date(now, "%H:%M:%S", localtime);
  /* This one needs a bodge: */
  check_date(now - now % 60, "%H:%M", localtime);
  /* Reject invalid formats */
  check_fatal(dateparse("12"));
  check_fatal(dateparse("12:34:56:23"));
  /* Reject invalid values */
  check_fatal(dateparse("25:34"));
  check_fatal(dateparse("23:61"));
  check_fatal(dateparse("23:23:62"));
}

TEST(dateparse);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
