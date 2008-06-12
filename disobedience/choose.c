/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file disobedience/choose.c
 * @brief Hierarchical track selection and search
 *
 * We now use an ordinary GtkTreeStore/GtkTreeView.
 *
 * We have an extra column with per-row data.  This isn't referenced from
 * anywhere the GC can see so explicit memory management is required.
 * (TODO perhaps we could fix this using a gobject?)
 *
 * We don't want to pull the entire tree in memory, but we want directories to
 * show up as having children.  Therefore we give directories a placeholder
 * child and replace their children when they are opened.  Placeholders have a
 * null choosedata pointer.
 *
 * TODO We do a period sweep which kills contracted nodes, putting back
 * placeholders, and updating expanded nodes to keep up with server-side
 * changes.  (We could trigger the latter off rescan complete notifications?)
 * 
 * TODO:
 * - sweep up contracted nodes
 * - update when content may have changed (e.g. after a rescan)
 */

#include "disobedience.h"
#include "choose.h"

/** @brief The current selection tree */
GtkTreeStore *choose_store;

/** @brief The view onto the selection tree */
GtkWidget *choose_view;

/** @brief The selection tree's selection */
GtkTreeSelection *choose_selection;

/** @brief Map choosedata types to names */
static const char *const choose_type_map[] = { "track", "dir" };

/** @brief Return the choosedata given an interator */
struct choosedata *choose_iter_to_data(GtkTreeIter *iter) {
  GValue v[1];
  memset(v, 0, sizeof v);
  gtk_tree_model_get_value(GTK_TREE_MODEL(choose_store), iter, CHOOSEDATA_COLUMN, v);
  assert(G_VALUE_TYPE(v) == G_TYPE_POINTER);
  struct choosedata *const cd = g_value_get_pointer(v);
  g_value_unset(v);
  return cd;
}

struct choosedata *choose_path_to_data(GtkTreePath *path) {
  GtkTreeIter it[1];
  gboolean itv = gtk_tree_model_get_iter(GTK_TREE_MODEL(choose_store),
                                         it, path);
  assert(itv);
  return choose_iter_to_data(it);
}

/** @brief Remove node @p it and all its children
 * @param Iterator, updated to point to next
 * @return True if iterator remains valid
 */
static gboolean choose_remove_node(GtkTreeIter *it) {
  GtkTreeIter child[1];
  gboolean childv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                                 child,
                                                 it);
  while(childv)
    childv = choose_remove_node(child);
  struct choosedata *cd = choose_iter_to_data(it);
  if(cd) {
    g_free(cd->track);
    g_free(cd->sort);
    g_free(cd);
  }
  return gtk_tree_store_remove(choose_store, it);
}

/** @brief Update length and state fields */
static gboolean choose_set_state_callback(GtkTreeModel attribute((unused)) *model,
                                          GtkTreePath attribute((unused)) *path,
                                          GtkTreeIter *it,
                                          gpointer attribute((unused)) data) {
  struct choosedata *cd = choose_iter_to_data(it);
  if(!cd)
    return FALSE;                       /* Skip placeholders*/
  if(cd->type == CHOOSE_FILE) {
    const long l = namepart_length(cd->track);
    char length[64];
    if(l > 0)
      byte_snprintf(length, sizeof length, "%ld:%02ld", l / 60, l % 60);
    else
      length[0] = 0;
    gtk_tree_store_set(choose_store, it,
                       LENGTH_COLUMN, length,
                       STATE_COLUMN, queued(cd->track),
                       -1);
  }
  return FALSE;                         /* continue walking */
}

/** @brief Called when the queue or playing track change */
static void choose_set_state(const char attribute((unused)) *event,
                             void attribute((unused)) *eventdata,
                             void attribute((unused)) *callbackdata) {
  gtk_tree_model_foreach(GTK_TREE_MODEL(choose_store),
                         choose_set_state_callback,
                         NULL);
}

/** @brief (Re-)populate a node
 * @param parent_ref Node to populate or NULL to fill root
 * @param nvec Number of children to add
 * @param vec Children
 * @param dirs True if children are directories
 *
 * Adjusts the set of files (or directories) below @p parent_ref to match those
 * listed in @p nvec and @p vec.
 *
 * @parent_ref will be destroyed.
 */
