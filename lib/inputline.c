/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007-9, 2013 Richard Kettlewell
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
/** @file lib/inputline.c
 * @brief Line input
 */

#include "common.h"

#include <errno.h>

#include "log.h"
#include "mem.h"
#include "vector.h"
#include "inputline.h"
#include "sink.h"

/** @brief Read a line from @p fp
 * @param tag Used in error messages
 * @param fp Stream to read from
 * @param lp Where to store newly allocated string
 * @param newline Newline character or @ref CRLF
 * @return 0 on success, -1 on error or eof.
 *
 * The newline is not included in the string.  If the last line of a
 * stream does not have a newline then that line is still returned.
 *
 * If @p newline is @ref CRLF then the line is terminated by CR LF,
 * not by a single newline character.  The CRLF is still not included
 * in the string in this case.
 *
 * @p *lp is only set if the return value was 0.
 */
int inputline(const char *tag, FILE *fp, char **lp, int newline) {
  struct source *s = source_stdio(fp);
  int rc = inputlines(tag, s, lp, newline);
  xfree(s);
  return rc;
}

int inputlines(const char *tag, struct source *s, char **lp, int newline) {
  struct dynstr d;
  int ch, err;
  char errbuf[1024];

  dynstr_init(&d);
  while((ch = source_getc(s)),
	(!source_err(s) && !source_eof(s) && ch != newline)) {
    dynstr_append(&d, ch);
    if(newline == CRLF && d.nvec >= 2
       && d.vec[d.nvec - 2] == 0x0D && d.vec[d.nvec - 1] == 0x0A) {
      d.nvec -= 2;
      break;
    }
  }
  if((err = source_err(s))) {
    disorder_error(0, "error reading %s: %s", tag,
                   format_error(s->eclass, err, errbuf, sizeof errbuf));
    return -1;
  } else if(source_eof(s)) {
    if(d.nvec != 0)
      disorder_error(0, "error reading %s: unexpected EOF", tag);
    return -1;
  }
  dynstr_terminate(&d);
  *lp = d.vec;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
