/*
 * This file is part of DisOrder
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file lib/validity.c
 * @brief Various validity checks
 */
#include "common.h"
#include "validity.h"

#include "mem.h"

/** @brief Parse a playlist name
 * @param name Playlist name
 * @param ownerp Where to put owner, or NULL
 * @param sharep Where to put default sharing, or NULL
 * @return 0 on success, -1 on error
 *
 * Playlists take the form USER.PLAYLIST or just PLAYLIST.  The PLAYLIST part
 * is alphanumeric and nonempty.  USER is a username (see valid_username()).
 */
int playlist_parse_name(const char *name,
                        char **ownerp,
                        char **sharep) {
  const char *dot = strchr(name, '.'), *share;
  char *owner;

  if(dot) {
    /* Owned playlist */
    owner = xstrndup(name, dot - name);
    if(!valid_username(owner))
      return -1;
    if(!valid_username(dot + 1))
      return -1;
    share = "private";
  } else {
    /* Shared playlist */
    if(!valid_username(name))
      return -1;
    owner = 0;
    share = "shared";
  }
  if(ownerp)
    *ownerp = owner;
  if(sharep)
    *sharep = xstrdup(share);
  return 0;
}

/** @brief Return non-zero for a valid username
 * @param user Candidate username
 * @return Nonzero if it's valid
 *
 * Currently we only allow the letters and digits in ASCII, and a maximum
 * length of 32 octets.  We could be more liberal than this but it is a nice
 * simple test.  It is critical that semicolons are never allowed.
 *
 * NB also used by playlist_parse_name() to validate playlist names!
 */
int valid_username(const char *user) {
  if(!*user)
    return 0;
  if(strlen(user) > 32)
    return 0;
  while(*user) {
    const uint8_t c = *user++;
    /* For now we are very strict */
    if((c >= 'a' && c <= 'z')
       || (c >= 'A' && c <= 'Z')
       || (c >= '0' && c <= '9'))
      /* ok */;
    else
      return 0;
  }
  return 1;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
