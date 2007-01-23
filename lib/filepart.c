/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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

#include <string.h>

#include "filepart.h"
#include "mem.h"

char *d_dirname(const char *path) {
  const char *s;

  if((s = strrchr(path, '/'))) {
    if(s == path)
      return xstrdup("/");
    else
      return xstrndup(path, s - path);
  } else
    return xstrdup(".");
}

static const char *find_extension(const char *path) {
  const char *q = path + strlen(path);
  
  while(q > path && strchr("abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"0123456789", *q))
    --q;
  return *q == '.' ? q : 0;
}

const char *strip_extension(const char *path) {
  const char *q = find_extension(path);

  return q ? xstrndup(path, q - path) : path;
}

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
/* arch-tag:4WKGtvPyLNQ6h3I0n2cMIg */
