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
#include "popup.h"
#include "queue-generic.h"

/* Select All */

int ql_selectall_sensitive(void *extra) {
  struct queuelike *ql = extra;
  return !!ql->q;
}

void ql_selectall_activate(GtkMenuItem attribute((unused)) *menuitem,
                           gpointer user_data) {
  struct queuelike *ql = user_data;

  gtk_tree_selection_select_all(ql->selection);
}

/* Select None */

int ql_selectnone_sensitive(void *extra) {
  struct queuelike *ql = extra;
  return gtk_tree_selection_count_selected_rows(ql->selection) > 0;
}

void ql_selectnone_activate(GtkMenuItem attribute((unused)) *menuitem,
                            gpointer user_data) {
  struct queuelike *ql = user_data;

  gtk_tree_selection_unselect_all(ql->selection);
}

/* Properties */

int ql_properties_sensitive(void *extra) {
  struct queuelike *ql = extra;
  return gtk_tree_selection_count_selected_rows(ql->selection) > 0;
}

void ql_properties_activate(GtkMenuItem attribute((unused)) *menuitem,
                            gpointer user_data) {
  struct queuelike *ql = user_data;
  struct vector v[1];
  GtkTreeIter iter[1];

  vector_init(v);
  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql->store), iter);
  for(struct queue_entry *q = ql->q; q; q = q->next) {
    if(gtk_tree_selection_iter_is_selected(ql->selection, iter))
      vector_append(v, (char *)q->track);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(ql->store), iter);
  }
  if(v->nvec)
    properties(v->nvec, (const char **)v->vec);
}

/* Scratch */

int ql_scratch_sensitive(void attribute((unused)) *extra) {
  return !!(last_state & DISORDER_PLAYING)
    && right_scratchable(last_rights, config->username, playing_track);
}

static void ql_scratch_completed(void attribute((unused)) *v,
                                 const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

void ql_scratch_activate(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer attribute((unused)) user_data) {
  disorder_eclient_scratch_playing(client, ql_scratch_completed, 0);
}

/* Remove */

static void ql_remove_sensitive_callback(GtkTreeModel *model,
                                         GtkTreePath attribute((unused)) *path,
                                         GtkTreeIter *iter,
                                         gpointer data) {
  struct queue_entry *q = ql_iter_to_q(model, iter);
  const int removable = (q != playing_track
                         && right_removable(last_rights, config->username, q));
  int *const counts = data;
  ++counts[removable];
}

int ql_remove_sensitive(void *extra) {
  struct queuelike *ql = extra;
  int counts[2] = { 0, 0 };
  gtk_tree_selection_selected_foreach(ql->selection,
                                      ql_remove_sensitive_callback,
                                      counts);
  /* Remove will work if we have at least some removable tracks selected, and
   * no unremovable ones */
  return counts[1] > 0 && counts[0] == 0;
}

static void ql_remove_completed(void attribute((unused)) *v,
                                const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

static void ql_remove_activate_callback(GtkTreeModel *model,
                                        GtkTreePath attribute((unused)) *path,
                                        GtkTreeIter *iter,
                                        gpointer attribute((unused)) data) {
  struct queue_entry *q = ql_iter_to_q(model, iter);

  disorder_eclient_remove(client, q->id, ql_remove_completed, q);
}

void ql_remove_activate(GtkMenuItem attribute((unused)) *menuitem,
                        gpointer user_data) {
  struct queuelike *ql = user_data;
  gtk_tree_selection_selected_foreach(ql->selection,
                                      ql_remove_activate_callback,
                                      0);
}

/* Play */

int ql_play_sensitive(void *extra) {
  struct queuelike *ql = extra;
  return (last_rights & RIGHT_PLAY)
    && gtk_tree_selection_count_selected_rows(ql->selection) > 0;
}

static void ql_play_completed(void attribute((unused)) *v, const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

static void ql_play_activate_callback(GtkTreeModel *model,
                                      GtkTreePath attribute((unused)) *path,
                                      GtkTreeIter *iter,
                                      gpointer attribute((unused)) data) {
  struct queue_entry *q = ql_iter_to_q(model, iter);

  disorder_eclient_play(client, q->track, ql_play_completed, q);
}

void ql_play_activate(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer user_data) {
  struct queuelike *ql = user_data;
  gtk_tree_selection_selected_foreach(ql->selection,
                                      ql_play_activate_callback,
                                      0);
}

/** @brief Called when a button is released over a queuelike */
gboolean ql_button_release(GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data) {
  struct queuelike *ql = user_data;

  if(event->type == GDK_BUTTON_PRESS
     && event->button == 3) {
    /* Right button click. */
    ensure_selected(GTK_TREE_VIEW(widget), event);
    popup(&ql->menu, event, ql->menuitems, ql->nmenuitems, ql);
    return TRUE;                        /* hide the click from other widgets */
  }

  return FALSE;
}

struct tabtype *ql_tabtype(struct queuelike *ql) {
  static const struct tabtype queuelike_tabtype = {
    ql_properties_sensitive,
    ql_selectall_sensitive,
    ql_selectnone_sensitive,
    ql_properties_activate,
    ql_selectall_activate,
    ql_selectnone_activate,
    0,
    0
  };

  ql->tabtype = queuelike_tabtype;
  ql->tabtype.extra = ql;
  return &ql->tabtype;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
