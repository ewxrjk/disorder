/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef KVP_H
#define KVP_H

struct dynstr;
struct sink;

struct kvp {
  struct kvp *next;			/* next entry */
  const char *name;			/* name */
  const char *value;			/* value */
};

struct kvp *kvp_urldecode(const char *ptr, size_t n);
/* url-decode [ptr,ptr+n) */

char *kvp_urlencode(const struct kvp *kvp, size_t *np);
/* url-encode @kvp@ into a null-terminated string.  If @np@ is not
 * null return the length thru it. */

int kvp_set(struct kvp **kvpp, const char *name, const char *value);
/* set @name@ to @value@.  If @value@ is 0, remove @name@.
 * Returns 1 if we made a real change, else 0. */

const char *kvp_get(const struct kvp *kvp, const char *name);
/* Get the value of @name@ */

int urldecode(struct sink *sink, const char *ptr, size_t n);
/* url-decode the @n@ bytes at @ptr@, writing the results to @s@.
 * Return 0 on success, -1 on error. */

int urlencode(struct sink *sink, const char *s, size_t n);
/* url-encode the @n@ bytes at @s@, writing to @sink@.  Return 0 on
 * success, -1 on error.  */

const char *urlencodestring(const char *s);
/* return the url-encoded form of @s@ */

#endif /* KVP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
