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
/** @file disobedience/search.c
 * @brief Search support
 *
 * TODO:
 * - cleverer focus to implement typeahead find
 * - don't steal ^A
 * - make arrow buttons work
 * - delay search initiation until keyboard goes quiet for a bit
 *   (so that you don't get all the 'lo' tracks when you search for 'love')
 */
#include "disobedience.h"
#include "choose.h"

static GtkWidget *choose_search_entry;
static GtkWidget *choose_next;
static GtkWidget *choose_prev;
static GtkWidget *choose_clear;

/** @brief True if a search command is in flight */
static int choose_searching;

/** @brief True if in-flight search is now known to be obsolete */
static int choose_search_obsolete;

/** @brief Hash of all search result */
static hash *choose_search_hash;

/** @brief List of invisible search results
 *
 * This only lists search results not yet known to be visible, and is
 * gradually depleted.
 */
static char **choose_search_results;

/** @brief Length of @ref choose_search_results */
static int choose_n_search_results;

/** @brief Row references for search results */
static GtkTreeRowReference **choose_search_references;

/** @brief Length of @ref choose_search_references */
static int choose_n_search_references;

/** @brief Event handle for monitoring newly inserted tracks */
static event_handle choose_inserted_handle;

static void choose_search_entry_changed(GtkEditable *editable,
                                        gpointer user_data);

int choose_is_search_result(const char *track) {
  return choose_search_hash && hash_find(choose_search_hash, track);
}

/** @brief Called when the cancel search button is clicked */
static void choose_clear_clicked(GtkButton attribute((unused)) *button,
                                 gpointer attribute((unused)) userdata) {
  gtk_entry_set_text(GTK_ENTRY(choose_search_entry), "");
  /* The changed signal will do the rest of the work for us */
}

static int is_prefix(const char *dir, const char *track) {
  size_t nd = strlen(dir);

  if(nd < strlen(track)
     && track[nd] == '/'
     && !strncmp(track, dir, nd))
    return 1;
  else
    return 0;
}

/** @brief Do some work towards making @p track visible
 * @return True if we made it visible or it was missing
 */
static int choose_make_one_visible(const char *track) {
  //fprintf(stderr, " choose_make_one_visible %s\n", track);
  /* We walk through nodes at the top level looking for directories that are
   * prefixes of the target track.
   *
   * - if we find one and it's expanded we walk through its children
   * - if we find one and it's NOT expanded then we expand it, and arrange
   *   to be revisited
   * - if we don't find one then we're probably out of date
   */
  GtkTreeIter it[1];
  gboolean itv = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(choose_store),
                                               it);
  while(itv) {
    const char *dir = choose_get_track(it);

    //fprintf(stderr, "  %s\n", dir);
    if(!dir) {
      /* Placeholder */
      itv = gtk_tree_model_iter_next(GTK_TREE_MODEL(choose_store), it);
      continue;
    }
    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(choose_store),
                                                it);
    if(!strcmp(dir, track)) {
      /* We found the track.  If everything above it was expanded, it will be
       * too.  So we can report it as visible. */
      //fprintf(stderr, "   found %s\n", track);
      choose_search_references[choose_n_search_references++]
        = gtk_tree_row_reference_new(GTK_TREE_MODEL(choose_store), path);
      gtk_tree_path_free(path);
      return 1;
    }
    if(is_prefix(dir, track)) {
      /* We found a prefix of the target track. */
      //fprintf(stderr, "   is a prefix\n");
      const gboolean expanded
        = gtk_tree_view_row_expanded(GTK_TREE_VIEW(choose_view), path);
      if(expanded) {
        //fprintf(stderr, "   is apparently expanded\n");
        /* This directory is expanded, let's make like Augustus Gibbons and
         * take it to the next level. */
        GtkTreeIter child[1];           /* don't know if parent==iter allowed */
        itv = gtk_tree_model_iter_children(GTK_TREE_MODEL(choose_store),
                                           child,
                                           it);
        *it = *child;
        if(choose_is_placeholder(it)) {
          //fprintf(stderr, "   %s is expanded, has a placeholder child\n", dir);
          /* We assume that placeholder children of expanded rows are about to
           * be replaced */
          gtk_tree_path_free(path);
          return 0;
        }
      } else {
        //fprintf(stderr, "   requesting expansion of %s\n", dir);
        /* Track is below a non-expanded directory.  So let's expand it.
         * choose_make_visible() will arrange a revisit in due course. */
        gtk_tree_view_expand_row(GTK_TREE_VIEW(choose_view),
                                 path,
                                 FALSE/*open_all*/);
        gtk_tree_path_free(path);
        /* TODO: the old version would remember which rows had been expanded
         * just to show search results and collapse them again.  We should
         * probably do that. */
        return 0;
      }
    } else
      itv = gtk_tree_model_iter_next(GTK_TREE_MODEL(choose_store), it);
    gtk_tree_path_free(path);
  }
  /* If we reach the end then we didn't find the track at all. */
  fprintf(stderr, "choose_make_one_visible: could not find %s\n",
          track);
  return 1;
}

