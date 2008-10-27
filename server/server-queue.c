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
#include "disorder-server.h"

/* the head of the queue is played next, so normally we add to the tail */
struct queue_entry qhead = {
  .next = &qhead,
  .prev = &qhead
};

/* the head of the recent list is the oldest thing, the tail the most recently
 * played */
struct queue_entry phead = {
  .next = &phead,
  .prev = &phead
};

long pcount;

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
  int version = 0;

  if(!(fp = fopen(path, "r"))) {
    if(errno == ENOENT)
      return;			/* no queue */
    fatal(errno, "error opening %s", path);
  }
  head->next = head->prev = head;
  while(!inputline(path, fp, &buffer, '\n')) {
    if(buffer[0] == '#') {
      /* Version indicator */
      version = atoi(buffer + 1);
      continue;
    }
    q = xmalloc(sizeof *q);
    queue_unmarshall(q, buffer, queue_read_error, (void *)path);
    if(version < 1) {
      /* Fix up origin field as best we can; will be wrong in some cases but
       * hopefully not too horribly so. */
      q->origin = q->submitter ? origin_picked : origin_random;
      if(q->state == playing_isscratch)
        q->origin = origin_scratch;
    }
    if(head == &qhead
       && (!q->track
	   || !q->when))
      fatal(0, "incomplete queue entry in %s", path);
    queue_insert_entry(head->prev, q);
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
  /* Save version indicator */
  if(fprintf(fp, "#1\n") < 0)
    fatal(errno, "error writing %s", tmp);
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

struct queue_entry *queue_find(const char *key) {
  struct queue_entry *q;

  for(q = qhead.next;
      q != &qhead && strcmp(q->track, key) && strcmp(q->id, key);
      q = q->next)
    ;
  return q != &qhead ? q : 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
