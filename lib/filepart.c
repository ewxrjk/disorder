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
/** @file lib/filepart.c
 * @brief Filename parsing
 */

#include <config.h>
#include "types.h"

#include <string.h>

#include "filepart.h"
#include "mem.h"

/** @brief Return the directory part of @p path
 * @param path Path to parse
 * @return Directory part of @p path
 *
 * Extracts the directory part of @p path.  This is a simple lexical
 * transformation and no canonicalization is performed.  The result will only
 * ever end "/" if it is the root directory.
 */
char *d_dirname(const char *path) {
  const char *s;

  if((s = strrchr(path, '/'))) {
    while(s > path && s[-1] == '/')
      --s;
    if(s == path)
      return xstrdup("/");
    else
      return xstrndup(path, s - path);
  } else
    return xstrdup(".");
}

/** @brief Find the extension part of @p path
 * @param path Path to parse
 * @return Start of extension in @p path, or NULL
 *
 * The return value points into @p path and points at the "." at the start of
 * the path.  If the basename has no extension the result is NULL.  Extensions
 * are assumed to only contain the ASCII digits and letters.
 *
 * See also extension().
 */
static const char *find_extension(const char *path) {
  const char *q = path + strlen(path);
  
  while(q > path && strchr("abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"0123456789", *q))
    --q;
  return *q == '.' ? q : 0;
}

/** @brief Strip the extension from @p path
 * @param path Path to parse
 * @return @p path with extension removed, or @p path
 *
 * The extension is defined exactly as for find_extension().  The result might
 * or might not point into @p path.
 */
const char *strip_extension(const char *path) {
  const char *q = find_extension(path);

  return q ? xstrndup(path, q - path) : path;
}

/** @brief Find the extension part of @p path
 * @param path Path to parse
 * @return Start of extension in @p path, or ""
 *
 * The return value may points into @p path and if so points at the "." at the
 * start of the path.  If the basename has no extension the result is "".
 * Extensions are assumed to only contain the ASCII digits and letters.
 *
 * See also find_extension().
 */
const char *extension(const char *path) {
  const char *q = find_extension(path);

  return q ? q : "";
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
