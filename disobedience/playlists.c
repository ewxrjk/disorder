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
/** @file disobedience/playlists.c
 * @brief Playlist for Disobedience
 *
 * The playlists management window contains:
 * - a list of all playlists
 * - an add button
 * - a delete button
 * - a drag+drop capable view of the playlist
 * - a close button
 */
#include "disobedience.h"

static void playlists_updated(void *v,
                              const char *err,
                              int nvec, char **vec);

/** @brief Playlist editing window */
static GtkWidget *playlists_window;

/** @brief Tree model for list of playlists */
static GtkListStore *playlists_list;

/** @brief Selection for list of playlists */
static GtkTreeSelection *playlists_selection;

/** @brief Currently selected playlist */
static const char *playlists_selected;

/** @brief Delete button */
static GtkWidget *playlists_delete_button;

/** @brief Current list of playlists or NULL */
char **playlists;

/** @brief Count of playlists */
int nplaylists;

/** @brief Schedule an update to the list of playlists */
static void playlists_update(const char attribute((unused)) *event,
                             void attribute((unused)) *eventdata,
                             void attribute((unused)) *callbackdata) {
  disorder_eclient_playlists(client, playlists_updated, 0);
}

/** @brief qsort() callback for playlist name comparison */
static int playlistcmp(const void *ap, const void *bp) {
  const char *a = *(char **)ap, *b = *(char **)bp;
  const char *ad = strchr(a, '.'), *bd = strchr(b, '.');
  int c;

  /* Group owned playlists by owner */
  if(ad && bd) {
    const int adn = ad - a, bdn = bd - b;
    if((c = strncmp(a, b, adn < bdn ? adn : bdn)))
      return c;
    /* Lexical order within playlists of a single owner */
    return strcmp(ad + 1, bd + 1);
  }

  /* Owned playlists after shared ones */
  if(ad) {
    return 1;
  } else if(bd) {
    return -1;
  }

  /* Lexical order of shared playlists */
  return strcmp(a, b);
}

/** @brief Called with a new list of playlists */
static void playlists_updated(void attribute((unused)) *v,
                              const char *err,
                              int nvec, char **vec) {
  if(err) {
    playlists = 0;
    nplaylists = -1;
    /* Probably means server does not support playlists */
  } else {
    playlists = vec;
    nplaylists = nvec;
    qsort(playlists, nplaylists, sizeof (char *), playlistcmp);
  }
  /* Tell our consumers */
  event_raise("playlists-updated", 0);
}

/** @brief Called to activate a playlist */
static void menu_activate_playlist(GtkMenuItem *menuitem,
                                   gpointer attribute((unused)) user_data) {
  GtkLabel *label = GTK_LABEL(GTK_BIN(menuitem)->child);
  const char *playlist = gtk_label_get_text(label);

  fprintf(stderr, "activate playlist %s\n", playlist); /* TODO */
}

/** @brief Called when the playlists change */
static void menu_playlists_changed(const char attribute((unused)) *event,
                                   void attribute((unused)) *eventdata,
                                   void attribute((unused)) *callbackdata) {
  if(!playlists_menu)
    return;                             /* OMG too soon */
  GtkMenuShell *menu = GTK_MENU_SHELL(playlists_menu);
  /* TODO: we could be more sophisticated and only insert/remove widgets as
   * needed.  For now that's too much effort. */
  while(menu->children)
    gtk_container_remove(GTK_CONTAINER(menu), GTK_WIDGET(menu->children->data));
  /* NB nplaylists can be -1 as well as 0 */
  for(int n = 0; n < nplaylists; ++n) {
    GtkWidget *w = gtk_menu_item_new_with_label(playlists[n]);
    g_signal_connect(w, "activate", G_CALLBACK(menu_activate_playlist), 0);
    gtk_widget_show(w);
    gtk_menu_shell_append(menu, w);
  }
  gtk_widget_set_sensitive(playlists_widget,
                           nplaylists > 0);
  gtk_widget_set_sensitive(editplaylists_widget,
                           nplaylists >= 0);
}

/** @brief (Re-)populate the playlist tree model */
static void playlists_fill(void) {
  GtkTreeIter iter[1];

  if(!playlists_list)
    playlists_list = gtk_list_store_new(1, G_TYPE_STRING);
  gtk_list_store_clear(playlists_list);
  for(int n = 0; n < nplaylists; ++n)
    gtk_list_store_insert_with_values(playlists_list, iter, n/*position*/,
                                      0, playlists[n],        /* column 0 */
                                      -1);                    /* no more cols */
  // TODO reselect whatever was formerly selected if possible, if not then
  // zap the contents view
}

