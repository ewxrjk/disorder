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
/** @file disobedience/queue-generic.c
 * @brief Queue widgets
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
 * - random play icon sensitivity is wrong (onl) if changed from edit menu
 * - drag and drop queue rearrangement
 * - display playing row in a different color?
 */
#include "disobedience.h"
#include "queue-generic.h"

static struct queuelike *const queuelikes[] = {
  &ql_queue, &ql_recent, &ql_added
};
#define NQUEUELIKES (sizeof queuelikes / sizeof *queuelikes)

/* Track detail lookup ----------------------------------------------------- */

static int namepart_lookups_outstanding;
static const struct cache_type cachetype_string = { 3600 };
static const struct cache_type cachetype_integer = { 3600 };

/** @brief Called when a namepart lookup has completed or failed
 *
 * When there are no lookups in flight a redraw is provoked.  This might well
 * provoke further lookups.
 */
static void namepart_completed_or_failed(void) {
  --namepart_lookups_outstanding;
  if(!namepart_lookups_outstanding) {
    /* There are no more lookups outstanding, so we update the display */
    for(unsigned n = 0; n < NQUEUELIKES; ++n)
      ql_update_list_store(queuelikes[n]);
  }
}

/** @brief Called when a namepart lookup has completed */
static void namepart_completed(void *v, const char *error, const char *value) {
  D(("namepart_completed"));
  if(error) {
    gtk_label_set_text(GTK_LABEL(report_label), error);
    value = "?";
  }
  const char *key = v;
  
  cache_put(&cachetype_string, key, value);
  namepart_completed_or_failed();
}

/** @brief Called when a length lookup has completed */
static void length_completed(void *v, const char *error, long l) {
  D(("length_completed"));
  if(error) {
    gtk_label_set_text(GTK_LABEL(report_label), error);
    l = -1;
  }
  const char *key = v;
  long *value;
  
  D(("namepart_completed"));
  value = xmalloc(sizeof *value);
  *value = l;
  cache_put(&cachetype_integer, key, value);
  namepart_completed_or_failed();
}

/** @brief Arrange to fill in a namepart cache entry */
static void namepart_fill(const char *track,
                          const char *context,
                          const char *part,
                          const char *key) {
  D(("namepart_fill %s %s %s %s", track, context, part, key));
  /* We limit the total number of lookups in flight */
  ++namepart_lookups_outstanding;
  D(("namepart_lookups_outstanding -> %d\n", namepart_lookups_outstanding));
  disorder_eclient_namepart(client, namepart_completed,
                            track, context, part, (void *)key);
}

/** @brief Look up a namepart
 * @param track Track name
 * @param context Context
 * @param part Name part
 * @param lookup If nonzero, will schedule a lookup for unknown values
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
static const char *namepart(const char *track,
                            const char *context,
                            const char *part) {
  char *key;
  const char *value;

  D(("namepart %s %s %s", track, context, part));
  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  value = cache_get(&cachetype_string, key);
  if(!value) {
    D(("deferring..."));
    namepart_fill(track, context, part, key);
    value = "?";
  }
  return value;
}

/** @brief Called from @ref disobedience/properties.c when we know a name part has changed */
void namepart_update(const char *track,
                     const char *context,
                     const char *part) {
  char *key;

  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  /* Only refetch if it's actually in the cache. */
  if(cache_get(&cachetype_string, key))
    namepart_fill(track, context, part, key);
}

/** @brief Look up a track length
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
static long getlength(const char *track) {
  char *key;
  const long *value;

  D(("getlength %s", track));
  byte_xasprintf(&key, "length track=%s", track);
  value = cache_get(&cachetype_integer, key);
  if(value)
    return *value;
  D(("deferring..."));;
  ++namepart_lookups_outstanding;
  D(("namepart_lookups_outstanding -> %d\n", namepart_lookups_outstanding));
  disorder_eclient_length(client, length_completed, track, key);
  return -1;
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
  l = getlength(q->track);
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
      time(&now);
      l = playing_track->sofar + (now - last_playing);
    }
    byte_xasprintf(&played, "%ld:%02ld/%s", l / 60, l % 60, length);
    return played;
  } else
    return length;
}

/* Selection processing ---------------------------------------------------- */

/** @brief Stash the selection of @c ql->view
 * @param ql Queuelike of interest
 * @return Hash representing current selection
 */
static hash *save_selection(struct queuelike *ql) {
  hash *h = hash_new(1);
  GtkTreeIter iter[1];
  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store), iter);
  for(const struct queue_entry *q = ql->q; q; q = q->next) {
    if(gtk_tree_selection_iter_is_selected(ql->selection, iter))
      hash_add(h, q->id, "", HASH_INSERT);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
  }
  return h;
}

/** @brief Called from restore_selection() */
static int restore_selection_callback(const char *key,
                                      void attribute((unused)) *value,
                                      void *u) {
  const struct queuelike *const ql = u;
  GtkTreeIter iter[1];
  const struct queue_entry *q;
  
  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store), iter);
  for(q = ql->q; q && strcmp(key, q->id); q = q->next)
    gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
  if(q) 
    gtk_tree_selection_select_iter(ql->selection, iter);
  /* There might be gaps if things have disappeared */
  return 0;
}

/** @brief Restore selection of @c ql->view
 * @param ql Queuelike of interest
 * @param h Hash representing former selection
 */
static void restore_selection(struct queuelike *ql, hash *h) {
  hash_foreach(h, restore_selection_callback, ql);
}

/* List store maintenance -------------------------------------------------- */

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
  for(int col = 0; col < ql->ncolumns; ++col)
    gtk_list_store_set(ql->store, iter,
                       col, ql->columns[col].value(q,
                                                   ql->columns[col].data),
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

/** @brief Reset the list store
 * @param ql Queuelike to reset
 *
 * Throws away all rows and starts again.  Used when new queue contents arrives
 * from the server.
 */
void ql_new_queue(struct queuelike *ql,
                  struct queue_entry *newq) {
  D(("ql_new_queue"));
  hash *h = save_selection(ql);
  /* Clear out old contents */
  gtk_list_store_clear(ql->store);
  /* Put in new rows */
  ql->q = newq;
  for(struct queue_entry *q = ql->q; q; q = q->next) {
    /* Tell every queue entry which queue owns it */
    q->ql = ql;
    /* Add a row */
    GtkTreeIter iter[1];
    gtk_list_store_append(ql->store, iter);
    /* Update the row contents */
    ql_update_row(q, iter);
  }
  restore_selection(ql, h);
  /* Update menu sensitivity */
  menu_update(-1);
}

/** @brief Initialize a @ref queuelike */
GtkWidget *init_queuelike(struct queuelike *ql) {
  D(("init_queuelike"));
  /* Create the list store */
  GType *types = xcalloc(ql->ncolumns, sizeof (GType));
  for(int n = 0; n < ql->ncolumns; ++n)
    types[n] = G_TYPE_STRING;
  ql->store = gtk_list_store_newv(ql->ncolumns, types);

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
       (char *)0);
    g_object_set(c, "resizable", TRUE, (char *)0);
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

  /* TODO style? */
  /* TODO drag+drop */

  ql->init();

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
