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

int inputline(const char *tag, FILE *fp, char **lp, int newline) {
  struct dynstr d;
  int ch;

  dynstr_init(&d);
  while((ch = getc(fp)),
	(!ferror(fp) && !feof(fp) && ch != newline))
    dynstr_append(&d, ch);
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
/* arch-tag:2df12a23b06659ceea8a6019550a65b4 */
