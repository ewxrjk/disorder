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
/** @file disobedience/queue-generic.c
 * @brief Disobedience queue widgets
 *
 * This file provides contains code shared between all the queue-like
 * widgets - the queue, the recent list and the added tracks list.
 *
 * This code is in the process of being rewritten to use the native list
 * widget.
 *
 * There are three @ref queuelike objects: @ref ql_queue, @ref
 * ql_recent and @ref ql_added.  Each has an associated queue linked
 * list and a list store containing the contents.
 *
 * When new contents turn up we rearrange the list store accordingly.
 *
 * NB that while in the server the playing track is not in the queue, in
 * Disobedience, the playing does live in @c ql_queue.q, despite its different
 * status to everything else found in that list.
 *
 * To do:
 * - display playing row in a different color?
 */
#include "disobedience.h"
#include "popup.h"
#include "queue-generic.h"

/* Track detail lookup ----------------------------------------------------- */

static void queue_lookups_completed(const char attribute((unused)) *event,
                                   void attribute((unused)) *eventdata,
                                   void *callbackdata) {
  struct queuelike *ql = callbackdata;
  ql_update_list_store(ql);
}

/* Column formatting -------------------------------------------------------- */

/** @brief Format the 'when' column */
const char *column_when(const struct queue_entry *q,
                        const char attribute((unused)) *data) {
  char when[64];
  struct tm tm;
  time_t t;

  D(("column_when"));
  switch(q->state) {
  case playing_isscratch:
  case playing_unplayed:
  case playing_random:
    t = q->expected;
    break;
  case playing_failed:
  case playing_no_player:
  case playing_ok:
  case playing_scratched:
  case playing_started:
  case playing_paused:
  case playing_quitting:
    t = q->played;
    break;
  default:
    t = 0;
    break;
  }
  if(t)
    strftime(when, sizeof when, "%H:%M", localtime_r(&t, &tm));
  else
    when[0] = 0;
  return xstrdup(when);
}

/** @brief Format the 'who' column */
const char *column_who(const struct queue_entry *q,
                       const char attribute((unused)) *data) {
  D(("column_who"));
  return q->submitter ? q->submitter : "";
}

/** @brief Format one of the track name columns */
const char *column_namepart(const struct queue_entry *q,
                            const char *data) {
  D(("column_namepart"));
  return namepart(q->track, "display", data);
}

/** @brief Format the length column */
const char *column_length(const struct queue_entry *q,
                          const char attribute((unused)) *data) {
  D(("column_length"));
  long l;
  time_t now;
  char *played = 0, *length = 0;

  /* Work out what to say for the length */
  l = namepart_length(q->track);
  if(l > 0)
    byte_xasprintf(&length, "%ld:%02ld", l / 60, l % 60);
  else
    byte_xasprintf(&length, "?:??");
  /* For the currently playing track we want to report how much of the track
   * has been played */
  if(q == playing_track) {
    /* log_state() arranges that we re-get the playing data whenever the
     * pause/resume state changes */
    if(last_state & DISORDER_TRACK_PAUSED)
      l = playing_track->sofar;
    else {
      if(!last_playing)
        return NULL;
      xtime(&now);
      l = playing_track->sofar + (now - last_playing);
    }
    byte_xasprintf(&played, "%ld:%02ld/%s", l / 60, l % 60, length);
    return played;
  } else
    return length;
}

/* List store maintenance -------------------------------------------------- */

/** @brief Return the @ref queue_entry corresponding to @p iter
 * @param model Model that owns @p iter
 * @param iter Tree iterator
 * @return Pointer to queue entry
 */
struct queue_entry *ql_iter_to_q(GtkTreeModel *model,
                                 GtkTreeIter *iter) {
  struct queuelike *ql = g_object_get_data(G_OBJECT(model), "ql");
  GValue v[1];
  memset(v, 0, sizeof v);
  gtk_tree_model_get_value(model, iter, ql->ncolumns + QUEUEPOINTER_COLUMN, v);
  assert(G_VALUE_TYPE(v) == G_TYPE_POINTER);
  struct queue_entry *const q = g_value_get_pointer(v);
  g_value_unset(v);
  return q;
}

/** @brief Update one row of a list store
 * @param q Queue entry
 * @param iter Iterator referring to row or NULL to work it out
 */
