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

/** @brief Called with search results */
static void choose_search_completed(void attribute((unused)) *v,
                                    const char *error,
                                    int nvec, char **vec) {
  if(error) {
    popup_protocol_error(0, error);
    return;
  }
  choose_searching = 0;
  /* If the search was obsoleted initiate another one */
  if(choose_search_obsolete) {
    choose_search_entry_changed(0, 0);
    return;
  }
  //fprintf(stderr, "%d search results\n", nvec);
  choose_search_hash = hash_new(1);
  for(int n = 0; n < nvec; ++n)
    hash_add(choose_search_hash, vec[n], "", HASH_INSERT);
  /* TODO arrange visibility of all search results */
  event_raise("search-results-changed", 0);
}

/** @brief Called when the search entry changes */
static void choose_search_entry_changed
    (GtkEditable attribute((unused)) *editable,
     gpointer attribute((unused)) user_data) {
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

static void choose_next_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  fprintf(stderr, "next\n");            /* TODO */
}

static void choose_prev_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  fprintf(stderr, "prev\n");            /* TODO */
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