/** @brief Called when the selection might have changed */
static void playlists_selection_changed(GtkTreeSelection attribute((unused)) *treeselection,
                                        gpointer attribute((unused)) user_data) {
  GtkTreeIter iter;
  char *gselected, *selected;
  
  /* Identify the current selection */
  if(gtk_tree_selection_get_selected(playlists_selection, 0, &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(playlists_list), &iter,
                       0, &gselected, -1);
    selected = xstrdup(gselected);
    g_free(gselected);
  } else
    selected = 0;
  /* Eliminate no-change cases */
  if(!selected && !playlists_selected)
    return;
  if(selected && playlists_selected && !strcmp(selected, playlists_selected))
    return;
  /* There's been a change */
  playlists_selected = selected;
  if(playlists_selected) {
    fprintf(stderr, "playlists selection changed\n'"); /* TODO */
    gtk_widget_set_sensitive(playlists_delete_button, 1);
  } else
    gtk_widget_set_sensitive(playlists_delete_button, 0);
}

/** @brief Called when the 'add' button is pressed */
static void playlists_add(GtkButton attribute((unused)) *button,
                          gpointer attribute((unused)) userdata) {
  /* Unselect whatever is selected */
  gtk_tree_selection_unselect_all(playlists_selection);
  fprintf(stderr, "playlists_add\n");/* TODO */
}

/** @brief Called when the 'Delete' button is pressed */
static void playlists_delete(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata) {
  GtkWidget *yesno;
  int res;

  if(!playlists_selected)
    return;                             /* shouldn't happen */
  yesno = gtk_message_dialog_new(GTK_WINDOW(playlists_window),
                                 GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION,
                                 GTK_BUTTONS_YES_NO,
                                 "Do you really want to delete user %s?"
                                 " This action cannot be undone.",
                                 playlists_selected);
  res = gtk_dialog_run(GTK_DIALOG(yesno));
  gtk_widget_destroy(yesno);
  if(res == GTK_RESPONSE_YES) {
    disorder_eclient_playlist_delete(client,
                                     NULL/*playlists_delete_completed*/,
                                     playlists_selected,
                                     NULL);
  }
}

/** @brief Table of buttons below the playlist list */
static struct button playlists_buttons[] = {
  {
    GTK_STOCK_ADD,
    playlists_add,
    "Create a new playlist",
    0
  },
  {
    GTK_STOCK_REMOVE,
    playlists_delete,
    "Delete a playlist",
    0
  },
};
#define NPLAYLISTS_BUTTONS (sizeof playlists_buttons / sizeof *playlists_buttons)

/** @brief Keypress handler */
static gboolean playlists_keypress(GtkWidget attribute((unused)) *widget,
                                   GdkEventKey *event,
                                   gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_Escape:
    gtk_widget_destroy(playlists_window);
    return TRUE;
  default:
    return FALSE;
  }
}

void edit_playlists(gpointer attribute((unused)) callback_data,
                     guint attribute((unused)) callback_action,
                     GtkWidget attribute((unused)) *menu_item) {
  GtkWidget *tree, *hbox, *vbox, *buttons;
  GtkCellRenderer *cr;
  GtkTreeViewColumn *col;

  /* If the window already exists, raise it */
  if(playlists_window) {
    gtk_window_present(GTK_WINDOW(playlists_window));
    return;
  }
  /* Create the window */
  playlists_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(playlists_window, tool_style);
  g_signal_connect(playlists_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &playlists_window);
  gtk_window_set_title(GTK_WINDOW(playlists_window), "Playlists Management");
  /* TODO loads of this is very similar to (copied from!) users.c - can we
   * de-dupe? */
  /* Keyboard shortcuts */
  g_signal_connect(playlists_window, "key-press-event",
                   G_CALLBACK(playlists_keypress), 0);
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(playlists_window), 240, 240);
  /* Create the list of playlist and populate it */
  playlists_fill();
  /* Create the tree view */
  tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlists_list));
  /* ...and the renderers for it */
  cr = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes("Playlist",
						 cr,
						 "text", 0,
						 NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
  /* Get the selection for the view; set its mode; arrange for a callback when
   * it changes */
  playlists_selected = NULL;
  playlists_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
  gtk_tree_selection_set_mode(playlists_selection, GTK_SELECTION_BROWSE);
  g_signal_connect(playlists_selection, "changed",
                   G_CALLBACK(playlists_selection_changed), NULL);

  /* Create the control buttons */
  buttons = create_buttons_box(playlists_buttons,
			       NPLAYLISTS_BUTTONS,
			       gtk_hbox_new(FALSE, 1));
  playlists_delete_button = playlists_buttons[1].widget;

  /* Buttons live below the list */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scroll_widget(tree), TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE/*expand*/, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE/*expand*/, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_event_box_new(), FALSE/*expand*/, FALSE, 2);
  // TODO something to edit the playlist in
  //gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_container_add(GTK_CONTAINER(playlists_window), frame_widget(hbox, NULL));
  gtk_widget_show_all(playlists_window);
}

/** @brief Initialize playlist support */
void playlists_init(void) {
  /* We re-get all playlists upon any change... */
  event_register("playlist-created", playlists_update, 0);
  event_register("playlist-modified", playlists_update, 0);
  event_register("playlist-deleted", playlists_update, 0);
  /* ...and on reconnection */
  event_register("log-connected", playlists_update, 0);
  /* ...and from time to time */
  event_register("periodic-slow", playlists_update, 0);
  /* ...and at startup */
  event_register("playlists-updated", menu_playlists_changed, 0);
  playlists_update(0, 0, 0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
