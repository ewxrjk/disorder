/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file disobedience/log.c
 * @brief State monitoring
 *
 * Disobedience relies on the server to tell when essentially anything changes,
 * even if it initiated the change itself.  It uses the @c log command to
 * achieve this.
 */

#include "disobedience.h"

/* State monitoring -------------------------------------------------------- */

static void log_connected(void *v);
static void log_completed(void *v, const char *track);
static void log_failed(void *v, const char *track, const char *status);
static void log_moved(void *v, const char *user);
static void log_playing(void *v, const char *track, const char *user);
static void log_queue(void *v, struct queue_entry *q);
static void log_recent_added(void *v, struct queue_entry *q);
static void log_recent_removed(void *v, const char *id);
static void log_removed(void *v, const char *id, const char *user);
static void log_scratched(void *v, const char *track, const char *user);
static void log_state(void *v, unsigned long state);
static void log_volume(void *v, int l, int r);
static void log_rescanned(void *v);
static void log_rights_changed(void *v, rights_type r);

/** @brief Callbacks for server state monitoring */
const disorder_eclient_log_callbacks log_callbacks = {
  .connected = log_connected,
  .completed = log_completed,
  .failed = log_failed,
  .moved = log_moved,
  .playing = log_playing,
  .queue = log_queue,
  .recent_added = log_recent_added,
  .recent_removed = log_recent_removed,
  .removed = log_removed,
  .scratched = log_scratched,
  .state = log_state,
  .volume = log_volume,
  .rescanned = log_rescanned,
  .rights_changed = log_rights_changed
};

/** @brief Update everything */
void all_update(void) {
  ++suppress_actions;
  event_raise("queue-changed", 0);
  event_raise("recent-changed", 0);
  event_raise("volume-changed", 0);
  event_raise("rescan-complete", 0);
  --suppress_actions;
}

/** @brief Called when the client connects
 *
 * Depending on server and network state the TCP connection to the server may
 * go up or down many times during the lifetime of Disobedience.  This function
 * is called whenever it connects.
 */
static void log_connected(void attribute((unused)) *v) {
  /* Don't know what we might have missed while disconnected so update
   * everything.  We get this at startup too and this is how we do the initial
   * state fetch. */
  all_update();
  event_raise("log-connected", 0);
}

/** @brief Called when the current track finishes playing */
static void log_completed(void attribute((unused)) *v,
                          const char attribute((unused)) *track) {
}

/** @brief Called when the current track fails */
static void log_failed(void attribute((unused)) *v,
                       const char attribute((unused)) *track,
                       const char attribute((unused)) *status) {
}

/** @brief Called when some track is moved within the queue */
static void log_moved(void attribute((unused)) *v,
                      const char attribute((unused)) *user) {
  event_raise("queue-changed", 0);
}

static void log_playing(void attribute((unused)) *v,
                        const char attribute((unused)) *track,
                        const char attribute((unused)) *user) {
}

/** @brief Called when a track is added to the queue */
static void log_queue(void attribute((unused)) *v,
                      struct queue_entry attribute((unused)) *q) {
  event_raise("queue-changed", 0);
}

/** @brief Called when a track is added to the recently-played list */
static void log_recent_added(void attribute((unused)) *v,
                             struct queue_entry attribute((unused)) *q) {
  event_raise("recent-changed", 0);
}

/** @brief Called when a track is removed from the recently-played list
 *
 * We do nothing here - log_recent_added() suffices.
 */
static void log_recent_removed(void attribute((unused)) *v,
                               const char attribute((unused)) *id) {
  /* nothing - log_recent_added() will trigger the relevant update */
}

/** @brief Called when a track is removed from the queue */
static void log_removed(void attribute((unused)) *v,
                        const char attribute((unused)) *id,
                        const char attribute((unused)) *user) {
  event_raise("queue-changed", 0);
}

/** @brief Called when the current track is scratched */
static void log_scratched(void attribute((unused)) *v,
                          const char attribute((unused)) *track,
                          const char attribute((unused)) *user) {
}

/** @brief Map from state bits to state change events */
static const struct {
  unsigned long bit;
  const char *event;
} state_events[] = {
  { DISORDER_PLAYING_ENABLED, "enabled-changed" },
  { DISORDER_RANDOM_ENABLED, "random-changed" },
  { DISORDER_TRACK_PAUSED, "pause-changed" },
  { DISORDER_PLAYING, "playing-changed" },
  { DISORDER_CONNECTED, "connected-changed" },
};
#define NSTATE_EVENTS (sizeof state_events / sizeof *state_events)

/** @brief Called when a state change occurs */
static void log_state(void attribute((unused)) *v,
                      unsigned long state) {
  unsigned long changes = state ^ last_state;
  static int first = 1;

  ++suppress_actions;
  if(first) {
    changes = -1UL;
    first = 0;
  }
  D(("log_state old=%s new=%s changed=%s",
     disorder_eclient_interpret_state(last_state),
     disorder_eclient_interpret_state(state),
     disorder_eclient_interpret_state(changes)));
  last_state = state;
  /* Notify interested parties what has changed */
  for(unsigned n = 0; n < NSTATE_EVENTS; ++n)
    if(changes & state_events[n].bit)
      event_raise(state_events[n].event, 0);
  --suppress_actions;
}

/** @brief Called when volume changes */
static void log_volume(void attribute((unused)) *v,
                       int l, int r) {
  if(!rtp_supported && (volume_l != l || volume_r != r)) {
    volume_l = l;
    volume_r = r;
    ++suppress_actions;
    event_raise("volume-changed", 0);
    --suppress_actions;
  }
}

/** @brief Called when a rescan completes */
static void log_rescanned(void attribute((unused)) *v) {
  event_raise("rescan-complete", 0);
}

/** @brief Called when our rights change */
static void log_rights_changed(void attribute((unused)) *v,
                               rights_type new_rights) {
  ++suppress_actions;
  last_rights = new_rights;
  event_raise("rights-changed", 0);
  --suppress_actions;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
