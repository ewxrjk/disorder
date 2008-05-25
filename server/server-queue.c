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

#include <config.h>
#include "types.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "mem.h"
#include "log.h"
#include "split.h"
#include "printf.h"
#include "queue.h"
#include "server-queue.h"
#include "eventlog.h"
#include "plugin.h"
#include "random.h"
#include "configuration.h"
#include "inputline.h"
#include "disorder.h"

/* the head of the queue is played next, so normally we add to the tail */
struct queue_entry qhead = { &qhead, &qhead, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* the head of the recent list is the oldest thing, the tail the most recently
 * played */
struct queue_entry phead = { &phead, &phead, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static long pcount;

/* add new entry @n@ to a doubly linked list just after @b@ */
static void l_add(struct queue_entry *b, struct queue_entry *n) {
  n->prev = b;
  n->next = b->next;
  n->next->prev = n;
  n->prev->next = n;
}

/* remove an entry from a doubly-linked list */
static void l_remove(struct queue_entry *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
}

void queue_fix_sofar(struct queue_entry *q) {
  long sofar;
  
  /* Fake up SOFAR field for currently-playing tracks that don't have it filled
   * in by the speaker process.  XXX this horrible bodge should go away when we
   * have a more general implementation of pausing as that field will always
   * have to be right for the playing track. */
  if((q->state == playing_started
      || q->state == playing_paused)
     && q->type & DISORDER_PLAYER_PAUSES
     && (q->type & DISORDER_PLAYER_TYPEMASK) != DISORDER_PLAYER_RAW) {
    if(q->lastpaused) {
      if(q->uptopause == -1)		/* Don't know how far thru. */
	sofar = -1;
      else if(q->lastresumed)		/* Has been paused and resumed. */
	sofar = q->uptopause + time(0) - q->lastresumed;
      else				/* Currently paused. */
	sofar = q->uptopause;
    } else				/* Never been paused. */
      sofar = time(0) - q->played;
    q->sofar = sofar;
  }
}

static void queue_read_error(const char *msg,
			     void *u) {
  fatal(0, "error parsing queue %s: %s", (const char *)u, msg);
}

static void queue_do_read(struct queue_entry *head, const char *path) {
  char *buffer;
  FILE *fp;
  struct queue_entry *q;

  if(!(fp = fopen(path, "r"))) {
    if(errno == ENOENT)
      return;			/* no queue */
    fatal(errno, "error opening %s", path);
  }
  head->next = head->prev = head;
  while(!inputline(path, fp, &buffer, '\n')) {
    q = xmalloc(sizeof *q);
    queue_unmarshall(q, buffer, queue_read_error, (void *)path);
    if(head == &qhead
       && (!q->track
	   || !q->when))
      fatal(0, "incomplete queue entry in %s", path);
    l_add(head->prev, q);
  }
  if(ferror(fp)) fatal(errno, "error reading %s", path);
  fclose(fp);
}

void queue_read(void) {
  queue_do_read(&qhead, config_get_file("queue"));
}

void recent_read(void) {
  struct queue_entry *q;

  queue_do_read(&phead, config_get_file("recent"));
  /* reset pcount after loading */
  pcount = 0;
  q = phead.next;
  while(q != &phead) {
    ++pcount;
    q = q->next;
  }
}

static void queue_do_write(const struct queue_entry *head, const char *path) {
  char *tmp;
  FILE *fp;
  struct queue_entry *q;

  byte_xasprintf(&tmp, "%s.new", path);
  if(!(fp = fopen(tmp, "w"))) fatal(errno, "error opening %s", tmp);
  for(q = head->next; q != head; q = q->next)
    if(fprintf(fp, "%s\n", queue_marshall(q)) < 0)
      fatal(errno, "error writing %s", tmp);
  if(fclose(fp) < 0) fatal(errno, "error closing %s", tmp);
  if(rename(tmp, path) < 0) fatal(errno, "error replacing %s", path);
}

void queue_write(void) {
  queue_do_write(&qhead, config_get_file("queue"));
}

void recent_write(void) {
  queue_do_write(&phead, config_get_file("recent"));
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
			      int where) {
  struct queue_entry *q, *beforeme;

  q = xmalloc(sizeof *q);
  q->track = xstrdup(track);
  q->submitter = submitter ? xstrdup(submitter) : 0;
  q->state = playing_unplayed;
  queue_id(q);
  time(&q->when);
  switch(where) {
  case WHERE_START:
    l_add(&qhead, q);
    break;
  case WHERE_END:
    l_add(qhead.prev, q);
    break;
  case WHERE_BEFORE_RANDOM:
    /* We want to find the point in the queue before the block of random tracks
     * at the end. */
    beforeme = &qhead;
    while(beforeme->prev != &qhead
	  && beforeme->prev->state == playing_random)
      beforeme = beforeme->prev;
    l_add(beforeme->prev, q);
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

static int find_in_list(struct queue_entry *needle,
			int nqs, struct queue_entry **qs) {
  int n;

  for(n = 0; n < nqs; ++n)
    if(qs[n] == needle)
      return 1;
  return 0;
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
    l_remove(q);
    l_add(target, q);
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
  l_remove(which);
}

struct queue_entry *queue_find(const char *key) {
  struct queue_entry *q;

  for(q = qhead.next;
      q != &qhead && strcmp(q->track, key) && strcmp(q->id, key);
      q = q->next)
    ;
  return q != &qhead ? q : 0;
}

void queue_played(struct queue_entry *q) {
  while(pcount && pcount >= config->history) {
    eventlog("recent_removed", phead.next->id, (char *)0);
    l_remove(phead.next);
    pcount--;
  }
  if(config->history) {
    eventlog_raw("recent_added", queue_marshall(q), (char *)0);
    l_add(phead.prev, q);
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