void ql_update_row(struct queue_entry *q,
                   GtkTreeIter *iter) { 
  const struct queuelike *const ql = q->ql; 

  D(("ql_update_row"));
  /* If no iter was supplied, work it out */
  GtkTreeIter my_iter[1];
  if(!iter) {
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store), my_iter);
    struct queue_entry *qq;
    for(qq = ql->q; qq && q != qq; qq = qq->next)
      gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), my_iter);
    if(!qq)
      return;
    iter = my_iter;
  }
  /* Update all the columns */
  for(int col = 0; col < ql->ncolumns; ++col) {
    const char *const v = ql->columns[col].value(q,
                                                 ql->columns[col].data);
    if(v)
      gtk_list_store_set(ql->store, iter,
                         col, v,
                         -1);
  }
  gtk_list_store_set(ql->store, iter,
                     ql->ncolumns + QUEUEPOINTER_COLUMN, q,
                     -1);
  if(q == playing_track)
    gtk_list_store_set(ql->store, iter,
                       ql->ncolumns + BACKGROUND_COLUMN, BG_PLAYING,
                       ql->ncolumns + FOREGROUND_COLUMN, FG_PLAYING,
                       -1);
  else
    gtk_list_store_set(ql->store, iter,
                       ql->ncolumns + BACKGROUND_COLUMN, (char *)0,
                       ql->ncolumns + FOREGROUND_COLUMN, (char *)0,
                       -1);
}

/** @brief Update the list store
 * @param ql Queuelike to update
 *
 * Called when new namepart data is available (and initially).  Doesn't change
 * the rows, just updates the cell values.
 */
void ql_update_list_store(struct queuelike *ql) {
  D(("ql_update_list_store"));
  GtkTreeIter iter[1];
  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store), iter);
  for(struct queue_entry *q = ql->q; q; q = q->next) {
    ql_update_row(q, iter);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
  }
}

struct newqueue_data {
  struct queue_entry *old, *new;
};

static void record_queue_map(hash *h,
                             const char *id,
                             struct queue_entry *old,
                             struct queue_entry *new) {
  struct newqueue_data *nqd;

  if(!(nqd = hash_find(h, id))) {
    static const struct newqueue_data empty[1];
    hash_add(h, id, empty, HASH_INSERT);
    nqd = hash_find(h, id);
  }
  if(old)
    nqd->old = old;
  if(new)
    nqd->new = new;
}

#if 0
static void dump_queue(struct queue_entry *head, struct queue_entry *mark) {
  for(struct queue_entry *q = head; q; q = q->next) {
    if(q == mark)
      fprintf(stderr, "!");
    fprintf(stderr, "%s", q->id);
    if(q->next)
      fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");
}

static void dump_rows(struct queuelike *ql) {
  GtkTreeIter iter[1];
  gboolean it = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store),
                                              iter);
  while(it) {
    struct queue_entry *q = ql_iter_to_q(GTK_TREE_MODEL(ql->store), iter);
    it = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
    fprintf(stderr, "%s", q->id);
    if(it)
      fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");
}
#endif

/** @brief Reset the list store
 * @param ql Queuelike to reset
 * @param newq New queue contents/ordering
 *
 * Updates the queue to match @p newq
 */
