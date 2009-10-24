/*
 * This file is part of DisOrder
 * Copyright (C) 2006-2008 Richard Kettlewell
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
/** @file disobedience/queue.c
 * @brief Disobedience queue widget
 */
#include "disobedience.h"
#include "popup.h"
#include "queue-generic.h"

/** @brief The actual queue */
static struct queue_entry *actual_queue;
static struct queue_entry *actual_playing_track;

/** @brief The playing track */
struct queue_entry *playing_track;

/** @brief When we last got the playing track
 *
 * Set to 0 if the timings are currently off due to having just unpaused.
 */
time_t last_playing;

static void queue_completed(void *v,
                            const char *err,
                            struct queue_entry *q);
static void playing_completed(void *v,
                              const char *err,
                              struct queue_entry *q);

/** @brief Called when either the actual queue or the playing track change */
static void queue_playing_changed(void) {
  /* Check that the playing track isn't in the queue.  There's a race here due
   * to the fact that we issue the two commands at slightly different times.
   * If it goes wrong we re-issue and try again, so that we never offer up an
   * inconsistent state. */
  if(actual_playing_track) {
    struct queue_entry *q;
    for(q = actual_queue; q; q = q->next)
      if(!strcmp(q->id, actual_playing_track->id))
        break;
    if(q) {
      disorder_eclient_playing(client, playing_completed, 0);
      disorder_eclient_queue(client, queue_completed, 0);
      return;
    }
  }
  
  struct queue_entry *q = xmalloc(sizeof *q);
  if(actual_playing_track) {
    *q = *actual_playing_track;
    q->next = actual_queue;
    playing_track = q;
  } else {
    playing_track = NULL;
    q = actual_queue;
  }
  ql_new_queue(&ql_queue, q);
  /* Tell anyone who cares */
  event_raise("queue-list-changed", q);
  event_raise("playing-track-changed", q);
}

/** @brief Update the queue itself */
static void queue_completed(void attribute((unused)) *v,
                            const char *err,
                            struct queue_entry *q) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  actual_queue = q;
  queue_playing_changed();
}

/** @brief Update the playing track */
static void playing_completed(void attribute((unused)) *v,
                              const char *err,
                              struct queue_entry *q) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  actual_playing_track = q;
  queue_playing_changed();
  xtime(&last_playing);
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
  /* Setting last_playing=0 means that we don't know what the correct value
   * is right now, e.g. because things have been deranged by a pause. */
  last_playing = 0;
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

static void queue_move_completed(void attribute((unused)) *v,
                                 const char *err) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  /* The log should tell us the queue changed so we do no more here */
}

/** @brief Called when drag+drop completes */
static void queue_drop(int src, int dst) {
  struct queue_entry *sq, *dq;
  int n;

  //fprintf(stderr, "queue_drop %d -> %d\n", src, dst);
  if(playing_track) {
    /* If there's a playing track then you can't drag it anywhere  */
    if(src == 0) {
      //fprintf(stderr, "cannot drag playing track\n");
      queue_playing_changed();
      return;
    }
    /* If you try to drop before the playing track we assume you missed and
     * mean after instead */
    if(!dst)
      dst = 1;
    //fprintf(stderr, "...adjusted to %d -> %d\n\n", src, dst);
  }
  /* Find the entry to move */
  for(n = 0, sq = ql_queue.q; n < src; ++n)
    sq = sq->next;
  /*fprintf(stderr, "source=%s (%s)\n",
          sq->id, sq->track);*/
  const int after = dst - 1;
  if(after == -1)
    dq = 0;
  else
    /* Find the entry to insert after */
    for(n = 0, dq = ql_queue.q; n < after; ++n)
      dq = dq->next;
  if(dq == playing_track)
    dq = 0;
#if 0
  if(dq)
    fprintf(stderr, "after=%s (%s)\n",
            dq->id, dq->track);
  else
    fprintf(stderr, "after=NULL\n");
#endif
  disorder_eclient_moveafter(client,
                             dq ? dq->id : "",
                             1, &sq->id,
                             queue_move_completed, NULL);
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
static struct menuitem queue_menuitems[] = {
  { "Track properties", ql_properties_activate, ql_properties_sensitive, 0, 0 },
  { "Select all tracks", ql_selectall_activate, ql_selectall_sensitive, 0, 0 },
  { "Deselect all tracks", ql_selectnone_activate, ql_selectnone_sensitive, 0, 0 },
  { "Scratch playing track", ql_scratch_activate, ql_scratch_sensitive, 0, 0 },
  { "Remove track from queue", ql_remove_activate, ql_remove_sensitive, 0, 0 },
  { "Adopt track", ql_adopt_activate, ql_adopt_sensitive, 0, 0 },
};

struct queuelike ql_queue = {
  .name = "queue",
  .init = queue_init,
  .columns = queue_columns,
  .ncolumns = sizeof queue_columns / sizeof *queue_columns,
  .menuitems = queue_menuitems,
  .nmenuitems = sizeof queue_menuitems / sizeof *queue_menuitems,
  .drop = queue_drop
};

/** @brief Called when a key is pressed in the queue tree view */
static gboolean queue_key_press(GtkWidget attribute((unused)) *widget,
                                GdkEventKey *event,
                                gpointer user_data) {
  /*fprintf(stderr, "queue_key_press type=%d state=%#x keyval=%#x\n",
          event->type, event->state, event->keyval);*/
  switch(event->keyval) {
  case GDK_BackSpace:
  case GDK_Delete:
    if(event->state)
      break;                            /* Only take unmodified DEL/<-- */
    ql_remove_activate(0, user_data);
    return TRUE;                        /* Do not propagate */
  }
  return FALSE;                         /* Propagate */
}

GtkWidget *queue_widget(void) {
  GtkWidget *const w = init_queuelike(&ql_queue);

  /* Catch keypresses */
  g_signal_connect(ql_queue.view, "key-press-event",
                   G_CALLBACK(queue_key_press), &ql_queue);
  return w;
}

/** @brief Return nonzero if @p track is in the queue */
int queued(const char *track) {
  struct queue_entry *q;

  D(("queued %s", track));
  /* Queue will contain resolved name */
  track = namepart_resolve(track);
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
