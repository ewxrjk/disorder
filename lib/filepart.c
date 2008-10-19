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
/** @file lib/filepart.c
 * @brief Filename parsing
 */

#include "common.h"

#include "filepart.h"
#include "mem.h"

/** @brief Parse a filename
 * @param path Filename to parse
 * @param Where to put directory name, or NULL
 * @param Where to put basename, or NULL
 */
static void parse_filename(const char *path,
                           char **dirnamep,
                           char **basenamep) {
  const char *s, *e = path + strlen(path);

  /* Strip trailing slashes.  We never take these into account. */
  while(e > path && e[-1] == '/')
    --e;
  if(e == path) {
    /* The path is empty or contains only slashes */
    if(*path) {
      if(dirnamep)
        *dirnamep = xstrdup("/");
      if(basenamep)
        *basenamep = xstrdup("/");
    } else {
      if(dirnamep)
        *dirnamep = xstrdup("");
      if(basenamep)
        *basenamep = xstrdup("");
    }
  } else {
    /* The path isn't empty and has more than just slashes.  e therefore now
     * points at the end of the basename. */
    s = e;
    while(s > path && s[-1] != '/')
      --s;
    /* Now s points at the start of the basename */
    if(basenamep)
      *basenamep = xstrndup(s, e - s);
    if(s > path) {
      --s;
      /* s must now be pointing at a '/' before the basename */
      assert(*s == '/');
      while(s > path && s[-1] == '/')
        --s;
      /* Now s must be pointing at the last '/' after the dirname */
      assert(*s == '/');
      if(s == path) {
        /* If we reached the start we must be at the root */
        if(dirnamep)
          *dirnamep = xstrdup("/");
      } else {
        /* There's more than just the root here */
        if(dirnamep)
          *dirnamep = xstrndup(path, s - path);
      }
    } else {
      /* There wasn't a slash */
      if(dirnamep)
        *dirnamep = xstrdup(".");
    }
  }
}

/** @brief Return the directory part of @p path
 * @param path Path to parse
 * @return Directory part of @p path
 *
 * Extracts the directory part of @p path.  This is a simple lexical
 * transformation and no canonicalization is performed.  The result will only
 * ever end "/" if it is the root directory.  The result will be "." if there
 * is no directory part.
 */
char *d_dirname(const char *path) {
  char *d = 0;

  parse_filename(path, &d, 0);
  assert(d != 0);
  return d;
}

/** @brief Return the basename part of @p path
 * @param Path to parse
 * @return Base part of @p path
 *
 * Extracts the base part of @p path.  This is a simple lexical transformation
 * and no canonicalization is performed.  The result is always newly allocated
 * even if compares equal to @p path.
 */
char *d_basename(const char *path) {
  char *b = 0;

  parse_filename(path, 0, &b);
  assert(b != 0);
  return b;
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
