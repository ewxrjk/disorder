/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
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
/** @file lib/trackname.c
 * @brief Track name calculation
 */
#include "common.h"

#include <pcre.h>
#include <fnmatch.h>

#include "trackname.h"
#include "configuration.h"
#include "regsub.h"
#include "log.h"
#include "filepart.h"
#include "unicode.h"

const struct collection *find_track_collection(const char *track) {
  int n;
  size_t l, tl = strlen(track);

  for(n = 0; n < config->collection.n; ++n) {
    l = strlen(config->collection.s[n].root);
    if(tl > l
       && !strncmp(track, config->collection.s[n].root, l)
       && track[l] == '/')
      break;
  }
  if(n < config->collection.n)
    return &config->collection.s[n];
  else
    return 0;
}

const char *find_track_root(const char *track) {
  const struct collection *c = find_track_collection(track);
  if(c)
    return c->root;
  /* Suppress this message for scratches */
  for(int n = 0; n < config->scratch.n; ++n)
    if(!strcmp(track, config->scratch.s[n]))
      return 0;
  disorder_error(0, "found track in no collection '%s'", track);
  return 0;
}

const char *track_rootless(const char *track) {
  const char *root;

  if(!(root = find_track_root(track))) return 0;
  return track + strlen(root);
}

const char *trackname_part(const char *track,
			   const char *context,
			   const char *part) {
  int n;
  const char *replaced, *rootless;

  assert(track != 0);
  if(!strcmp(part, "path")) return track;
  if(!strcmp(part, "ext")) return extension(track);
  if((rootless = track_rootless(track))) track = rootless;
  for(n = 0; n < config->namepart.n; ++n) {
    if(!strcmp(config->namepart.s[n].part, part)
       && fnmatch(config->namepart.s[n].context, context, 0) == 0) {
      if((replaced = regsub(config->namepart.s[n].re,
			    track,
			    config->namepart.s[n].replace,
			    config->namepart.s[n].reflags
			    |REGSUB_MUST_MATCH
			    |REGSUB_REPLACE)))
	return replaced;
    }
  }
  return "";
}

const char *trackname_transform(const char *type,
				const char *subject,
				const char *context) {
  const char *replaced;
  int n;
  const struct transform *k;

  for(n = 0; n < config->transform.n; ++n) {
    k = &config->transform.t[n];
    if(strcmp(k->type, type))
      continue;
    if(fnmatch(k->context, context, 0) != 0)
      continue;
    if((replaced = regsub(k->re, subject, k->replace, k->flags)))
      subject = replaced;
  }
  return subject;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
