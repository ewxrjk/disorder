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
/** @file server/queue-ops.c
 * @brief Track queues (server-specific code)
 */
#include "disorder-server.h"

static int find_in_list(struct queue_entry *needle,
			int nqs, struct queue_entry **qs) {
  int n;

  for(n = 0; n < nqs; ++n)
    if(qs[n] == needle)
      return 1;
  return 0;
}

static int id_in_use(const char *id) {
  struct queue_entry *q;

  for(q = qhead.next; q != &qhead; q = q->next)
    if(!strcmp(id, q->id))
      return 1;
  return 0;
}

static void queue_id(struct queue_entry *q) {
  const char *id;

  id = random_id();
  while(id_in_use(id))
    id = random_id();
  q->id = id;
}

struct queue_entry *queue_add(const char *track, const char *submitter,
			      int where, enum track_origin origin) {
  struct queue_entry *q, *beforeme;

  q = xmalloc(sizeof *q);
  q->track = xstrdup(track);
  q->submitter = submitter ? xstrdup(submitter) : 0;
  q->state = playing_unplayed;
  q->origin = origin;
  q->pid = -1;
  queue_id(q);
  time(&q->when);
  switch(where) {
  case WHERE_START:
    queue_insert_entry(&qhead, q);
    break;
  case WHERE_END:
    queue_insert_entry(qhead.prev, q);
    break;
  case WHERE_BEFORE_RANDOM:
    /* We want to find the point in the queue before the block of random tracks
     * at the end. */
    beforeme = &qhead;
    while(beforeme->prev != &qhead
	  && beforeme->prev->origin == origin_random)
      beforeme = beforeme->prev;
    queue_insert_entry(beforeme->prev, q);
    break;
  }
  /* submitter will be a null pointer for a scratch */
  if(submitter)
    notify_queue(track, submitter);
  eventlog_raw("queue", queue_marshall(q), (const char *)0);
  return q;
}

int queue_move(struct queue_entry *q, int delta, const char *who) {
  int moved = 0;
  char buffer[20];

  /* not the most efficient approach but hopefuly relatively comprehensible:
   * the idea is that for each step we determine which nodes are affected, and
   * fill in all the links starting at the 'prev' end and moving towards the
   * 'next' end. */
  
  while(delta > 0 && q->prev != &qhead) {
    struct queue_entry *n, *p, *pp;

    n = q->next;
    p = q->prev;
    pp = p->prev;
    pp->next = q;
    q->prev = pp;
    q->next = p;
    p->prev = q;
    p->next = n;
    n->prev = p;
    --delta;
    ++moved;
  }

  while(delta < 0 && q->next != &qhead) {
    struct queue_entry *n, *p, *nn;

    p = q->prev;
    n = q->next;
    nn = n->next;
    p->next = n;
    n->prev = p;
    n->next = q;
    q->prev = n;
    q->next = nn;
    nn->prev = q;
    ++delta;
    --moved;
  }

  if(moved) {
    info("user %s moved %s", who, q->id);
    notify_queue_move(q->track, who);
    sprintf(buffer, "%d", moved);
    eventlog("moved", who, (char *)0);
  }
  
  return delta;
}

void queue_moveafter(struct queue_entry *target,
		     int nqs, struct queue_entry **qs,
		     const char *who) {
  struct queue_entry *q;
  int n;

  /* Normalize */
  if(!target)
    target = &qhead;
  else
    while(find_in_list(target, nqs, qs))
      target = target->prev;
  /* Do the move */
  for(n = 0; n < nqs; ++n) {
    q = qs[n];
    queue_delete_entry(q);
    queue_insert_entry(target, q);
    target = q;
    /* Log the individual tracks */
    info("user %s moved %s", who, q->id);
    notify_queue_move(q->track, who);
  }
  /* Report that the queue changed to the event log */
  eventlog("moved", who, (char *)0);
}

void queue_remove(struct queue_entry *which, const char *who) {
  if(who) {
    info("user %s removed %s", who, which->id);
    notify_queue_move(which->track, who);
  }
  eventlog("removed", which->id, who, (const char *)0);
  queue_delete_entry(which);
}

void queue_played(struct queue_entry *q) {
  while(pcount && pcount >= config->history) {
    eventlog("recent_removed", phead.next->id, (char *)0);
    queue_delete_entry(phead.next);
    pcount--;
  }
  if(config->history) {
    eventlog_raw("recent_added", queue_marshall(q), (char *)0);
    queue_insert_entry(phead.prev, q);
    ++pcount;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
