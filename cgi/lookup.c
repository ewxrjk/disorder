/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file server/lookup.c
 * @brief Server lookups
 *
 * To improve performance many server lookups are cached.
 */

#include "disorder-cgi.h"

/** @brief Cached data */
static unsigned flags;

/** @brief Map of hashes to queud data */
static hash *queuemap;

struct queue_entry *dcgi_queue;
struct queue_entry *dcgi_playing;
struct queue_entry *dcgi_recent;

int dcgi_volume_left;
int dcgi_volume_right;

char **dcgi_new;
int dcgi_nnew;

rights_type dcgi_rights;

int dcgi_enabled;
int dcgi_random_enabled;

static void queuemap_add(struct queue_entry *q) {
  if(!queuemap)
    queuemap = hash_new(sizeof (struct queue_entry *));
  for(; q; q = q->next)
    hash_add(queuemap, q->id, &q, HASH_INSERT_OR_REPLACE);
}

/** @brief Fetch cachable data */
void dcgi_lookup(unsigned want) {
  unsigned need = want ^ (flags & want);
  struct queue_entry *r, *rnext;
  char *rs;

  if(!dcgi_client || !need)
    return;
  if(need & DCGI_QUEUE) {
    disorder_queue(dcgi_client, &dcgi_queue);
    queuemap_add(dcgi_queue);
  }
  if(need & DCGI_PLAYING) {
    disorder_playing(dcgi_client, &dcgi_playing);
    queuemap_add(dcgi_playing);
  }
  if(need & DCGI_NEW)
    disorder_new_tracks(dcgi_client, &dcgi_new, &dcgi_nnew, 0);
  if(need & DCGI_RECENT) {
    /* we need to reverse the order of the list */
    disorder_recent(dcgi_client, &r);
    while(r) {
      rnext = r->next;
      r->next = dcgi_recent;
      dcgi_recent = r;
      r = rnext;
    }
    queuemap_add(dcgi_recent);
  }
  if(need & DCGI_VOLUME)
    disorder_get_volume(dcgi_client,
                        &dcgi_volume_left, &dcgi_volume_right);
  if(need & DCGI_RIGHTS) {
    dcgi_rights = RIGHT_READ;	/* fail-safe */
    if(!disorder_userinfo(dcgi_client, disorder_user(dcgi_client),
                          "rights", &rs))
      parse_rights(rs, &dcgi_rights, 1);
  }
  if(need & DCGI_ENABLED)
    disorder_enabled(dcgi_client, &dcgi_enabled);
  if(need & DCGI_RANDOM_ENABLED)
    disorder_random_enabled(dcgi_client, &dcgi_random_enabled);
  flags |= need;
}

/** @brief Locate a track by ID */
struct queue_entry *dcgi_findtrack(const char *id) {
  struct queue_entry **qq;

  if(queuemap && (qq = hash_find(queuemap, id)))
    return *qq;
  dcgi_lookup(DCGI_PLAYING);
  if(queuemap && (qq = hash_find(queuemap, id)))
    return *qq;
  dcgi_lookup(DCGI_QUEUE);
  if(queuemap && (qq = hash_find(queuemap, id)))
    return *qq;
  dcgi_lookup(DCGI_RECENT);
  if(queuemap && (qq = hash_find(queuemap, id)))
    return *qq;
  return NULL;
}

void dcgi_lookup_reset(void) {
  /* Forget everything we knew */
  flags = 0;
  queuemap = 0;
  dcgi_recent = 0;
  dcgi_queue = 0;
  dcgi_playing = 0;
  dcgi_rights = 0;
  dcgi_new = 0;
  dcgi_nnew = 0;
  dcgi_enabled = 0;
  dcgi_random_enabled = 0;
  dcgi_volume_left = dcgi_volume_right = 0;
}


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
