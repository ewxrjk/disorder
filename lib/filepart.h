/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/filepart.h
 * @brief Filename parsing
 */

#ifndef FILEPART_H
#define FILEPART_H

char *d_basename(const char *path);

char *d_dirname(const char *path);
/* return the directory name part of @path@ */

const char *strip_extension(const char *path);
/* return a filename with the extension stripped */

const char *extension(const char *path);
/* return just the extension, or "" */

#endif /* FILEPART_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
