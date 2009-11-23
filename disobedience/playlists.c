/*
 * This file is part of DisOrder
 * Copyright (C) 2008, 2009 Richard Kettlewell
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
#include "queue-generic.h"
#include "popup.h"
#include "validity.h"

#if PLAYLISTS

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

/** @brief Columns for the playlist editor */
static const struct queue_column playlist_columns[] = {
  { "Artist", column_namepart, "artist", COL_EXPAND|COL_ELLIPSIZE },
  { "Album",  column_namepart, "album",  COL_EXPAND|COL_ELLIPSIZE },
  { "Title",  column_namepart, "title",  COL_EXPAND|COL_ELLIPSIZE },
  { "Length", column_length,   0,        COL_RIGHT }
};

/** @brief Pop-up menu for playlist editor */
// TODO some of these may not be generic enough yet - check!
static struct menuitem playlist_menuitems[] = {
  { "Track properties", ql_properties_activate, ql_properties_sensitive, 0, 0 },
  { "Play track", ql_play_activate, ql_play_sensitive, 0, 0 },
  //{ "Play playlist", ql_playall_activate, ql_playall_sensitive, 0, 0 },
  { "Remove track from queue", ql_remove_activate, ql_remove_sensitive, 0, 0 },
  { "Select all tracks", ql_selectall_activate, ql_selectall_sensitive, 0, 0 },
  { "Deselect all tracks", ql_selectnone_activate, ql_selectnone_sensitive, 0, 0 },
};

/** @brief Queuelike for editing a playlist */
static struct queuelike ql_playlist = {
  .name = "playlist",
  .columns = playlist_columns,
  .ncolumns = sizeof playlist_columns / sizeof *playlist_columns,
  .menuitems = playlist_menuitems,
  .nmenuitems = sizeof playlist_menuitems / sizeof *playlist_menuitems,
  //.drop = playlist_drop  //TODO
};

/* Maintaining the list of playlists ---------------------------------------- */

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

/* Playlists menu ----------------------------------------------------------- */

/** @brief Play received playlist contents
 *
 * Passed as a completion callback by menu_activate_playlist().
 */
static void playlist_play_content(void attribute((unused)) *v,
                                  const char *err,
                                  int nvec, char **vec) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  for(int n = 0; n < nvec; ++n)
    disorder_eclient_play(client, vec[n], NULL, NULL);
}

/** @brief Called to activate a playlist
 *
 * Called when the menu item for a playlist is clicked.
 */
static void menu_activate_playlist(GtkMenuItem *menuitem,
                                   gpointer attribute((unused)) user_data) {
  GtkLabel *label = GTK_LABEL(GTK_BIN(menuitem)->child);
  const char *playlist = gtk_label_get_text(label);

  disorder_eclient_playlist_get(client, playlist_play_content, playlist, NULL);
}

/** @brief Called when the playlists change */
static void menu_playlists_changed(const char attribute((unused)) *event,
                                   void attribute((unused)) *eventdata,
                                   void attribute((unused)) *callbackdata) {
  if(!playlists_menu)
    return;                             /* OMG too soon */
  GtkMenuShell *menu = GTK_MENU_SHELL(playlists_menu);
  /* TODO: we could be more sophisticated and only insert/remove widgets as
   * needed.  The current logic trashes the selection which is not acceptable
   * and interacts badly with one playlist being currently locked and
   * edited. */
  while(menu->children)
    gtk_container_remove(GTK_CONTAINER(menu), GTK_WIDGET(menu->children->data));
  /* NB nplaylists can be -1 as well as 0 */
  for(int n = 0; n < nplaylists; ++n) {
    GtkWidget *w = gtk_menu_item_new_with_label(playlists[n]);
    g_signal_connect(w, "activate", G_CALLBACK(menu_activate_playlist), 0);
    gtk_widget_show(w);
    gtk_menu_shell_append(menu, w);
  }
  gtk_widget_set_sensitive(menu_playlists_widget,
                           nplaylists > 0);
  gtk_widget_set_sensitive(menu_editplaylists_widget,
                           nplaylists >= 0);
}

/* Popup to create a new playlist ------------------------------------------- */

/** @brief New-playlist popup */
static GtkWidget *playlist_new_window;

/** @brief Text entry in new-playlist popup */
static GtkWidget *playlist_new_entry;

static GtkWidget *playlist_new_info;

static GtkWidget *playlist_new_shared;
static GtkWidget *playlist_new_public;
static GtkWidget *playlist_new_private;

