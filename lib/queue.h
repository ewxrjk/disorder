/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#ifndef QUEUE_H
#define QUEUE_H

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
  /* For DISORDER_PLAYER_PAUSES only: */
  time_t lastpaused, lastresumed;	/* when last paused/resumed, or 0 */
  long uptopause;			/* how much played up to last pause */
  /* For Disobedience */
  struct queuelike *ql;			/* owning queue */
};

extern struct queue_entry qhead;
/* queue of things yet to be played.  the head will be played
 * soonest. */

extern struct queue_entry phead;
/* things that have been played in the past.  the head is the oldest. */

void queue_read(void);
/* read the queue in.  Calls @fatal@ on error. */

void queue_write(void);
/* write the queue out.  Calls @fatal@ on error. */

void recent_read(void);
/* read the recently played list in.  Calls @fatal@ on error. */

void recent_write(void);
/* write the recently played list out.  Calls @fatal@ on error. */

struct queue_entry *queue_add(const char *track, const char *submitter,
			      int where);
#define WHERE_START 0			/* Add to head of queue */
#define WHERE_END 1			/* Add to end of queue */
#define WHERE_BEFORE_RANDOM 2		/* End, or before random track */
/* add an entry to the queue.  Return a pointer to the new entry. */

void queue_remove(struct queue_entry *q, const char *who);
/* remove an from the queue */

struct queue_entry *queue_find(const char *key);
/* find a track in the queue by name or ID */

void queue_played(struct queue_entry *q);
/* add @q@ to the played list */

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

void queue_id(struct queue_entry *q);
/* give @q@ an ID */

int queue_move(struct queue_entry *q, int delta, const char *who);
/* move element @q@ in the queue towards the front (@delta@ > 0) or towards the
 * back (@delta@ < 0).  The return value is the leftover delta once we've hit
 * the end in whichever direction we were going. */

void queue_moveafter(struct queue_entry *target,
		     int nqs, struct queue_entry **qs, const char *who);
/* Move all the elements QS to just after TARGET, or to the head if
 * TARGET=0. */

void queue_fix_sofar(struct queue_entry *q);
/* Fix up the sofar field for standalone players */

#endif /* QUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
/* arch-tag:23ec4c111fdd6573a0adc8c366b87e7b */
