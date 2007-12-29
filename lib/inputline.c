/*
 * This file is part of DisOrder.
 * Copyright (C) 2004 Richard Kettlewell
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
/** @file lib/inputline.c
 * @brief Line input
 */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "mem.h"
#include "vector.h"
#include "charset.h"
#include "inputline.h"

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
  struct dynstr d;
  int ch;

  dynstr_init(&d);
  while((ch = getc(fp)),
	(!ferror(fp) && !feof(fp) && ch != newline)) {
    dynstr_append(&d, ch);
    if(newline == CRLF && d.nvec >= 2
       && d.vec[d.nvec - 2] == 0x0D && d.vec[d.nvec - 1] == 0x0A) {
      d.nvec -= 2;
      break;
    }
  }
  if(ferror(fp)) {
    error(errno, "error reading %s", tag);
    return -1;
  } else if(feof(fp)) {
    if(d.nvec != 0)
      error(0, "error reading %s: unexpected EOF", tag);
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
