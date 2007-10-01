/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007 Richard Kettlewell
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

/** @brief Callbacks for server state monitoring */
const disorder_eclient_log_callbacks log_callbacks = {
  log_connected,
  log_completed,
  log_failed,
  log_moved,
  log_playing,
  log_queue,
  log_recent_added,
  log_recent_removed,
  log_removed,
  log_scratched,
  log_state,
  log_volume
};

/** @brief State monitor
 *
 * We keep a linked list of everything that is interested in state changes.
 */
struct monitor {
  /** @brief Next monitor */
  struct monitor *next;

  /** @brief State bits of interest */
  unsigned long mask;

  /** @brief Function to call if any of @c mask change */
  monitor_callback *callback;

  /** @brief User data for callback */
  void *u;
};

/** @brief List of monitors */
static struct monitor *monitors;

/** @brief Update everything */
void all_update(void) {
  queue_update();
  recent_update();
  volume_update();
}

/** @brief Called when the client connects
 *
 * Depending on server and network state the TCP connection to the server may
 * go up or down many times during the lifetime of Disobedience.  This function
 * is called whenever it connects.
 *
 * The intent is to use the monitor logic to achieve this in future.
 */
static void log_connected(void attribute((unused)) *v) {
  /* Don't know what we might have missed while disconnected so update
   * everything.  We get this at startup too and this is how we do the initial
   * state fetch. */
  all_update();
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
  queue_update();
}

static void log_playing(void attribute((unused)) *v,
                        const char attribute((unused)) *track,
                        const char attribute((unused)) *user) {
}

/** @brief Called when a track is added to the queue */
static void log_queue(void attribute((unused)) *v,
                      struct queue_entry attribute((unused)) *q) {
  queue_update();
}

/** @brief Called when a track is added to the recently-played list */
static void log_recent_added(void attribute((unused)) *v,
                             struct queue_entry attribute((unused)) *q) {
  recent_update();
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

  queue_update();
}

/** @brief Called when the current track is scratched */
static void log_scratched(void attribute((unused)) *v,
                          const char attribute((unused)) *track,
                          const char attribute((unused)) *user) {
}

/** @brief Called when a state change occurs */
static void log_state(void attribute((unused)) *v,
                      unsigned long state) {
  const struct monitor *m;
  unsigned long changes = state ^ last_state;
  static int first = 1;
  
  if(first) {
    changes = -1UL;
    first = 0;
  }
  D(("log_state old=%s new=%s changed=%s",
     disorder_eclient_interpret_state(last_state),
     disorder_eclient_interpret_state(state),
     disorder_eclient_interpret_state(changes)));
  last_state = state;
  /* Tell anything that cares about the state change */
  for(m = monitors; m; m = m->next) {
    if(changes & m->mask)
      m->callback(m->u);
  }
}

/** @brief Called when volume changes */
static void log_volume(void attribute((unused)) *v,
                       int l, int r) {
  if(volume_l != l || volume_r != r) {
    volume_l = l;
    volume_r = r;
    volume_update();
  }
}

/** @brief Add a monitor to the list
 * @param callback Function to call
 * @param u User data to pass to @p callback
 * @param mask Mask of flags that @p callback cares about
 *
 * Pass @p mask as -1UL to match all flags.
 */
void register_monitor(monitor_callback *callback,
                      void *u,
                      unsigned long mask) {
  struct monitor *m = xmalloc(sizeof *m);

  m->next = monitors;
  m->mask = mask;
  m->callback = callback;
  m->u = u;
  monitors = m;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
