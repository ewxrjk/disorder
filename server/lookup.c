/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file server/macros-disorder.c
 * @brief DisOrder-specific expansions
 */

#include <config.h>
#include "types.h"

#include "sink.h"
#include "client.h"
#include "lookup.h"
#include "cgi.h"

/** @brief Client used by CGI
 *
 * The caller should arrange for this to be created before any of
 * these expansions are used (if it cannot connect then it's safe to
 * leave it as NULL).
 */
disorder_client *client;

/** @brief Cached data */
static unsigned flags;

struct queue_entry *queue;
struct queue_entry *playing;
struct queue_entry *recent;

int volume_left;
int volume_right;

char **files;
int nfiles;

char **dirs;
int ndirs;

char **new;
int nnew;

rights_type rights;

int enabled;
int random_enabled;

/** @brief Fetch cachable data */
static void lookup(unsigned want) {
  unsigned need = want ^ (flags & want);
  struct queue_entry *r, *rnext;
  const char *dir, *re;
  char *rights;

  if(!client || !need)
    return;
  if(need & DC_QUEUE)
    disorder_queue(client, &queue);
  if(need & DC_PLAYING)
    disorder_playing(client, &playing);
  if(need & DC_NEW)
    disorder_new_tracks(client, &new, &nnew, 0);
  if(need & DC_RECENT) {
    /* we need to reverse the order of the list */
    disorder_recent(client, &r);
    while(r) {
      rnext = r->next;
      r->next = recent;
      recent = r;
      r = rnext;
    }
  }
  if(need & DC_VOLUME)
    disorder_get_volume(client,
                        &volume_left, &volume_right);
  /* DC_FILES and DC_DIRS are looking obsolete now */
  if(need & (DC_FILES|DC_DIRS)) {
    if(!(dir = cgi_get("directory")))
      dir = "";
    re = cgi_get("regexp");
    if(need & DC_DIRS)
      if(disorder_directories(client, dir, re,
                              &dirs, &ndirs))
        ndirs = 0;
    if(need & DC_FILES)
      if(disorder_files(client, dir, re,
                        &files, &nfiles))
        nfiles = 0;
  }
  if(need & DC_RIGHTS) {
    rights = RIGHT_READ;	/* fail-safe */
    if(!disorder_userinfo(client, disorder_user(client),
                          "rights", &rights))
      parse_rights(rights, &rights, 1);
  }
  if(need & DC_ENABLED)
    disorder_enabled(client, &enabled);
  if(need & DC_RANDOM_ENABLED)
    disorder_random_enabled(client, &random_enabled);
  if(need & DC_RANDOM_ENABLED)
  flags |= need;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
