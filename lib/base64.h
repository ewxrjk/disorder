/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/base64.h
 * @brief Support for MIME base64
 */

#ifndef BASE64_H
#define BASE64_H

char *mime_base64(const char *s, size_t *nsp);
char *mime_to_base64(const uint8_t *s, size_t ns);

#endif /* BASE64_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/