static void choose_populate(GtkTreeRowReference *parent_ref,
                            int nvec, char **vec,
                            int type) {
  /* Compute parent_* */
  GtkTreeIter pit[1], *parent_it;
  GtkTreePath *parent_path;
  if(parent_ref) {
    parent_path = gtk_tree_row_reference_get_path(parent_ref);
    parent_it = pit;
    gboolean pitv = gtk_tree_model_get_iter(GTK_TREE_MODEL(choose_store),
                                            pit, parent_path);
    assert(pitv);
    /*fprintf(stderr, "choose_populate %s: parent path is [%s]\n",
            choose_type_map[type],
            gtk_tree_path_to_string(parent_path));*/
  } else {
    parent_path = 0;
    parent_it = 0;
    /*fprintf(stderr, "choose_populate %s: populating the root\n",
            choose_type_map[type]);*/
  }
  /* Remove unwanted nodes and find out which we must add */
  //fprintf(stderr, " trimming unwanted %s nodes\n", choose_type_map[type]);
  char *found = xmalloc(nvec);
  GtkTreeIter it[1];
  gboolean itv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                              it,
                                              parent_it);
  while(itv) {
    struct choosedata *cd = choose_iter_to_data(it);
    int keep;

    if(!cd)  {
      /* Always kill placeholders */
      //fprintf(stderr, "  kill a placeholder\n");
      keep = 0;
    } else if(cd->type == type) {
      /* This is the type we care about */
      //fprintf(stderr, "  %s is a %s\n", cd->track, choose_type_map[cd->type]);
      int n;
      for(n = 0; n < nvec && strcmp(vec[n], cd->track); ++n)
        ;
      if(n < nvec) {
        //fprintf(stderr, "   ... and survives\n");
        found[n] = 1;
        keep = 1;
      } else {
        //fprintf(stderr, "   ... and is to be removed\n");
        keep = 0;
      }
    } else {
      /* Keep wrong-type entries */
      //fprintf(stderr, "  %s is a %s\n", cd->track, choose_type_map[cd->type]);
      keep = 1;
    }
    if(keep)
      itv = gtk_tree_model_iter_next(GTK_TREE_MODEL(choose_store), it);
    else
      itv = choose_remove_node(it);
  }
  /* Add nodes we don't have */
  int inserted = 0;
  //fprintf(stderr, " inserting new %s nodes\n", choose_type_map[type]);
  for(int n = 0; n < nvec; ++n) {
    if(!found[n]) {
      //fprintf(stderr, "  %s was not found\n", vec[n]);
      struct choosedata *cd = g_malloc0(sizeof *cd);
      cd->type = type;
      cd->track = g_strdup(vec[n]);
      cd->sort = g_strdup(trackname_transform(choose_type_map[type],
                                              vec[n],
                                              "sort"));
      gtk_tree_store_append(choose_store, it, parent_it);
      gtk_tree_store_set(choose_store, it,
                         NAME_COLUMN, trackname_transform(choose_type_map[type],
                                                          vec[n],
                                                          "display"),
                         CHOOSEDATA_COLUMN, cd,
                         ISFILE_COLUMN, type == CHOOSE_FILE,
                         -1);
      /* Update length and state; we expect this to kick off length lookups
       * rather than necessarily get the right value the first time round. */
      choose_set_state_callback(0, 0, it, 0);
      ++inserted;
      /* If we inserted a directory, insert a placeholder too, so it appears to
       * have children; it will be deleted when we expand the directory. */
      if(type == CHOOSE_DIRECTORY) {
        //fprintf(stderr, "  inserting a placeholder\n");
        GtkTreeIter placeholder[1];

        gtk_tree_store_append(choose_store, placeholder, it);
        gtk_tree_store_set(choose_store, placeholder,
                           NAME_COLUMN, "Waddling...",
                           CHOOSEDATA_COLUMN, (void *)0,
                           -1);
      }
    }
  }
  //fprintf(stderr, " %d nodes inserted\n", inserted);
  if(inserted) {
    /* TODO sort the children */
  }
  if(parent_ref) {
    /* If we deleted a placeholder then we must re-expand the row */
    gtk_tree_view_expand_row(GTK_TREE_VIEW(choose_view), parent_path, FALSE);
    gtk_tree_row_reference_free(parent_ref);
    gtk_tree_path_free(parent_path);
  }
}

static void choose_dirs_completed(void *v,
                                  const char *error,
                                  int nvec, char **vec) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_populate(v, nvec, vec, CHOOSE_DIRECTORY);
}

static void choose_files_completed(void *v,
                                   const char *error,
                                   int nvec, char **vec) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_populate(v, nvec, vec, CHOOSE_FILE);
}

