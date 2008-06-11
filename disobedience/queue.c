/*
 * This file is part of DisOrder
 * Copyright (C) 2006-2008 Richard Kettlewell
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
#include "queue-generic.h"

/** @brief The actual queue */
static struct queue_entry *actual_queue;
static struct queue_entry *actual_playing_track;

/** @brief The playing track */
struct queue_entry *playing_track;

/** @brief When we last got the playing track */
time_t last_playing;

/** @brief Called when either the actual queue or the playing track change */
static void queue_playing_changed(void) {
  struct queue_entry *q = xmalloc(sizeof *q);

  if(actual_playing_track) {
    *q = *actual_playing_track;
    q->next = actual_queue;
    playing_track = q;
  } else {
    playing_track = NULL;
    q = actual_queue;
  }
  time(&last_playing);          /* for column_length() */
  ql_new_queue(&ql_queue, q);
  /* Tell anyone who cares */
  event_raise("queue-list-changed", q);
  event_raise("playing-track-changed", q);
}

/** @brief Update the queue itself */
static void queue_completed(void attribute((unused)) *v,
                            const char *error,
                            struct queue_entry *q) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  actual_queue = q;
  queue_playing_changed();
}

/** @brief Update the playing track */
static void playing_completed(void attribute((unused)) *v,
                              const char *error,
                              struct queue_entry *q) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  actual_playing_track = q;
  queue_playing_changed();
}

/** @brief Schedule an update to the queue
 *
 * Called whenever a track is added to it or removed from it.
 */
static void queue_changed(const char attribute((unused)) *event,
                           void  attribute((unused)) *eventdata,
                           void  attribute((unused)) *callbackdata) {
  D(("queue_changed"));
  gtk_label_set_text(GTK_LABEL(report_label), "updating queue");
  disorder_eclient_queue(client, queue_completed, 0);
}

/** @brief Schedule an update to the playing track
 *
 * Called whenever it changes
 */
static void playing_changed(const char attribute((unused)) *event,
                            void  attribute((unused)) *eventdata,
                            void  attribute((unused)) *callbackdata) {
  D(("playing_changed"));
  gtk_label_set_text(GTK_LABEL(report_label), "updating playing track");
  disorder_eclient_playing(client, playing_completed, 0);
}

/** @brief Called regularly
 *
 * Updates the played-so-far field
 */
static gboolean playing_periodic(gpointer attribute((unused)) data) {
  /* If there's a track playing, update its row */
  if(playing_track)
    ql_update_row(playing_track, 0);
  return TRUE;
}

/** @brief Called at startup */
static void queue_init(void) {
  /* Arrange a callback whenever the playing state changes */ 
  event_register("playing-changed", playing_changed, 0);
  /* We reget both playing track and queue at pause/resume so that start times
   * can be computed correctly */
  event_register("pause-changed", playing_changed, 0);
  event_register("pause-changed", queue_changed, 0);
  /* Reget the queue whenever it changes */
  event_register("queue-changed", queue_changed, 0);
  /* ...and once a second anyway */
  g_timeout_add(1000/*ms*/, playing_periodic, 0);
}

/** @brief Columns for the queue */
static const struct queue_column queue_columns[] = {
  { "When",   column_when,     0,        COL_RIGHT },
  { "Who",    column_who,      0,        0 },
  { "Artist", column_namepart, "artist", COL_EXPAND|COL_ELLIPSIZE },
  { "Album",  column_namepart, "album",  COL_EXPAND|COL_ELLIPSIZE },
  { "Title",  column_namepart, "title",  COL_EXPAND|COL_ELLIPSIZE },
  { "Length", column_length,   0,        COL_RIGHT }
};

/** @brief Pop-up menu for queue */
static struct queue_menuitem queue_menuitems[] = {
  { "Track properties", ql_properties_activate, ql_properties_sensitive, 0, 0 },
  { "Select all tracks", ql_selectall_activate, ql_selectall_sensitive, 0, 0 },
  { "Deselect all tracks", ql_selectnone_activate, ql_selectnone_sensitive, 0, 0 },
  { "Scratch playing track", ql_scratch_activate, ql_scratch_sensitive, 0, 0 },
  { "Remove track from queue", ql_remove_activate, ql_remove_sensitive, 0, 0 },
};

struct queuelike ql_queue = {
  .name = "queue",
  .init = queue_init,
  .columns = queue_columns,
  .ncolumns = sizeof queue_columns / sizeof *queue_columns,
  .menuitems = queue_menuitems,
  .nmenuitems = sizeof queue_menuitems / sizeof *queue_menuitems,
};

GtkWidget *queue_widget(void) {
  return init_queuelike(&ql_queue);
}

/** @brief Return nonzero if @p track is in the queue */
int queued(const char *track) {
  struct queue_entry *q;

  D(("queued %s", track));
  for(q = ql_queue.q; q; q = q->next)
    if(!strcmp(q->track, track))
      return 1;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
