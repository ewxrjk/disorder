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
/** @file server/options.ch
 * @brief CGI options
 */

#ifndef OPTIONS_H
#define OPTIONS_H

void option_set(const char *name, const char *value);
const char *option_label(const char *key);
int option_label_exists(const char *key);
char **option_columns(const char *name, int *ncolumns);

#endif /* OPTIONS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