void choose_play_completed(void attribute((unused)) *v,
                           const char *error) {
  if(error)
    popup_protocol_error(0, error);
}

static void choose_state_toggled
    (GtkCellRendererToggle attribute((unused)) *cell_renderer,
     gchar *path_str,
     gpointer attribute((unused)) user_data) {
  GtkTreeIter it[1];
  /* Identify the track */
  gboolean itv =
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(choose_store),
                                        it,
                                        path_str);
  if(!itv)
    return;
  struct choosedata *cd = choose_iter_to_data(it);
  if(!cd)
    return;
  if(cd->type != CHOOSE_FILE)
    return;
  if(queued(cd->track))
    return;
  disorder_eclient_play(client, xstrdup(cd->track),
                        choose_play_completed, 0);
  
}

static void choose_row_expanded(GtkTreeView attribute((unused)) *treeview,
                                GtkTreeIter *iter,
                                GtkTreePath *path,
                                gpointer attribute((unused)) user_data) {
  /*fprintf(stderr, "row-expanded path=[%s]\n",
          gtk_tree_path_to_string(path));*/
  /* We update a node's contents whenever it is expanded, even if it was
   * already populated; the effect is that contracting and expanding a node
   * suffices to update it to the latest state on the server. */
  struct choosedata *cd = choose_iter_to_data(iter);
  disorder_eclient_files(client, choose_files_completed,
                         xstrdup(cd->track),
                         NULL,
                         gtk_tree_row_reference_new(GTK_TREE_MODEL(choose_store),
                                                    path));
  disorder_eclient_dirs(client, choose_dirs_completed,
                        xstrdup(cd->track),
                        NULL,
                        gtk_tree_row_reference_new(GTK_TREE_MODEL(choose_store),
                                                   path));
  /* The row references are destroyed in the _completed handlers. */
}

/** @brief Create the choose tab */
GtkWidget *choose_widget(void) {
  /* Create the tree store. */
  choose_store = gtk_tree_store_new(1 + CHOOSEDATA_COLUMN,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_POINTER);

  /* Create the view */
  choose_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(choose_store));
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(choose_view), TRUE);

  /* Create cell renderers and columns */
  /* TODO use a table */
  {
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes
      ("Track",
       r,
       "text", NAME_COLUMN,
       (char *)0);
    gtk_tree_view_column_set_resizable(c, TRUE);
    gtk_tree_view_column_set_reorderable(c, TRUE);
    g_object_set(c, "expand", TRUE, (char *)0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(choose_view), c);
  }
  {
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes
      ("Length",
       r,
       "text", LENGTH_COLUMN,
       (char *)0);
    gtk_tree_view_column_set_resizable(c, TRUE);
    gtk_tree_view_column_set_reorderable(c, TRUE);
    g_object_set(r, "xalign", (gfloat)1.0, (char *)0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(choose_view), c);
  }
  {
    GtkCellRenderer *r = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes
      ("Queued",
       r,
       "active", STATE_COLUMN,
       "visible", ISFILE_COLUMN,
       (char *)0);
    gtk_tree_view_column_set_resizable(c, TRUE);
    gtk_tree_view_column_set_reorderable(c, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(choose_view), c);
    g_signal_connect(r, "toggled",
                     G_CALLBACK(choose_state_toggled), 0);
  }
  
  /* The selection should support multiple things being selected */
  choose_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(choose_view));
  gtk_tree_selection_set_mode(choose_selection, GTK_SELECTION_MULTIPLE);

  /* Catch button presses */
  g_signal_connect(choose_view, "button-press-event",
                   G_CALLBACK(choose_button_event), 0);
  g_signal_connect(choose_view, "button-release-event",
                   G_CALLBACK(choose_button_event), 0);
  /* Catch row expansions so we can fill in placeholders */
  g_signal_connect(choose_view, "row-expanded",
                   G_CALLBACK(choose_row_expanded), 0);

  event_register("queue-list-changed", choose_set_state, 0);
  event_register("playing-track-changed", choose_set_state, 0);
  
  /* Fill the root */
  disorder_eclient_files(client, choose_files_completed, "", NULL, NULL); 
  disorder_eclient_dirs(client, choose_dirs_completed, "", NULL, NULL); 
  
  /* Make the widget scrollable */
  GtkWidget *scrolled = scroll_widget(choose_view);
  g_object_set_data(G_OBJECT(scrolled), "type", (void *)&choose_tabtype);
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
