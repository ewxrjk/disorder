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
 * We don't want to pull the entire tree in memory, but we want directories to
 * show up as having children.  Therefore we give directories a placeholder
 * child and replace their children when they are opened.  Placeholders have
 * TRACK_COLUMN="" and ISFILE_COLUMN=FALSE (so that they don't get check boxes,
 * lengths, etc).
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

/** @brief Count of file listing operations in flight */
static int choose_list_in_flight;

static char *choose_get_string(GtkTreeIter *iter, int column) {
  gchar *gs;
  gtk_tree_model_get(GTK_TREE_MODEL(choose_store), iter,
                     column, &gs,
                     -1);
  char *s = xstrdup(gs);
  g_free(gs);
  return s;
}

char *choose_get_track(GtkTreeIter *iter) {
  char *s = choose_get_string(iter, TRACK_COLUMN);
  return *s ? s : 0;                    /* Placeholder -> NULL */
}

char *choose_get_sort(GtkTreeIter *iter) {
  return choose_get_string(iter, SORT_COLUMN);
}

char *choose_get_display(GtkTreeIter *iter) {
  return choose_get_string(iter, NAME_COLUMN);
}

int choose_is_file(GtkTreeIter *iter) {
  gboolean isfile;
  gtk_tree_model_get(GTK_TREE_MODEL(choose_store), iter,
                     ISFILE_COLUMN, &isfile,
                     -1);
  return isfile;
}

int choose_is_dir(GtkTreeIter *iter) {
  gboolean isfile;
  gtk_tree_model_get(GTK_TREE_MODEL(choose_store), iter,
                     ISFILE_COLUMN, &isfile,
                     -1);
  if(isfile)
    return FALSE;
  return !choose_is_placeholder(iter);
}

int choose_is_placeholder(GtkTreeIter *iter) {
  return choose_get_string(iter, TRACK_COLUMN)[0] == 0;
}

/** @brief Remove node @p it and all its children
 * @param Iterator, updated to point to next
 * @return True if iterator remains valid
 *
 * TODO is this necessary?  gtk_tree_store_remove() does not document what
 * happens to children.
 */
static gboolean choose_remove_node(GtkTreeIter *it) {
  GtkTreeIter child[1];
  gboolean childv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                                 child,
                                                 it);
  while(childv)
    childv = choose_remove_node(child);
  return gtk_tree_store_remove(choose_store, it);
}