void ql_new_queue(struct queuelike *ql,
                  struct queue_entry *newq) {
  D(("ql_new_queue"));
  ++suppress_actions;

  /* Tell every queue entry which queue owns it */
  //fprintf(stderr, "%s: filling in q->ql\n", ql->name);
  for(struct queue_entry *q = newq; q; q = q->next)
    q->ql = ql;

  //fprintf(stderr, "%s: constructing h\n", ql->name);
  /* Construct map from id to new and old structures */
  hash *h = hash_new(sizeof(struct newqueue_data));
  for(struct queue_entry *q = ql->q; q; q = q->next)
    record_queue_map(h, q->id, q, NULL);
  for(struct queue_entry *q = newq; q; q = q->next)
    record_queue_map(h, q->id, NULL, q);

  /* The easy bit: delete rows not present any more.  In the same pass we
   * update the secret column containing the queue_entry pointer. */
  //fprintf(stderr, "%s: deleting rows...\n", ql->name);
  GtkTreeIter iter[1];
  gboolean it = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store),
                                              iter);
  int inserted = 0, deleted = 0, kept = 0;
  while(it) {
    struct queue_entry *q = ql_iter_to_q(GTK_TREE_MODEL(ql->store), iter);
    const struct newqueue_data *nqd = hash_find(h, q->id);
    if(nqd->new) {
      /* Tell this row that it belongs to the new version of the queue */
      gtk_list_store_set(ql->store, iter,
                         ql->ncolumns + QUEUEPOINTER_COLUMN, nqd->new,
                         -1);
      it = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
      ++kept;
    } else {
      /* Delete this row (and move iter to the next one) */
      //fprintf(stderr, " delete %s", q->id);
      it = gtk_list_store_remove(ql->store, iter);
      ++deleted;
    }
  }

  /* Now every row's secret column is right, but we might be missing new rows
   * and they might be in the wrong order */

  /* We're going to have to support arbitrary rearrangements, so we might as
   * well add new elements at the end. */
  //fprintf(stderr, "%s: adding rows...\n", ql->name);
  struct queue_entry *after = 0;
  for(struct queue_entry *q = newq; q; q = q->next) {
    const struct newqueue_data *nqd = hash_find(h, q->id);
    if(!nqd->old) {
      if(after) {
        /* Try to insert at the right sort of place */
        GtkTreeIter where[1];
        gboolean wit = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store),
                                                     where);
        while(wit && ql_iter_to_q(GTK_TREE_MODEL(ql->store), where) != after)
          wit = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), where);
        if(wit)
          gtk_list_store_insert_after(ql->store, iter, where);
        else
          gtk_list_store_append(ql->store, iter);
      } else
        gtk_list_store_prepend(ql->store, iter);
      gtk_list_store_set(ql->store, iter,
                         ql->ncolumns + QUEUEPOINTER_COLUMN, q,
                         -1);
      //fprintf(stderr, " add %s", q->id);
      ++inserted;
    }
    after = newq;
  }

  /* Now exactly the right set of rows are present, and they have the right
   * queue_entry pointers in their secret column, but they may be in the wrong
   * order.
   *
   * The current code is simple but amounts to a bubble-sort - we might easily
   * called gtk_tree_model_iter_next a couple of thousand times.
   */
  //fprintf(stderr, "%s: rearranging rows\n", ql->name);
  //fprintf(stderr, "%s: queue state: ", ql->name);
  //dump_queue(newq, 0);
  //fprintf(stderr, "%s: row state: ", ql->name);
  //dump_rows(ql);
  it = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store),
                                              iter);
  struct queue_entry *rq = newq;        /* r for 'right, correct' */
  int swaps = 0, searches = 0;
  while(it) {
    struct queue_entry *q = ql_iter_to_q(GTK_TREE_MODEL(ql->store), iter);
    //fprintf(stderr, " rq = %p, q = %p\n", rq, q);
    //fprintf(stderr, " rq->id = %s, q->id = %s\n", rq->id, q->id);

    if(q != rq) {
      //fprintf(stderr, "  mismatch\n");
      GtkTreeIter next[1] = { *iter };
      gboolean nit = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), next);
      while(nit) {
        struct queue_entry *nq = ql_iter_to_q(GTK_TREE_MODEL(ql->store), next);
        //fprintf(stderr, "   candidate: %s\n", nq->id);
        if(nq == rq)
          break;
        nit = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), next);
        ++searches;
      }
      assert(nit);
      //fprintf(stderr, "  found it\n");
      gtk_list_store_swap(ql->store, iter, next);
      *iter = *next;
      //fprintf(stderr, "%s: new row state: ", ql->name);
      //dump_rows(ql);
      ++swaps;
    }
    /* ...and onto the next one */
    it = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
    rq = rq->next;
  }
#if 0
  fprintf(stderr, "%6s: %3d kept %3d inserted %3d deleted %3d swaps %4d searches\n", ql->name,
          kept, inserted, deleted, swaps, searches);
#endif
  //fprintf(stderr, "done\n");
  ql->q = newq;
  /* Set the rest of the columns in new rows */
  ql_update_list_store(ql);
  --suppress_actions;
}

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

/** @brief row-inserted callback */
static void ql_row_inserted(GtkTreeModel attribute((unused)) *treemodel,
                            GtkTreePath *path,
                            GtkTreeIter attribute((unused)) *iter,
                            gpointer user_data) {
  struct queuelike *const ql = user_data;
  if(!suppress_actions) {
#if 0
    char *ps = gtk_tree_path_to_string(path);
    GtkTreeIter piter[1];
    gboolean pi = gtk_tree_model_get_iter(treemodel, piter, path);
    struct queue_entry *pq = pi ? ql_iter_to_q(treemodel, piter) : 0;
    struct queue_entry *iq = ql_iter_to_q(treemodel, iter);

    fprintf(stderr, "row-inserted %s path=%s pi=%d pq=%p path=%s iq=%p iter=%s\n",
            ql->name,
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
      jt = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), j);
    }
    g_free(ps);
#endif
    /* Remember an iterator pointing at the insertion target */
    if(ql->drag_target)
      gtk_tree_path_free(ql->drag_target);
    ql->drag_target = gtk_tree_path_copy(path);
  }
}

