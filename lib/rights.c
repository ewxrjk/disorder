/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/rights.c
 * @brief User rights
 */

#include <config.h>
#include "types.h"

#include <string.h>

#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "rights.h"
#include "vector.h"

static const struct {
  rights_type bit;
  const char *name;
} rights_names[] = {
  { RIGHT_READ, "read" },
  { RIGHT_PLAY, "play" },
  { RIGHT_MOVE_ANY, "move any" },
  { RIGHT_MOVE_MINE, "move mine" },
  { RIGHT_MOVE_RANDOM, "move random" },
  { RIGHT_REMOVE_ANY, "remove any" },
  { RIGHT_REMOVE_MINE, "remove mine" },
  { RIGHT_REMOVE_RANDOM, "remove random" },
  { RIGHT_SCRATCH_ANY, "scratch any" },
  { RIGHT_SCRATCH_MINE, "scratch mine" },
  { RIGHT_SCRATCH_RANDOM, "scratch random" },
  { RIGHT_VOLUME, "volume" },
  { RIGHT_ADMIN, "admin" },
  { RIGHT_RESCAN, "rescan" },
  { RIGHT_REGISTER, "register" },
  { RIGHT_USERINFO, "userinfo" },
  { RIGHT_PREFS, "prefs" },
  { RIGHT_GLOBAL_PREFS, "global prefs" }
};
#define NRIGHTS (sizeof rights_names / sizeof *rights_names)

/** @brief Convert a rights word to a string */
char *rights_string(rights_type r) {
  struct dynstr d[1];
  size_t n;

  dynstr_init(d);
  for(n = 0; n < NRIGHTS; ++n) {
    if(r & rights_names[n].bit) {
      if(d->nvec)
        dynstr_append(d, ',');
      dynstr_append_string(d, rights_names[n].name);
    }
  }
  dynstr_terminate(d);
  return d->vec;
}

/** @brief Compute default rights for a new user
 * @return Default rights value
 */
rights_type default_rights(void) {
  /* TODO get rights from config */
  rights_type r = RIGHTS__MASK & ~(RIGHT_ADMIN|RIGHT_REGISTER
                                   |RIGHT_MOVE__MASK
                                   |RIGHT_SCRATCH__MASK
                                   |RIGHT_REMOVE__MASK);
  if(config->restrictions & RESTRICT_SCRATCH)
    r |= RIGHT_SCRATCH_MINE|RIGHT_SCRATCH_RANDOM;
  else
    r |= RIGHT_SCRATCH_ANY;
  if(!(config->restrictions & RESTRICT_MOVE))
    r |= RIGHT_MOVE_ANY;
  if(config->restrictions & RESTRICT_REMOVE)
    r |= RIGHT_REMOVE_MINE;
  else
    r |= RIGHT_REMOVE_ANY;
  return r;
}

/** @brief Parse a rights list
 * @param s Rights list in string form
 * @param rp Where to store rights, or NULL to just validate
 * @return 0 on success, non-0 if @p s is not valid
 */
int parse_rights(const char *s, rights_type *rp) {
  rights_type r = 0;
  const char *t;
  size_t n, l;

  if(!*s) {
    /* You can't have no rights */
    error(0, "empty rights string");
    return -1;
  }
  while(*s) {
    t = strchr(s, ',');
    if(!t)
      t = s + strlen(s);
    l = (size_t)(t - s);
    if(l == 3 && !strncmp(s, "all", 3))
      r = RIGHTS__MASK;
    else {
      for(n = 0; n < NRIGHTS; ++n)
	if(strlen(rights_names[n].name) == l
	   && !strncmp(rights_names[n].name, s, l))
	  break;
      if(n >= NRIGHTS) {
	error(0, "unknown user right '%.*s'", (int)l, s);
	return -1;
      }
      r |= rights_names[n].bit;
    }
    s = t;
    if(*s == ',')
      ++s;
  }
  if(rp)
    *rp = r;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