/** @brief Get entered new-playlist details */
static void playlist_new_details(char **namep,
                                 char **fullnamep,
                                 gboolean *sharedp,
                                 gboolean *publicp,
                                 gboolean *privatep) {
  gboolean shared, public, private;
  g_object_get(playlist_new_shared, "active", &shared, (char *)NULL);
  g_object_get(playlist_new_public, "active", &public, (char *)NULL);
  g_object_get(playlist_new_private, "active", &private, (char *)NULL);
  char *gname = gtk_editable_get_chars(GTK_EDITABLE(playlist_new_entry),
                                       0, -1); /* name owned by calle */
  char *name = xstrdup(gname);
  g_free(gname);
  if(sharedp) *sharedp = shared;
  if(publicp) *publicp = public;
  if(privatep) *privatep = private;
  if(namep) *namep = name;
  if(fullnamep) {
    if(*sharedp) *fullnamep = *namep;
    else byte_xasprintf(fullnamep, "%s.%s", config->username, name);
  }
}

/** @brief Called when the newly created playlist has unlocked */
static void playlist_new_unlocked(void attribute((unused)) *v, const char *err) {
  if(err)
    popup_protocol_error(0, err);
  gtk_widget_destroy(playlist_new_window);
}

/** @brief Called when the new playlist has been created */
static void playlist_new_created(void attribute((unused)) *v, const char *err) {
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  disorder_eclient_playlist_unlock(client, playlist_new_unlocked, NULL);
  // TODO arrange for the new playlist to be selected
}

/** @brief Called when the proposed new playlist's contents have been retrieved
 *
 * ...or rather, normally, when it's been reported that it does not exist.
 */
static void playlist_new_retrieved(void *v, const char *err,
                                   int nvec,
                                   char attribute((unused)) **vec) {
  char *fullname = v;
  if(!err && nvec != -1)
    /* A rare case but not in principle impossible */
    err = "A playlist with that name already exists.";
  if(err) {
    popup_protocol_error(0, err);
    disorder_eclient_playlist_unlock(client, playlist_new_unlocked, fullname);
    return;
  }
  gboolean shared, public, private;
  playlist_new_details(0, 0, &shared, &public, &private);
  disorder_eclient_playlist_set_share(client, playlist_new_created, fullname,
                                      public ? "public"
                                      : private ? "private"
                                      : "shared",
                                      fullname);
}

/** @brief Called when the proposed new playlist has been locked */
static void playlist_new_locked(void *v, const char *err) {
  char *fullname = v;
  if(err) {
    popup_protocol_error(0, err);
    return;
  }
  disorder_eclient_playlist_get(client, playlist_new_retrieved,
                                fullname, fullname);
}

/** @brief Called when 'ok' is clicked in new-playlist popup */
static void playlist_new_ok(GtkButton attribute((unused)) *button,
                            gpointer attribute((unused)) userdata) {
  gboolean shared, public, private;
  char *name, *fullname;
  playlist_new_details(&name, &fullname, &shared, &public, &private);

  /* We need to:
   * - lock the playlist
   * - check it doesn't exist
   * - set sharing (which will create it empty
   * - unlock it
   *
   * TODO we should freeze the window while this is going on
   */
  disorder_eclient_playlist_lock(client, playlist_new_locked, fullname,
                                 fullname);
}

/** @brief Called when 'cancel' is clicked in new-playlist popup */
static void playlist_new_cancel(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(playlist_new_window);
}

/** @brief Buttons for new-playlist popup */
static struct button playlist_new_buttons[] = {
  {
    .stock = GTK_STOCK_OK,
    .clicked = playlist_new_ok,
    .tip = "Create new playlist"
  },
  {
    .stock = GTK_STOCK_CANCEL,
    .clicked = playlist_new_cancel,
    .tip = "Do not create new playlist"
  }
};
#define NPLAYLIST_NEW_BUTTONS (sizeof playlist_new_buttons / sizeof *playlist_new_buttons)

/** @brief Test whether the new-playlist window settings are valid
 * @return NULL on success or an error string if not
 */
static const char *playlist_new_valid(void) {
  gboolean shared, public, private;
  char *name, *fullname;
  playlist_new_details(&name, &fullname, &shared, &public, &private);
  if(!(shared || public || private))
    return "No type set.";
  if(!*name)
    return "";
  /* See if the result is valid */
  if(!valid_username(name)
     || playlist_parse_name(fullname, NULL, NULL))
    return "Not a valid playlist name.";
  /* See if the result clashes with an existing name */
  for(int n = 0; n < nplaylists; ++n)
    if(!strcmp(playlists[n], fullname)) {
      if(shared)
        return "A shared playlist with that name already exists.";
      else
        return "You already have a playlist with that name.";
    }
  /* As far as we can tell creation would work */
  return NULL;
}

/** @brief Called to update new playlist window state
 *
 * This is called whenever one the text entry or radio buttons changed, and
 * also when the set of known playlists changes.  It determines whether the new
 * playlist would be creatable and sets the sensitivity of the OK button
 * and info display accordingly.
 */
