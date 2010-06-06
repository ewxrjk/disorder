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
/** @file disobedience/choose-menu.c
 * @brief Popup menu for choose screen
 */
#include "disobedience.h"
#include "popup.h"
#include "choose.h"

/** @brief Popup menu */
static GtkWidget *choose_menu;

/** @brief Path to directory pending a "select children" operation */
static GtkTreePath *choose_eventually_select_children;

/** @brief Should edit->select all be sensitive?  No, for the choose tab. */
static int choose_selectall_sensitive(void attribute((unused)) *extra) {
  return FALSE;
}

/** @brief Activate edit->select all (which should do nothing) */
static void choose_selectall_activate(GtkMenuItem attribute((unused)) *item,
                                      gpointer attribute((unused)) userdata) {
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

static void choose_gather_selected_dirs_callback(GtkTreeModel attribute((unused)) *model,
                                                 GtkTreePath attribute((unused)) *path,
                                                 GtkTreeIter *iter,
                                                 gpointer data) {
  struct vector *v = data;

  if(choose_is_dir(iter))
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

/** @brief Set sensitivity for select children
 *
 * Sensitive if we've selected exactly one directory.
 */
static int choose_selectchildren_sensitive(void attribute((unused)) *extra) {
  struct vector v[1];
  /* Only one thing should be selected */
  if(gtk_tree_selection_count_selected_rows(choose_selection) != 1)
    return FALSE;
  /* The selected thing should be a directory */
  vector_init(v);
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_gather_selected_dirs_callback,
                                      v);
  return v->nvec == 1;
}

/** @brief Actually select the children of path
 *
 * We deselect everything else, too.
 */
static void choose_select_children(GtkTreePath *path) {
  GtkTreeIter iter[1], child[1];
  
  if(gtk_tree_model_get_iter(GTK_TREE_MODEL(choose_store), iter, path)) {
    gtk_tree_selection_unselect_all(choose_selection);
    for(int n = 0;
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(choose_store), child,
                                      iter, n);
        ++n) {
      if(choose_is_file(child))
        gtk_tree_selection_select_iter(choose_selection, child);
    }
  }
}

/** @brief Called to expand the children of path/iter */
static void choose_selectchildren_callback(GtkTreeModel attribute((unused)) *model,
                                           GtkTreePath *path,
                                           GtkTreeIter attribute((unused)) *iter,
                                           gpointer attribute((unused)) data) {
  if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(choose_view), path)) {
    /* Directory is already expanded */
    choose_select_children(path);
  } else {
    /* Directory is not expanded, so expand it */
    gtk_tree_view_expand_row(GTK_TREE_VIEW(choose_view), path, FALSE/*!expand_all*/);
    /* Select its children when it's done */
    if(choose_eventually_select_children)
      gtk_tree_path_free(choose_eventually_select_children);
    choose_eventually_select_children = gtk_tree_path_copy(path);
  }
}

/** @brief Called when all pending track fetches are finished
 *
 * If there's a pending select-children operation, it can now be actioned
 * (or might have gone stale).
 */
void choose_menu_moretracks(const char attribute((unused)) *event,
                            void attribute((unused)) *eventdata,
                            void attribute((unused)) *callbackdata) {
  if(choose_eventually_select_children) {
    choose_select_children(choose_eventually_select_children);
    gtk_tree_path_free(choose_eventually_select_children);
    choose_eventually_select_children = 0;
  }
}

/** @brief Select all children
 *
 * Easy enough if the directory is already expanded, we can just select its
 * children.  However if it is not then we must expand it and _when this has
 * completed_ select its children.
 *
 * The way this is implented could cope with multiple directories but
 * choose_selectchildren_sensitive() should stop this.
 */
static void choose_selectchildren_activate
    (GtkMenuItem attribute((unused)) *item,
     gpointer attribute((unused)) userdata) {
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_selectchildren_callback,
                                      0);
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
    "Select children",
    choose_selectchildren_activate,
    choose_selectchildren_sensitive,
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
