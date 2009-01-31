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
  time(&last_playing);
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
  .nmenuitems = sizeof queue_menuitems / sizeof *queue_menuitems
};

/* Drag and drop has to be figured out experimentally, because it is not well
 * documented.
 *
 * First you get a row-inserted.  The path argument points to the destination
 * row but this will not yet have had its values set.  The source row is still
 * present.  AFAICT the iter argument points to the same place.
 *
 * Then you get a row-deleted.  The path argument identifies the row that was
 * deleted.  By this stage the row inserted above has acquired its values.
 *
 * A complication is that the deletion will move the inserted row.  For
 * instance, if you do a drag that moves row 1 down to after the track that was
 * formerly on row 9, in the row-inserted call it will show up as row 10, but
 * in the row-deleted call, row 1 will have been deleted thus making the
 * inserted row be row 9.
 *
 * So when we see the row-inserted we have no idea what track to move.
 * Therefore we stash it until we see a row-deleted.
 */

/** @brief Target row for drag */
static int queue_drag_target = -1;

static void queue_move_completed(void attribute((unused)) *v,
                                 const char *err) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  /* The log should tell us the queue changed so we do no more here */
}

static void queue_row_deleted(GtkTreeModel *treemodel,
                              GtkTreePath *path,
                              gpointer attribute((unused)) user_data) {
  if(!suppress_actions) {
#if 0
    char *ps = gtk_tree_path_to_string(path);
    fprintf(stderr, "row-deleted path=%s queue_drag_target=%d\n",
            ps, queue_drag_target);
    GtkTreeIter j[1];
    gboolean jt = gtk_tree_model_get_iter_first(treemodel, j);
    int row = 0;
    while(jt) {
      struct queue_entry *q = ql_iter_to_q(treemodel, j);
      fprintf(stderr, " %2d %s\n", row++, q ? q->track : "(no q)");
      jt = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql_queue.store), j);
    }
    g_free(ps);
#endif
    if(queue_drag_target < 0) {
      error(0, "unsuppressed row-deleted with no row-inserted");
      return;
    }
    int drag_source = gtk_tree_path_get_indices(path)[0];

    /* If the drag is downwards (=towards higher row numbers) then the target
     * will have been moved upwards (=towards lower row numbers) by one row. */
    if(drag_source < queue_drag_target)
      --queue_drag_target;
    
    /* Find the track to move */
    GtkTreeIter src[1];
    gboolean srcv = gtk_tree_model_iter_nth_child(treemodel, src, NULL,
                                                  queue_drag_target);
    if(!srcv) {
      error(0, "cannot get iterator to drag target %d", queue_drag_target);
      queue_playing_changed();
      queue_drag_target = -1;
      return;
    }
    struct queue_entry *srcq = ql_iter_to_q(treemodel, src);
    assert(srcq);
    //fprintf(stderr, "move %s %s\n", srcq->id, srcq->track);
    
    /* Don't allow the currently playing track to be moved.  As above, we put
     * the queue back into the right order straight away. */
    if(srcq == playing_track) {
      //fprintf(stderr, "cannot move currently playing track\n");
      queue_playing_changed();
      queue_drag_target = -1;
      return;
    }

    /* Find the destination */
    struct queue_entry *dstq;
    if(queue_drag_target) {
      GtkTreeIter dst[1];
      gboolean dstv = gtk_tree_model_iter_nth_child(treemodel, dst, NULL,
                                                    queue_drag_target - 1);
      if(!dstv) {
        error(0, "cannot get iterator to drag target predecessor %d",
              queue_drag_target - 1);
        queue_playing_changed();
        queue_drag_target = -1;
        return;
      }
      dstq = ql_iter_to_q(treemodel, dst);
      assert(dstq);
      if(dstq == playing_track)
        dstq = 0;
    } else
      dstq = 0;
    /* NB if the user attempts to move a queued track before the currently
     * playing track we assume they just missed a bit, and put it after. */
    //fprintf(stderr, " target %s %s\n", dstq ? dstq->id : "(none)", dstq ? dstq->track : "(none)");
    /* Now we know what is to be moved.  We need to know the preceding queue
     * entry so we can move it. */
    disorder_eclient_moveafter(client,
                               dstq ? dstq->id : "",
                               1, &srcq->id,
                               queue_move_completed, NULL);
    queue_drag_target = -1;
  }
}

static void queue_row_inserted(GtkTreeModel attribute((unused)) *treemodel,
                               GtkTreePath *path,
                               GtkTreeIter attribute((unused)) *iter,
                               gpointer attribute((unused)) user_data) {
  if(!suppress_actions) {
#if 0
    char *ps = gtk_tree_path_to_string(path);
    GtkTreeIter piter[1];
    gboolean pi = gtk_tree_model_get_iter(treemodel, piter, path);
    struct queue_entry *pq = pi ? ql_iter_to_q(treemodel, piter) : 0;
    struct queue_entry *iq = ql_iter_to_q(treemodel, iter);

    fprintf(stderr, "row-inserted path=%s pi=%d pq=%p path=%s iq=%p iter=%s\n",
            ps,
            pi,
            pq,
            (pi
             ? (pq ? pq->track : "(pq=0)")
             : "(pi=FALSE)"),
            iq,
            iq ? iq->track : "(iq=0)");

    GtkTreeIter j[1];
    gboolean jt = gtk_tree_model_get_iter_first(treemodel, j);
    int row = 0;
    while(jt) {
      struct queue_entry *q = ql_iter_to_q(treemodel, j);
      fprintf(stderr, " %2d %s\n", row++, q ? q->track : "(no q)");
      jt = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql_queue.store), j);
    }
    g_free(ps);
#endif
    queue_drag_target = gtk_tree_path_get_indices(path)[0];
  }
}

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

  /* Enable drag+drop */
  gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ql_queue.view), TRUE);
  g_signal_connect(ql_queue.store,
                   "row-inserted",
                   G_CALLBACK(queue_row_inserted), &ql_queue);
  g_signal_connect(ql_queue.store,
                   "row-deleted",
                   G_CALLBACK(queue_row_deleted), &ql_queue);
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