static void playlist_new_changed(const char attribute((unused)) *event,
                                 void attribute((unused)) *eventdata,
                                 void attribute((unused)) *callbackdata) {
  if(!playlist_new_window)
    return;
  const char *reason = playlist_new_valid();
  gtk_widget_set_sensitive(playlist_new_buttons[0].widget,
                           !reason);
  gtk_label_set_text(GTK_LABEL(playlist_new_info), reason);
}

/** @brief Called when some radio button in the new-playlist popup changes */
static void playlist_new_button_toggled(GtkToggleButton attribute((unused)) tb,
                                        gpointer attribute((unused)) userdata) {
  playlist_new_changed(0,0,0);
}

/** @brief Called when the text entry field in the new-playlist popup changes */
static void playlist_new_entry_edited(GtkEditable attribute((unused)) *editable,
                                      gpointer attribute((unused)) user_data) {
  playlist_new_changed(0,0,0);
}

/** @brief Pop up a new window to enter the playlist name and details */
static void playlist_new(void) {
  assert(playlist_new_window == NULL);
  playlist_new_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(playlist_new_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &playlist_new_window);
  gtk_window_set_title(GTK_WINDOW(playlist_new_window), "Create new playlist");
  /* Window will be modal, suppressing access to other windows */
  gtk_window_set_modal(GTK_WINDOW(playlist_new_window), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(playlist_new_window),
                               GTK_WINDOW(playlists_window));

  /* Window contents will use a table (grid) layout */
  GtkWidget *table = gtk_table_new(3, 3, FALSE/*!homogeneous*/);

  /* First row: playlist name */
  gtk_table_attach_defaults(GTK_TABLE(table),
                            gtk_label_new("Playlist name"),
                            0, 1, 0, 1);
  playlist_new_entry = gtk_entry_new();
  g_signal_connect(playlist_new_entry, "changed",
                   G_CALLBACK(playlist_new_entry_edited), NULL);
  gtk_table_attach_defaults(GTK_TABLE(table),
                            playlist_new_entry,
                            1, 3, 0, 1);

  /* Second row: radio buttons to choose type */
  playlist_new_shared = gtk_radio_button_new_with_label(NULL, "shared");
  playlist_new_public
    = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(playlist_new_shared),
                                                  "public");
  playlist_new_private
    = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(playlist_new_shared),
                                                  "private");
  g_signal_connect(playlist_new_shared, "toggled",
                   G_CALLBACK(playlist_new_button_toggled), NULL);
  g_signal_connect(playlist_new_public, "toggled",
                   G_CALLBACK(playlist_new_button_toggled), NULL);
  g_signal_connect(playlist_new_private, "toggled",
                   G_CALLBACK(playlist_new_button_toggled), NULL);
  gtk_table_attach_defaults(GTK_TABLE(table), playlist_new_shared, 0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), playlist_new_public, 1, 2, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), playlist_new_private, 2, 3, 1, 2);

  /* Third row: info bar saying why not */
  playlist_new_info = gtk_label_new("");
  gtk_table_attach_defaults(GTK_TABLE(table), playlist_new_info,
                            0, 3, 2, 3);

  /* Fourth row: ok/cancel buttons */
  GtkWidget *hbox = create_buttons_box(playlist_new_buttons,
                                       NPLAYLIST_NEW_BUTTONS,
                                       gtk_hbox_new(FALSE, 0));
  gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 3, 3, 4);

  gtk_container_add(GTK_CONTAINER(playlist_new_window),
                    frame_widget(table, NULL));

  /* Set initial state of OK button */
  playlist_new_changed(0,0,0);

  /* Display the window */
  gtk_widget_show_all(playlist_new_window);
}

/* Playlists window (list of playlists) ------------------------------------- */

/** @brief (Re-)populate the playlist tree model */
static void playlists_fill(const char attribute((unused)) *event,
                           void attribute((unused)) *eventdata,
                           void attribute((unused)) *callbackdata) {
  GtkTreeIter iter[1];

  if(!playlists_window)
    return;
  if(!playlists_list)
    playlists_list = gtk_list_store_new(1, G_TYPE_STRING);
  const char *was_selected = playlists_selected;
  gtk_list_store_clear(playlists_list); /* clears playlists_selected */
  for(int n = 0; n < nplaylists; ++n) {
    gtk_list_store_insert_with_values(playlists_list, iter, n/*position*/,
                                      0, playlists[n],        /* column 0 */
                                      -1);                    /* no more cols */
    /* Reselect the selected playlist */
    if(was_selected && !strcmp(was_selected, playlists[n]))
      gtk_tree_selection_select_iter(playlists_selection, iter);
  }
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
  /* Set button sensitivity according to the new state */
  if(selected)
    gtk_widget_set_sensitive(playlists_delete_button, 1);
  else
    gtk_widget_set_sensitive(playlists_delete_button, 0);
  /* Eliminate no-change cases */
  if(!selected && !playlists_selected)
    return;
  if(selected && playlists_selected && !strcmp(selected, playlists_selected))
    return;
  /* Record the new state */
  playlists_selected = selected;
}

