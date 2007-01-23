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

#ifndef INPUTLINE_H
#define INPUTLINE_H

int inputline(const char *tag, FILE *fp, char **lp, int newline);
/* read characters from @fp@ until @newline@ is encountered.  Store
 * them (excluding @newline@) via @lp@.  Return 0 on success, -1 on
 * error/eof. */

#endif /* INPUTLINE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