/** @brief Compare two GtkTreeRowReferences
 *
 * Not very efficient since it does multiple memory operations per
 * comparison!
 */
static int choose_compare_references(const void *av, const void *bv) {
  GtkTreeRowReference *a = *(GtkTreeRowReference **)av;
  GtkTreeRowReference *b = *(GtkTreeRowReference **)bv;
  GtkTreePath *pa = gtk_tree_row_reference_get_path(a);
  GtkTreePath *pb = gtk_tree_row_reference_get_path(b);
  const int rc = gtk_tree_path_compare(pa, pb);
  gtk_tree_path_free(pa);
  gtk_tree_path_free(pb);
  return rc;
}

/** @brief Make @p path visible
 * @param path Row reference to make visible
 * @param row_align Row alignment (or -ve)
 * @return 0 on success, nonzero if @p ref has gone stale
 *
 * If @p row_align is negative no row alignemt is performed.  Otherwise
 * it must be between 0 (the top) and 1 (the bottom).
 */
static int choose_make_path_visible(GtkTreePath *path,
                                    gfloat row_align) {
  /* Make sure that the target's parents are all expanded */
  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(choose_view), path);
  /* Make sure the target is visible */
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(choose_view), path, NULL,
                               row_align >= 0.0,
                               row_align,
                               0);
  return 0;
}

/** @brief Make @p ref visible
 * @param ref Row reference to make visible
 * @param row_align Row alignment (or -ve)
 * @return 0 on success, nonzero if @p ref has gone stale
 *
 * If @p row_align is negative no row alignemt is performed.  Otherwise
 * it must be between 0 (the top) and 1 (the bottom).
 */
static int choose_make_ref_visible(GtkTreeRowReference *ref,
                                   gfloat row_align) {
  GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
  if(!path)
    return -1;
  choose_make_path_visible(path, row_align);
  gtk_tree_path_free(path);
  return 0;
}

/** @brief Do some work towards ensuring that all search results are visible
 *
 * Assumes there's at least one results!
 */
static void choose_make_visible(const char attribute((unused)) *event,
                                void attribute((unused)) *eventdata,
                                void attribute((unused)) *callbackdata) {
  //fprintf(stderr, "choose_make_visible\n");
  int remaining = 0;

  for(int n = 0; n < choose_n_search_results; ++n) {
    if(!choose_search_results[n])
      continue;
    if(choose_make_one_visible(choose_search_results[n]))
      choose_search_results[n] = 0;
    else
      ++remaining;
  }
  //fprintf(stderr, "remaining=%d\n", remaining);
  if(remaining) {
    /* If there's work left to be done make sure we get a callback when
     * something changes */
    if(!choose_inserted_handle)
      choose_inserted_handle = event_register("choose-inserted-tracks",
                                              choose_make_visible, 0);
  } else {
    /* Suppress callbacks if there's nothing more to do */
    event_cancel(choose_inserted_handle);
    choose_inserted_handle = 0;
    /* We've expanded everything, now we can mess with the cursor */
    //fprintf(stderr, "sort %d references\n", choose_n_search_references);
    qsort(choose_search_references,
          choose_n_search_references,
          sizeof (GtkTreeRowReference *),
          choose_compare_references);
    choose_make_ref_visible(choose_search_references[0], 0.5);
  }
}