/** @brief row-deleted callback */
static void ql_row_deleted(GtkTreeModel attribute((unused)) *treemodel,
                           GtkTreePath *path,
                           gpointer user_data) {
  struct queuelike *const ql = user_data;

  if(!suppress_actions) {
#if 0
    char *ps = gtk_tree_path_to_string(path);
    fprintf(stderr, "row-deleted %s path=%s ql->drag_target=%s\n",
            ql->name, ps, gtk_tree_path_to_string(ql->drag_target));
    GtkTreeIter j[1];
    gboolean jt = gtk_tree_model_get_iter_first(treemodel, j);
    int row = 0;
    while(jt) {
      struct queue_entry *q = ql_iter_to_q(treemodel, j);
      fprintf(stderr, " %2d %s\n", row++, q ? q->track : "(no q)");
      jt = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), j);
    }
    g_free(ps);
#endif
    if(!ql->drag_target) {
      error(0, "%s: unsuppressed row-deleted with no row-inserted",
            ql->name);
      return;
    }

    /* Get the source and destination row numbers. */
    int srcrow = gtk_tree_path_get_indices(path)[0];
    int dstrow = gtk_tree_path_get_indices(ql->drag_target)[0];
    //fprintf(stderr, "srcrow=%d dstrow=%d\n", srcrow, dstrow);

    /* Note that the source row is computed AFTER the destination has been
     * inserted, since GTK+ does the insert before the delete.  Therefore if
     * the source row is south (higher row number) of the destination, it will
     * be one higher than expected.
     *
     * For instance if we drag row 1 to before row 0 we will see row-inserted
     * for row 0 but then a row-deleted for row 2.
     */
    if(srcrow > dstrow)
      --srcrow;

    /* Tell the queue implementation */
    ql->drop(ql, srcrow, dstrow);

    /* Dispose of stashed data */
    gtk_tree_path_free(ql->drag_target);
    ql->drag_target = 0;
  }
}

/** @brief Initialize a @ref queuelike */
GtkWidget *init_queuelike(struct queuelike *ql) {
  D(("init_queuelike"));
  /* Create the list store.  We add an extra column to hold a pointer to the
   * queue_entry. */
  GType *types = xcalloc(ql->ncolumns + EXTRA_COLUMNS, sizeof (GType));
  for(int n = 0; n < ql->ncolumns + EXTRA_COLUMNS; ++n)
    types[n] = G_TYPE_STRING;
  types[ql->ncolumns + QUEUEPOINTER_COLUMN] = G_TYPE_POINTER;
  ql->store = gtk_list_store_newv(ql->ncolumns + EXTRA_COLUMNS, types);
  g_object_set_data(G_OBJECT(ql->store), "ql", (void *)ql);

  /* Create the view */
  ql->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ql->store));
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(ql->view), TRUE);

  /* Create cell renderers and label columns */
  for(int n = 0; n < ql->ncolumns; ++n) {
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    if(ql->columns[n].flags & COL_ELLIPSIZE)
      g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, (char *)0);
    if(ql->columns[n].flags & COL_RIGHT)
      g_object_set(r, "xalign", (gfloat)1.0, (char *)0);
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes
      (ql->columns[n].name,
       r,
       "text", n,
       "background", ql->ncolumns + BACKGROUND_COLUMN,
       "foreground", ql->ncolumns + FOREGROUND_COLUMN,
       (char *)0);
    gtk_tree_view_column_set_resizable(c, TRUE);
    gtk_tree_view_column_set_reorderable(c, TRUE);
    if(ql->columns[n].flags & COL_EXPAND)
      g_object_set(c, "expand", TRUE, (char *)0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ql->view), c);
  }

  /* The selection should support multiple things being selected */
  ql->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ql->view));
  gtk_tree_selection_set_mode(ql->selection, GTK_SELECTION_MULTIPLE);

  /* Catch button presses */
  g_signal_connect(ql->view, "button-press-event",
                   G_CALLBACK(ql_button_release), ql);

  /* Drag+drop*/
  if(ql->drop) {
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ql->view), TRUE);
    g_signal_connect(ql->store,
                     "row-inserted",
                     G_CALLBACK(ql_row_inserted), ql);
    g_signal_connect(ql->store,
                     "row-deleted",
                     G_CALLBACK(ql_row_deleted), ql);
  }
  
  /* TODO style? */

  ql->init(ql);

  /* Update display text when lookups complete */
  event_register("lookups-completed", queue_lookups_completed, ql);
  
  GtkWidget *scrolled = scroll_widget(ql->view);
  g_object_set_data(G_OBJECT(scrolled), "type", (void *)ql_tabtype(ql));
  return scrolled;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
