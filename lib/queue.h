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

#ifndef QUEUE_H
#define QUEUE_H

#include <time.h>


enum playing_state {
  playing_failed,			/* failed to play */
  playing_isscratch,			/* this is a scratch track */
  playing_no_player,			/* couldn't find a player */
  playing_ok,				/* played OK */
  playing_paused,			/* started but paused */
  playing_quitting,			/* interrupt because server quit */
  playing_random,			/* unplayed randomly chosen track */
  playing_scratched,			/* was scratched */
  playing_started,			/* started to play */
  playing_unplayed			/* haven't played this track yet */
};

extern const char *playing_states[];

/* queue entries form a circular doubly-linked list */
struct queue_entry {
  struct queue_entry *next;		/* next entry */
  struct queue_entry *prev;		/* previous entry */
  const char *track;			/* path to track */
  const char *submitter;		/* name of submitter */
  time_t when;				/* time submitted */
  time_t played;			/* when played */
  enum playing_state state;		/* state */
  long wstat;				/* wait status */
  const char *scratched;		/* scratched by */
  const char *id;			/* queue entry ID */
  time_t expected;			/* expected started time */
  /* for playing or soon-to-be-played tracks only: */
  unsigned long type;			/* type word from plugin */
  const struct plugin *pl;		/* plugin that's playing this track */
  void *data;				/* player data */
  long sofar;				/* how much played so far */
  int prepared;				/* true when connected to speaker */
  /* For DISORDER_PLAYER_PAUSES only: */
  time_t lastpaused, lastresumed;	/* when last paused/resumed, or 0 */
  long uptopause;			/* how much played up to last pause */
  /* For Disobedience */
  struct queuelike *ql;			/* owning queue */
};

void queue_insert_entry(struct queue_entry *b, struct queue_entry *n);
void queue_delete_entry(struct queue_entry *node);

int queue_unmarshall(struct queue_entry *q, const char *s,
		     void (*error_handler)(const char *, void *),
		     void *u);
/* unmarshall UTF-8 string @s@ into @q@ */

int queue_unmarshall_vec(struct queue_entry *q, int nvec, char **vec,
		     void (*error_handler)(const char *, void *),
		     void *u);
/* unmarshall pre-split string @vec@ into @q@ */

char *queue_marshall(const struct queue_entry *q);
/* marshall @q@ into a UTF-8 string */

#endif /* QUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
