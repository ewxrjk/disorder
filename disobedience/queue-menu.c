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

/* Select All */

int ql_selectall_sensitive(struct queuelike *ql) {
  return !!ql->q;
}

void ql_selectall_activate(GtkMenuItem attribute((unused)) *menuitem,
                           gpointer user_data) {
  struct queuelike *ql = user_data;

  gtk_tree_selection_select_all(ql->selection);
}

/* Select None */

int ql_selectnone_sensitive(struct queuelike *ql) {
  return gtk_tree_selection_count_selected_rows(ql->selection) > 0;
}

void ql_selectnone_activate(GtkMenuItem attribute((unused)) *menuitem,
                            gpointer user_data) {
  struct queuelike *ql = user_data;

  gtk_tree_selection_unselect_all(ql->selection);
}

/* Properties */

int ql_properties_sensitive(struct queuelike *ql) {
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

int ql_scratch_sensitive(struct queuelike attribute((unused)) *ql) {
  return !!playing_track;
}

void ql_scratch_activate(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer attribute((unused)) user_data) {
  /* TODO */
}

/* Remove */

static void remove_sensitive_callback(GtkTreeModel *model,
                                      GtkTreePath attribute((unused)) *path,
                                      GtkTreeIter *iter,
                                      gpointer data) {
  struct queuelike *ql = g_object_get_data(G_OBJECT(model), "ql");
  struct queue_entry *q = ql_iter_to_q(ql, iter);
  const int removable = (q != playing_track
                         && right_removable(last_rights, config->username, q));
  int *const counts = data;
  ++counts[removable];
}

int ql_remove_sensitive(struct queuelike *ql) {
  int counts[2] = { 0, 0 };
  gtk_tree_selection_selected_foreach(ql->selection,
                                      remove_sensitive_callback,
                                      counts);
  /* Remove will work if we have at least some removable tracks selected, and
   * no unremovable ones */
  return counts[1] > 0 && counts[0] == 0;
}

static void remove_completed(void attribute((unused)) *v, const char *error) {
  if(error)
    popup_protocol_error(0, error);
}

static void remove_activate_callback(GtkTreeModel *model,
                                     GtkTreePath attribute((unused)) *path,
                                     GtkTreeIter *iter,
                                     gpointer attribute((unused)) data) {
  struct queuelike *ql = g_object_get_data(G_OBJECT(model), "ql");
  struct queue_entry *q = ql_iter_to_q(ql, iter);

  disorder_eclient_remove(client, q->id, remove_completed, q);
}

void ql_remove_activate(GtkMenuItem attribute((unused)) *menuitem,
                        gpointer user_data) {
  struct queuelike *ql = user_data;
  gtk_tree_selection_selected_foreach(ql->selection,
                                      remove_activate_callback,
                                      0);
}

/* Play */

int ql_play_sensitive(struct queuelike *ql) {
  return gtk_tree_selection_count_selected_rows(ql->selection) > 0;
}

void ql_play_activate(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer attribute((unused)) user_data) {
  /* TODO */
}


/** @brief Create @c ql->menu if it does not already exist */
static void ql_create_menu(struct queuelike *ql) {
  if(ql->menu)
    return;
  ql->menu = gtk_menu_new();
  g_signal_connect(ql->menu, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &ql->menu);
  for(int n = 0; n < ql->nmenuitems; ++n) {
    ql->menuitems[n].w = gtk_menu_item_new_with_label(ql->menuitems[n].name);
    gtk_menu_attach(GTK_MENU(ql->menu), ql->menuitems[n].w, 0, 1, n, n + 1);
  }
  set_tool_colors(ql->menu);
}

/** @brief Configure @c ql->menu */
static void ql_configure_menu(struct queuelike *ql) {
  /* Set the sensitivity of each menu item and (re-)establish the signal
   * handlers */
  for(int n = 0; n < ql->nmenuitems; ++n) {
    if(ql->menuitems[n].handlerid)
      g_signal_handler_disconnect(ql->menuitems[n].w,
                                  ql->menuitems[n].handlerid);
    gtk_widget_set_sensitive(ql->menuitems[n].w,
                             ql->menuitems[n].sensitive(ql));
    ql->menuitems[n].handlerid = g_signal_connect
      (ql->menuitems[n].w, "activate",
       G_CALLBACK(ql->menuitems[n].activate), ql);
  }
}

/** @brief Called when a button is released over a queuelike */
gboolean ql_button_release(GtkWidget*widget,
                           GdkEventButton *event,
                           gpointer user_data) {
  struct queuelike *ql = user_data;

  if(event->type == GDK_BUTTON_PRESS
     && event->button == 3) {
    /* Right button click. */
    if(gtk_tree_selection_count_selected_rows(ql->selection) == 0) {
      /* Nothing is selected, select whatever is under the pointer */
      GtkTreePath *path;
      if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                       event->x, event->y,
                                       &path,
                                       NULL,
                                       NULL, NULL)) 
        gtk_tree_selection_select_path(ql->selection, path);
    }
    ql_create_menu(ql);
    ql_configure_menu(ql);
    gtk_widget_show_all(ql->menu);
    gtk_menu_popup(GTK_MENU(ql->menu), 0, 0, 0, 0,
                   event->button, event->time);
    return TRUE;                        /* hide the click from other widgets */
  }

  return FALSE;
}

static int ql_tab_selectall_sensitive(void *extra) {
  return ql_selectall_sensitive(extra);
}
  
static void ql_tab_selectall_activate(void *extra) {
  ql_selectall_activate(NULL, extra);
}
  
static int ql_tab_selectnone_sensitive(void *extra) {
  return ql_selectnone_sensitive(extra);
}
  
static void ql_tab_selectnone_activate(void *extra) {
  ql_selectnone_activate(NULL, extra);
}
  
static int ql_tab_properties_sensitive(void *extra) {
  return ql_properties_sensitive(extra);
}
  
static void ql_tab_properties_activate(void *extra) {
  ql_properties_activate(NULL, extra);
}

struct tabtype *ql_tabtype(struct queuelike *ql) {
  static const struct tabtype ql_tabtype = {
    ql_tab_properties_sensitive,
    ql_tab_selectall_sensitive,
    ql_tab_selectnone_sensitive,
    ql_tab_properties_activate,
    ql_tab_selectall_activate,
    ql_tab_selectnone_activate,
    0,
    0
  };

  struct tabtype *t = xmalloc(sizeof *t);
  *t = ql_tabtype;
  t->extra = ql;
  return t;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