/** @brief Called with search results */
static void choose_search_completed(void attribute((unused)) *v,
                                    const char *error,
                                    int nvec, char **vec) {
  //fprintf(stderr, "choose_search_completed\n");
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_searching = 0;
  /* If the search was obsoleted initiate another one */
  if(choose_search_obsolete) {
    choose_search_obsolete = 0;
    choose_search_entry_changed(0, 0);
    return;
  }
  //fprintf(stderr, "*** %d search results\n", nvec);
  choose_search_hash = hash_new(1);
  if(nvec) {
    for(int n = 0; n < nvec; ++n)
      hash_add(choose_search_hash, vec[n], "", HASH_INSERT);
    /* Stash results for choose_make_visible */
    choose_n_search_results = nvec;
    choose_search_results = vec;
    /* Make a big-enough buffer for the results row reference list */
    choose_n_search_references = 0;
    choose_search_references = xcalloc(nvec, sizeof (GtkTreeRowReference *));
    /* Start making rows visible */
    choose_make_visible(0, 0, 0);
    gtk_widget_set_sensitive(choose_next, TRUE);
    gtk_widget_set_sensitive(choose_prev, TRUE);
  } else {
    gtk_widget_set_sensitive(choose_next, FALSE);
    gtk_widget_set_sensitive(choose_prev, FALSE);
  }
  event_raise("search-results-changed", 0);
}

/** @brief Called when the search entry changes */
static void choose_search_entry_changed
    (GtkEditable attribute((unused)) *editable,
     gpointer attribute((unused)) user_data) {
  //fprintf(stderr, "choose_search_entry_changed\n");
  /* If a search is in flight don't initiate a new one until it comes back */
  if(choose_searching) {
    choose_search_obsolete = 1;
    return;
  }
  char *terms = xstrdup(gtk_entry_get_text(GTK_ENTRY(choose_search_entry)));
  /* Strip leading and trailing space */
  while(*terms == ' ') ++terms;
  char *e = terms + strlen(terms);
  while(e > terms && e[-1] == ' ') --e;
  *e = 0;
  if(!*terms) {
    /* Nothing to search for.  Fake a completion call. */
    choose_search_completed(0, 0, 0, 0);
    return;
  }
  if(disorder_eclient_search(client, choose_search_completed, terms, 0)) {
    /* Bad search terms.  Fake a completion call. */
    choose_search_completed(0, 0, 0, 0);
    return;
  }
  choose_searching = 1;
}

/** @brief Identify first and last visible paths
 *
 * We'd like to use gtk_tree_view_get_visible_range() for this, but that was
 * introduced in GTK+ 2.8, and Fink only has 2.6 (which is around three years
 * out of date at time of writing), and I'm not yet prepared to rule out Fink
 * support.
 */
static gboolean choose_get_visible_range(GtkTreeView *tree_view,
                                         GtkTreePath **startpathp,
                                         GtkTreePath **endpathp) {
  GdkRectangle visible_tc[1];

  /* Get the visible rectangle in tree coordinates */
  gtk_tree_view_get_visible_rect(tree_view, visible_tc);
  /*fprintf(stderr, "visible: %dx%x at %dx%d\n",
          visible_tc->width, visible_tc->height,
          visible_tc->x, visible_tc->y);*/
  if(startpathp) {
    /* Convert top-left visible point to widget coordinates */
    int x_wc, y_wc;
    gtk_tree_view_tree_to_widget_coords(tree_view,
                                        visible_tc->x, visible_tc->y,
                                        &x_wc, &y_wc);
    //fprintf(stderr, " start widget coords: %dx%d\n", x_wc, y_wc);
    gtk_tree_view_get_path_at_pos(tree_view,
                                  x_wc, y_wc,
                                  startpathp,
                                  NULL,
                                  NULL, NULL);
  }
  if(endpathp) {
    /* Convert bottom-left visible point to widget coordinates */
    /* Convert top-left visible point to widget coordinates */
    int x_wc, y_wc;
    gtk_tree_view_tree_to_widget_coords(tree_view,
                                        visible_tc->x,
                                        visible_tc->y + visible_tc->height - 1,
                                        &x_wc, &y_wc);
    //fprintf(stderr, " end widget coords: %dx%d\n", x_wc, y_wc);
    gtk_tree_view_get_path_at_pos(tree_view,
                                  x_wc, y_wc,
                                  endpathp,
                                  NULL,
                                  NULL, NULL);
  }
  return TRUE;
}