/** @brief Update length and state fields */
static gboolean choose_set_state_callback(GtkTreeModel attribute((unused)) *model,
                                          GtkTreePath attribute((unused)) *path,
                                          GtkTreeIter *it,
                                          gpointer attribute((unused)) data) {
  if(choose_is_file(it)) {
    const char *track = choose_get_track(it);
    const long l = namepart_length(track);
    char length[64];
    if(l > 0)
      byte_snprintf(length, sizeof length, "%ld:%02ld", l / 60, l % 60);
    else
      length[0] = 0;
    gtk_tree_store_set(choose_store, it,
                       LENGTH_COLUMN, length,
                       STATE_COLUMN, queued(track),
                       -1);
    if(choose_is_search_result(track))
      gtk_tree_store_set(choose_store, it,
                         BG_COLUMN, SEARCH_RESULT_BG,
                         FG_COLUMN, SEARCH_RESULT_FG,
                         -1);
    else
      gtk_tree_store_set(choose_store, it,
                         BG_COLUMN, (char *)0,
                         FG_COLUMN, (char *)0,
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
 * @param files 1 if children are files, 0 if directories
 *
 * Adjusts the set of files (or directories) below @p parent_ref to match those
 * listed in @p nvec and @p vec.
 *
 * @parent_ref will be destroyed.
 */
static void choose_populate(GtkTreeRowReference *parent_ref,
                            int nvec, char **vec,
                            int isfile) {
  const char *type = isfile ? "track" : "dir";
  //fprintf(stderr, "%d new children of type %s\n", nvec, type);
  if(!nvec)
    goto skip;
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
            type,
            gtk_tree_path_to_string(parent_path));*/
  } else {
    parent_path = 0;
    parent_it = 0;
    /*fprintf(stderr, "choose_populate %s: populating the root\n",
            type);*/
  }
  /* Both td[] and the current node set are sorted so we can do a single linear
   * pass to insert new nodes and remove unwanted ones.  The total performance
   * may be worse than linear depending on the performance of GTK+'s insert and
   * delete operations. */
  //fprintf(stderr, "sorting tracks\n");
  struct tracksort_data *td = tracksort_init(nvec, vec, type);
  GtkTreeIter it[1];
  gboolean itv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                              it,
                                              parent_it);
  int inserted = 0, deleted_placeholder = 0;
  //fprintf(stderr, "inserting tracks type=%s\n", type);
  while(nvec > 0 || itv) {
    /*fprintf(stderr, "td[] = %s, it=%s [%s]\n",
            nvec > 0 ? td->track : "(none)",
            itv ? choose_get_track(it) : "(!itv)",
            itv ? (choose_is_file(it) ? "file" : "dir") : "");*/
    enum { INSERT, DELETE, SKIP_TREE, SKIP_BOTH } action;
    const char *track = itv ? choose_get_track(it) : 0;
    if(itv && !track) {
      //fprintf(stderr, " placeholder\n");
      action = DELETE;
      ++deleted_placeholder;
    } else if(nvec > 0 && itv) {
      /* There's both a tree row and a td[] entry */
      const int cmp = compare_tracks(td->sort, choose_get_sort(it),
                                     td->display, choose_get_display(it),
                                     td->track, track);
      //fprintf(stderr, " cmp=%d\n", cmp);
      if(cmp < 0)
        /* td < it, so we insert td before it */
        action = INSERT;
      else if(cmp > 0) {
        /* td > it, so we must either delete it (if the same type) or skip it */
        if(choose_is_file(it) == isfile)
          action = DELETE;
        else
          action = SKIP_TREE;
      } else
        /* td = it, so we step past both */
        action = SKIP_BOTH;
    } else if(nvec > 0) {
      /* We've reached the end of the tree rows, but new have tracks left in
       * td[] */
      //fprintf(stderr, " inserting\n");
      action = INSERT;
    } else {
      /* We've reached the end of the new tracks from td[], but there are
       * further tracks in the tree */
      //fprintf(stderr, " deleting\n");
      if(choose_is_file(it) == isfile)
        action = DELETE;
      else
        action = SKIP_TREE;
    }
    
    switch(action) {
    case INSERT: {
      //fprintf(stderr, " INSERT %s\n", td->track);
      /* Insert a new row from td[] before it, or at the end if it is no longer
       * valid */
      GtkTreeIter child[1];
      gtk_tree_store_insert_before(choose_store,
                                   child, /* new row */
                                   parent_it, /* parent */
                                   itv ? it : NULL); /* successor */
      gtk_tree_store_set(choose_store, child,
                         NAME_COLUMN, td->display,
                         ISFILE_COLUMN, isfile,
                         TRACK_COLUMN, td->track,
                         SORT_COLUMN, td->sort,
                         -1);
      /* Update length and state; we expect this to kick off length lookups
       * rather than necessarily get the right value the first time round. */
      choose_set_state_callback(0, 0, child, 0);
      /* If we inserted a directory, insert a placeholder too, so it appears to
       * have children; it will be deleted when we expand the directory. */
      if(!isfile) {
        //fprintf(stderr, "  inserting a placeholder\n");
        GtkTreeIter placeholder[1];

        gtk_tree_store_append(choose_store, placeholder, child);
        gtk_tree_store_set(choose_store, placeholder,
                           NAME_COLUMN, "Waddling...",
                           TRACK_COLUMN, "",
                           ISFILE_COLUMN, FALSE,
                           -1);
      }
      ++inserted;
      ++td;
      --nvec;
      break;
    }
    case SKIP_BOTH:
      //fprintf(stderr, " SKIP_BOTH\n");
      ++td;
      --nvec;
      /* fall thru */
    case SKIP_TREE:
      //fprintf(stderr, " SKIP_TREE\n");
      itv = gtk_tree_model_iter_next(GTK_TREE_MODEL(choose_store), it);
      break;
    case DELETE:
      //fprintf(stderr, " DELETE\n");
      itv = choose_remove_node(it);
      break;
    }
  }
  /*fprintf(stderr, "inserted=%d deleted_placeholder=%d\n\n",
          inserted, deleted_placeholder);*/
  if(parent_ref) {
    /* If we deleted a placeholder then we must re-expand the row */
    if(deleted_placeholder)
      gtk_tree_view_expand_row(GTK_TREE_VIEW(choose_view), parent_path, FALSE);
    gtk_tree_row_reference_free(parent_ref);
    gtk_tree_path_free(parent_path);
  }
skip:
  /* We only notify others that we've inserted tracks when there are no more
   * insertions pending, so that they don't have to keep track of how many
   * requests they've made.  */
  if(--choose_list_in_flight == 0) {
    /* Notify interested parties that we inserted some tracks, AFTER making
     * sure that the row is properly expanded */
    //fprintf(stderr, "raising choose-more-tracks\n");
    event_raise("choose-more-tracks", 0);
  }
  //fprintf(stderr, "choose_list_in_flight -> %d-\n", choose_list_in_flight);
}

