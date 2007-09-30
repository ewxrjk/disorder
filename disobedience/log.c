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

struct monitor {
  struct monitor *next;
  unsigned long mask;
  monitor_callback *callback;
  void *u;
};

static struct monitor *monitors;

void all_update(void) {
  playing_update();
  queue_update();
  recent_update();
  control_update();
}

static void log_connected(void attribute((unused)) *v) {
  struct callbackdata *cbd;

  /* Don't know what we might have missed while disconnected so update
   * everything.  We get this at startup too and this is how we do the initial
   * state fetch. */
  all_update();
  /* Re-get the volume */
  cbd = xmalloc(sizeof *cbd);
  cbd->onerror = 0;
  disorder_eclient_volume(client, log_volume, -1, -1, cbd);
}

static void log_completed(void attribute((unused)) *v,
                          const char attribute((unused)) *track) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_failed(void attribute((unused)) *v,
                       const char attribute((unused)) *track,
                       const char attribute((unused)) *status) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_moved(void attribute((unused)) *v,
                      const char attribute((unused)) *user) {
   queue_update();
}

static void log_playing(void attribute((unused)) *v,
                        const char attribute((unused)) *track,
                        const char attribute((unused)) *user) {
  playing = 1;
  playing_update();
  control_update();
  /* we get a log_removed() anyway so we don't need to update_queue() from
   * here */
}

static void log_queue(void attribute((unused)) *v,
                      struct queue_entry attribute((unused)) *q) {
  queue_update();
}

static void log_recent_added(void attribute((unused)) *v,
                             struct queue_entry attribute((unused)) *q) {
  recent_update();
}

static void log_recent_removed(void attribute((unused)) *v,
                               const char attribute((unused)) *id) {
  /* nothing - log_recent_added() will trigger the relevant update */
}

static void log_removed(void attribute((unused)) *v,
                        const char attribute((unused)) *id,
                        const char attribute((unused)) *user) {
  queue_update();
}

static void log_scratched(void attribute((unused)) *v,
                          const char attribute((unused)) *track,
                          const char attribute((unused)) *user) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_state(void attribute((unused)) *v,
                      unsigned long state) {
  const struct monitor *m;

  /* Tell anything that cares about the state change */
  for(m = monitors; m; m = m->next) {
    if((state ^ last_state) & m->mask)
      m->callback(m->u, state);
  }
  last_state = state;
  control_update();
  /* If the track is paused or resume then the currently playing track is
   * refetched so that we can continue to correctly calculate the played so-far
   * field */
  playing_update();
}

static void log_volume(void attribute((unused)) *v,
                       int l, int r) {
  if(volume_l != l || volume_r != r) {
    volume_l = l;
    volume_r = r;
    control_update();
  }
}

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