static void choose_next_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  /* Find the last visible row */
  GtkTreePath *endpath;
  gboolean endvalid = choose_get_visible_range(GTK_TREE_VIEW(choose_view),
                                               NULL,
                                               &endpath);
  if(!endvalid)
    return;
  /* Find a the first search result later than it.  They're sorted so we could
   * actually do much better than this if necessary. */
  for(int n = 0; n < choose_n_search_references; ++n) {
    GtkTreePath *path
      = gtk_tree_row_reference_get_path(choose_search_references[n]);
    if(!path)
      continue;
    if(gtk_tree_path_compare(endpath, path) < 0) {
      choose_make_path_visible(path, 0.5);
      gtk_tree_path_free(path);
      return;
    }
    gtk_tree_path_free(path);
  }
}

static void choose_prev_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  /* TODO can we de-dupe with choose_next_clicked?  Probably yes. */
  /* Find the first visible row */
  GtkTreePath *startpath;
  gboolean startvalid = choose_get_visible_range(GTK_TREE_VIEW(choose_view),
                                                 &startpath,
                                                 NULL);
  if(!startvalid)
    return;
  /* Find a the last search result earlier than it.  They're sorted so we could
   * actually do much better than this if necessary. */
  for(int n = choose_n_search_references - 1; n >= 0; --n) {
    GtkTreePath *path
      = gtk_tree_row_reference_get_path(choose_search_references[n]);
    if(!path)
      continue;
    if(gtk_tree_path_compare(startpath, path) > 0) {
      choose_make_path_visible(path, 0.5);
      gtk_tree_path_free(path);
      return;
    }
    gtk_tree_path_free(path);
  }
}

/** @brief Create the search widget */
GtkWidget *choose_search_widget(void) {

  /* Text entry box for search terms */
  choose_search_entry = gtk_entry_new();
  gtk_widget_set_style(choose_search_entry, tool_style);
  g_signal_connect(choose_search_entry, "changed",
                   G_CALLBACK(choose_search_entry_changed), 0);
  gtk_tooltips_set_tip(tips, choose_search_entry,
                       "Enter search terms here; search is automatic", "");

  /* Cancel button to clear the search */
  choose_clear = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  gtk_widget_set_style(choose_clear, tool_style);
  g_signal_connect(G_OBJECT(choose_clear), "clicked",
                   G_CALLBACK(choose_clear_clicked), 0);
  gtk_tooltips_set_tip(tips, choose_clear, "Clear search terms", "");

  /* Up and down buttons to find previous/next results; initially they are not
   * usable as there are no search results. */
  choose_prev = iconbutton("up.png", "Previous search result");
  g_signal_connect(G_OBJECT(choose_prev), "clicked",
                   G_CALLBACK(choose_prev_clicked), 0);
  gtk_widget_set_style(choose_prev, tool_style);
  gtk_widget_set_sensitive(choose_prev, 0);
  choose_next = iconbutton("down.png", "Next search result");
  g_signal_connect(G_OBJECT(choose_next), "clicked",
                   G_CALLBACK(choose_next_clicked), 0);
  gtk_widget_set_style(choose_next, tool_style);
  gtk_widget_set_sensitive(choose_next, 0);
  
  /* Pack the search tools button together on a line */
  GtkWidget *hbox = gtk_hbox_new(FALSE/*homogeneous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(hbox), choose_search_entry,
                     TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), choose_prev,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), choose_next,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), choose_clear,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);

  return hbox;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