/** @brief Called when the 'add' button is pressed */
static void playlists_add(GtkButton attribute((unused)) *button,
                          gpointer attribute((unused)) userdata) {
  /* Unselect whatever is selected TODO why?? */
  gtk_tree_selection_unselect_all(playlists_selection);
  playlist_new();
}

/** @brief Called when playlist deletion completes */
static void playlists_delete_completed(void attribute((unused)) *v,
                                       const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

/** @brief Called when the 'Delete' button is pressed */
static void playlists_delete(GtkButton attribute((unused)) *button,
                             gpointer attribute((unused)) userdata) {
  GtkWidget *yesno;
  int res;

  fprintf(stderr, "playlists_delete\n");
  if(!playlists_selected)
    return;                             /* shouldn't happen */
  yesno = gtk_message_dialog_new(GTK_WINDOW(playlists_window),
                                 GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION,
                                 GTK_BUTTONS_YES_NO,
                                 "Do you really want to delete playlist %s?"
                                 " This action cannot be undone.",
                                 playlists_selected);
  res = gtk_dialog_run(GTK_DIALOG(yesno));
  gtk_widget_destroy(yesno);
  if(res == GTK_RESPONSE_YES) {
    disorder_eclient_playlist_delete(client,
                                     playlists_delete_completed,
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

/** @brief Create the list of playlists for the edit playlists window */
static GtkWidget *playlists_window_list(void) {
  /* Create the list of playlist and populate it */
  playlists_fill(NULL, NULL, NULL);
  /* Create the tree view */
  GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlists_list));
  /* ...and the renderers for it */
  GtkCellRenderer *cr = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes("Playlist",
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
  GtkWidget *buttons = create_buttons_box(playlists_buttons,
                                          NPLAYLISTS_BUTTONS,
                                          gtk_hbox_new(FALSE, 1));
  playlists_delete_button = playlists_buttons[1].widget;

  playlists_selection_changed(NULL, NULL);

  /* Buttons live below the list */
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scroll_widget(tree), TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE/*expand*/, FALSE, 0);

  return vbox;
}

/* Playlists window (edit current playlist) --------------------------------- */

static GtkWidget *playlists_window_edit(void) {
  assert(ql_playlist.view == NULL);     /* better not be set up already */
  GtkWidget *w = init_queuelike(&ql_playlist);
  return w;
}

/* Playlists window --------------------------------------------------------- */

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

/** @brief Called when the playlist window is destroyed */
static void playlists_window_destroyed(GtkWidget attribute((unused)) *widget,
                                       GtkWidget **widget_pointer) {
  destroy_queuelike(&ql_playlist);
  *widget_pointer = NULL;
}

/** @brief Pop up the playlists window
 *
 * Called when the playlists menu item is selected
 */
void edit_playlists(gpointer attribute((unused)) callback_data,
                    guint attribute((unused)) callback_action,
                    GtkWidget attribute((unused)) *menu_item) {
  /* If the window already exists, raise it */
  if(playlists_window) {
    gtk_window_present(GTK_WINDOW(playlists_window));
    return;
  }
  /* Create the window */
  playlists_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(playlists_window, tool_style);
  g_signal_connect(playlists_window, "destroy",
		   G_CALLBACK(playlists_window_destroyed), &playlists_window);
  gtk_window_set_title(GTK_WINDOW(playlists_window), "Playlists Management");
  /* TODO loads of this is very similar to (copied from!) users.c - can we
   * de-dupe? */
  /* Keyboard shortcuts */
  g_signal_connect(playlists_window, "key-press-event",
                   G_CALLBACK(playlists_keypress), 0);
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(playlists_window), 512, 240);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), playlists_window_list(),
                     FALSE/*expand*/, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_event_box_new(),
                     FALSE/*expand*/, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), playlists_window_edit(),
                     TRUE/*expand*/, TRUE/*fill*/, 0);

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

  /* Update the playlists menu when the set of playlists changes */
  event_register("playlists-updated", menu_playlists_changed, 0);
  /* Update the new-playlist OK button when the set of playlists changes */
  event_register("playlists-updated", playlist_new_changed, 0);
  /* Update the list of playlists in the edit window when the set changes */
  event_register("playlists-updated", playlists_fill, 0);
  playlists_update(0, 0, 0);
}

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
