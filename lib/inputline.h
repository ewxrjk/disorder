/*
 * This file is part of DisOrder.
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
/** @file lib/inputline.h
 * @brief Line input
 */

#ifndef INPUTLINE_H
#define INPUTLINE_H

struct source;

int inputlines(const char *tag, struct source *s, char **lp, int newline);
int inputline(const char *tag, FILE *fp, char **lp, int newline);
/* read characters from @fp@ until @newline@ is encountered.  Store
 * them (excluding @newline@) via @lp@.  Return 0 on success, -1 on
 * error/eof. */

/** @brief Magic @p newline value to make inputline() insist on CRLF */
#define CRLF 0x100

#endif /* INPUTLINE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
