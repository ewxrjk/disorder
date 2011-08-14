/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2009 Richard Kettlewell
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
/** @file lib/queue.h
 * @brief Track queues
 *
 * Used for the queue, the recently played list and the currently playing
 * track, both in the server and in clients.
 */
#ifndef QUEUE_H
#define QUEUE_H

#include <time.h>

/** @brief Possible track states */
enum playing_state {
  /** @brief Track failed to play */
  playing_failed,

  /** @brief OBSOLETE
   *
   * Formerly denoted an unplayed scratch.  This is now indicated by @p
   * playing_unplayed and @p origin_scratch.
   */
  playing_isscratch,

  /** @brief OBSOLETE
   *
   * Formerly meant that no player could be found.  Nothing sets this any more.
   */
  playing_no_player,

  /** @brief Play completed successfully
   *
   * Currently this actually means it finished decoding - it might still be
   * buffered in the speaker, RTP player, sound card, etc.
   *
   * It might also mean that it's a (short!) track that hasn't been played at
   * all yet but has been fully decoded ahead of time!  (This is very confusing
   * so might change.)
   */
  playing_ok,

  /** @brief Track is playing, but paused */
  playing_paused,

  /** @brief Track is playing but the server is quitting */
  playing_quitting,

  /** @brief OBSOLETE
   *
   * Formerly this meant a track that was picked at random and has not yet been
   * played.  This situation is now indicated by @p playing_unplayed and @p
   * origin_random (or @p origin_adopted).
   */
  playing_random,

  /** @brief Track was scratched */
  playing_scratched,

  /** @brief Track is now playing
   *
   * This refers to the actual playing track, not something being decoded ahead
   * of time.
   */
  playing_started,

  /** @brief Track has not been played yet */
  playing_unplayed
};

extern const char *const playing_states[];

/** @brief Possible track origins
 *
 * This is a newly introduced field.  The aim is ultimately to separate the
 * concepts of the track origin and its current state.  NB that both are
 * potentially mutable!
 */
enum track_origin {
  /** @brief Track was picked at random and then adopted by a user
   *
   * @c submitter identifies who adopted it.
   */
  origin_adopted,

  /** @brief Track was picked by a user
   *
   * @c submitter identifies who picked it
   */
  origin_picked,

  /** @brief Track was picked at random
   *
   * @c submitter will be NULL
   */
  origin_random,

  /** @brief Track was scheduled by a user
   *
   * @c submitter identifies who picked it
   */
  origin_scheduled,

  /** @brief Track is a scratch
   *
   * @c submitter identifies who did the scratching
   */
  origin_scratch
};

extern const char *const track_origins[];

/** @brief One queue/recently played entry
 *
 * The queue and recently played list form a doubly linked list with the head
 * and tail referred to from @ref qhead and @ref phead.
 */
struct queue_entry {
  /** @brief Next entry */
  struct queue_entry *next;

  /** @brief Previous entry */
  struct queue_entry *prev;

  /** @brief Path to track (a database key) */
  const char *track;

  /** @brief Submitter or NULL
   *
   * Adopter, if @c origin is @ref origin_adopted.
   */
  const char *submitter;

  /** @brief When submitted */
  time_t when;

  /** @brief When played */
  time_t played;

  /** @brief Current state
   *
   * Currently this includes some origin information but this is being phased
   * out. */
  enum playing_state state;

  /** @brief Where track came from */
  enum track_origin origin;

  /** @brief Wait status from player
   *
   * Only valid in certain states (TODO).
   */
  long wstat;

  /** @brief Who scratched this track or NULL */
  const char *scratched;

  /** @brief Unique ID string */
  const char *id;

  /** @brief Estimated starting time */
  time_t expected;

  /** @brief Type word from plugin (playing/buffered tracks only) */
  unsigned long type;			/* type word from plugin */

  /** @brief Plugin for this track (playing/buffered tracks only) */
  const struct plugin *pl;

  /** @brief Player-specific data (playing/buffered tracks only) */
  void *data;

  /** @brief How much of track has been played so far (seconds) */
  long sofar;

  /** @brief True if track preparation is underway
   *
   * This is set when a decoder has been started and is expected to connect to
   * the speaker, but the speaker has not sent as @ref SM_ARRIVED message back
   * yet. */
  int preparing;

  /** @brief True if decoder is connected to speaker 
   *
   * This is not a @ref playing_state for a couple of reasons
   * - it is orthogonal to @ref playing_started and @ref playing_unplayed
   * - it would have to be hidden to other users of @c queue_entry
   *
   * For non-raw tracks this should always be zero.
   */
  int prepared;
  /* For DISORDER_PLAYER_PAUSES only: */

  /** @brief When last paused or 0 */
  time_t lastpaused;

  /** @brief When last resumed or 0 */
  time_t lastresumed;

  /** @brief How much of track was played up to last pause (seconds) */
  long uptopause;

  /** @brief Owning queue (for Disobedience only) */
  struct queuelike *ql;
  
  /** @brief Decoder (or player) process ID */
  pid_t pid;

  /** @brief Termination signal sent to subprocess
   *
   * Used to supress 'terminated' messages.
   */
  int killed;
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

void queue_free(struct queue_entry *q, int rest);

#endif /* QUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
