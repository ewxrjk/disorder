/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/cgi.h
 * @brief CGI tools
 */

#ifndef CGI_H
#define CGI_H

struct sink;

void cgi_init(void);
const char *cgi_get(const char *name);
void cgi_set(const char *name, const char *value);
char *cgi_sgmlquote(const char *src);
void cgi_attr(struct sink *output, const char *name, const char *value);
void cgi_opentag(struct sink *output, const char *name, ...);
void cgi_closetag(struct sink *output, const char *name);
char *cgi_makeurl(const char *url, ...);
char *cgi_thisurl(const char *url);
void cgi_clear(void);

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
