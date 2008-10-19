/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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
#include "disobedience.h"
#include "popup.h"
#include "choose.h"

/** @brief Popup menu */
static GtkWidget *choose_menu;

/** @brief Recursion step for choose_get_visible()
 * @param parent A visible node, or NULL for the root
 * @param callback Called for each visible node
 * @param userdata Passed to @p callback
 *
 * If @p callback returns nonzero, the walk stops immediately.
 */
static int choose_visible_recurse(GtkTreeIter *parent,
                                  int (*callback)(GtkTreeIter *it,
                                                  int isfile,
                                                  void *userdata),
                                  void *userdata) {
  int expanded;
  if(parent) {
    /* Skip placeholders */
    if(choose_is_placeholder(parent))
      return 0;
    const int isfile = choose_is_file(parent);
    if(callback(parent, isfile, userdata))
      return 1;
    if(isfile)
      return 0;                 /* Files never have children */
    GtkTreePath *parent_path
      = gtk_tree_model_get_path(GTK_TREE_MODEL(choose_store),
                                parent);
    expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(choose_view),
                                          parent_path);
    gtk_tree_path_free(parent_path);
  } else
    expanded = 1;
  /* See if parent is expanded */
  if(expanded) {
    /* Parent is expanded, visit all its children */
    GtkTreeIter it[1];
    gboolean itv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                                it,
                                                parent);
    while(itv) {
      if(choose_visible_recurse(it, callback, userdata))
        return TRUE;
      itv = gtk_tree_model_iter_next(GTK_TREE_MODEL(choose_store), it);
    }
  }
  return 0;
}

static void choose_visible_visit(int (*callback)(GtkTreeIter *it,
                                                 int isfile,
                                                 void *userdata),
                                 void *userdata) {
  choose_visible_recurse(NULL, callback, userdata);
}

static int choose_selectall_sensitive_callback
   (GtkTreeIter attribute((unused)) *it,
     int isfile,
     void *userdata) {
  if(isfile) {
    *(int *)userdata = 1;
    return 1;
  }
  return 0;
}

/** @brief Should 'select all' be sensitive?
 *
 * Yes if there are visible files.
 */
static int choose_selectall_sensitive(void attribute((unused)) *extra) {
  int files = 0;
  choose_visible_visit(choose_selectall_sensitive_callback, &files);
  return files > 0;
}

static int choose_selectall_activate_callback
    (GtkTreeIter *it,
     int isfile,
     void attribute((unused)) *userdata) {
  if(isfile)
    gtk_tree_selection_select_iter(choose_selection, it);
  else
    gtk_tree_selection_unselect_iter(choose_selection, it);
  return 0;
}

/** @brief Activate select all
 *
 * Selects all files and deselects everything else.
 */
static void choose_selectall_activate(GtkMenuItem attribute((unused)) *item,
                                      gpointer attribute((unused)) userdata) {
  choose_visible_visit(choose_selectall_activate_callback, 0);
}

/** @brief Should 'select none' be sensitive
 *
 * Yes if anything is selected.
 */
static int choose_selectnone_sensitive(void attribute((unused)) *extra) {
  return gtk_tree_selection_count_selected_rows(choose_selection) > 0;
}

/** @brief Activate select none */
static void choose_selectnone_activate(GtkMenuItem attribute((unused)) *item,
                                       gpointer attribute((unused)) userdata) {
  gtk_tree_selection_unselect_all(choose_selection);
}

static void choose_play_sensitive_callback(GtkTreeModel attribute((unused)) *model,
                                           GtkTreePath attribute((unused)) *path,
                                           GtkTreeIter *iter,
                                           gpointer data) {
  int *filesp = data;

  if(*filesp == -1)
    return;
  if(choose_is_dir(iter))
    *filesp = -1;
  else if(choose_is_file(iter))
    ++*filesp;
}

/** @brief Should 'play' be sensitive?
 *
 * Yes if tracks are selected and no directories are */
static int choose_play_sensitive(void attribute((unused)) *extra) {
  int files = 0;
  
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_play_sensitive_callback,
                                      &files);
  return files > 0;
}

static void choose_gather_selected_files_callback(GtkTreeModel attribute((unused)) *model,
                                                  GtkTreePath attribute((unused)) *path,
                                                  GtkTreeIter *iter,
                                                  gpointer data) {
  struct vector *v = data;

  if(choose_is_file(iter))
    vector_append(v, choose_get_track(iter));
}

  
static void choose_play_activate(GtkMenuItem attribute((unused)) *item,
                                 gpointer attribute((unused)) userdata) {
  struct vector v[1];
  vector_init(v);
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_gather_selected_files_callback,
                                      v);
  for(int n = 0; n < v->nvec; ++n)
    disorder_eclient_play(client, v->vec[n], choose_play_completed, 0);
}
  
static int choose_properties_sensitive(void *extra) {
  return choose_play_sensitive(extra);
}
  
static void choose_properties_activate(GtkMenuItem attribute((unused)) *item,
                                       gpointer attribute((unused)) userdata) {
  struct vector v[1];
  vector_init(v);
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_gather_selected_files_callback,
                                      v);
  properties(v->nvec, (const char **)v->vec);
}

/** @brief Pop-up menu for choose */
static struct menuitem choose_menuitems[] = {
  {
    "Play track",
    choose_play_activate,
    choose_play_sensitive,
    0,
    0
  },
  {
    "Track properties",
    choose_properties_activate,
    choose_properties_sensitive,
    0,
    0
  },
  {
    "Select all tracks",
    choose_selectall_activate,
    choose_selectall_sensitive,
    0,
    0
  },
  {
    "Deselect all tracks",
    choose_selectnone_activate,
    choose_selectnone_sensitive,
    0,
    0
  },
};

const struct tabtype choose_tabtype = {
  choose_properties_sensitive,
  choose_selectall_sensitive,
  choose_selectnone_sensitive,
  choose_properties_activate,
  choose_selectall_activate,
  choose_selectnone_activate,
  0,
  0
};

/** @brief Called when a mouse button is pressed or released */
gboolean choose_button_event(GtkWidget attribute((unused)) *widget,
                             GdkEventButton *event,
                             gpointer attribute((unused)) user_data) {
  if(event->type == GDK_BUTTON_RELEASE && event->button == 2) {
    /* Middle click release - play track */
    ensure_selected(GTK_TREE_VIEW(choose_view), event);
    choose_play_activate(NULL, NULL);
  } else if(event->type == GDK_BUTTON_PRESS && event->button == 3) {
    /* Right click press - pop up the menu */
    ensure_selected(GTK_TREE_VIEW(choose_view), event);
    popup(&choose_menu, event,
          choose_menuitems, sizeof choose_menuitems / sizeof *choose_menuitems,
          0);
    return TRUE;
  }
  return FALSE;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