static void choose_dirs_completed(void *v,
                                  const char *error,
                                  int nvec, char **vec) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_populate(v, nvec, vec, 0/*!isfile*/);
}

static void choose_files_completed(void *v,
                                   const char *error,
                                   int nvec, char **vec) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_populate(v, nvec, vec, 1/*isfile*/);
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
  if(!choose_is_file(it))
    return;
  const char *track = choose_get_track(it);
  if(queued(track))
    return;
  disorder_eclient_play(client, track, choose_play_completed, 0);
  
}

static void choose_row_expanded(GtkTreeView attribute((unused)) *treeview,
                                GtkTreeIter *iter,
                                GtkTreePath *path,
                                gpointer attribute((unused)) user_data) {
  /*fprintf(stderr, "row-expanded path=[%s]\n\n",
          gtk_tree_path_to_string(path));*/
  /* We update a node's contents whenever it is expanded, even if it was
   * already populated; the effect is that contracting and expanding a node
   * suffices to update it to the latest state on the server. */
  const char *track = choose_get_track(iter);
  disorder_eclient_files(client, choose_files_completed,
                         track,
                         NULL,
                         gtk_tree_row_reference_new(GTK_TREE_MODEL(choose_store),
                                                    path));
  disorder_eclient_dirs(client, choose_dirs_completed,
                        track,
                        NULL,
                        gtk_tree_row_reference_new(GTK_TREE_MODEL(choose_store),
                                                   path));
  /* The row references are destroyed in the _completed handlers. */
  choose_list_in_flight += 2;
  //fprintf(stderr, "choose_list_in_flight -> %d+\n", choose_list_in_flight);
}

/** @brief Create the choose tab */
GtkWidget *choose_widget(void) {
  /* Create the tree store. */
  choose_store = gtk_tree_store_new(CHOOSE_COLUMNS,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

  /* Create the view */
  choose_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(choose_store));
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(choose_view), TRUE);

  /* Create cell renderers and columns */
  /* TODO use a table */
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
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes
      ("Track",
       r,
       "text", NAME_COLUMN,
       "background", BG_COLUMN,
       "foreground", FG_COLUMN,
       (char *)0);
    gtk_tree_view_column_set_resizable(c, TRUE);
    gtk_tree_view_column_set_reorderable(c, TRUE);
    g_object_set(c, "expand", TRUE, (char *)0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(choose_view), c);
    gtk_tree_view_set_expander_column(GTK_TREE_VIEW(choose_view), c);
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
  event_register("search-results-changed", choose_set_state, 0);
  event_register("lookups-completed", choose_set_state, 0);

  /* Fill the root */
  disorder_eclient_files(client, choose_files_completed, "", NULL, NULL); 
  disorder_eclient_dirs(client, choose_dirs_completed, "", NULL, NULL); 
  choose_list_in_flight += 2;
  //fprintf(stderr, "choose_list_in_flight -> %d+\n", choose_list_in_flight);

  /* Make the widget scrollable */
  GtkWidget *scrolled = scroll_widget(choose_view);

  /* Pack vertically with the search widget */
  GtkWidget *vbox = gtk_vbox_new(FALSE/*homogenous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled,
                     TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);
  gtk_box_pack_end(GTK_BOX(vbox), choose_search_widget(),
                   FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  
  g_object_set_data(G_OBJECT(vbox), "type", (void *)&choose_tabtype);
  return vbox;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
