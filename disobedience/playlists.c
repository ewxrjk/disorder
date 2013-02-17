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
 * @brief Playlist support for Disobedience
 *
 * The playlists management window contains:
 * - the playlist picker (a list of all playlists) TODO should be a tree!
 * - an add button
 * - a delete button
 * - the playlist editor (a d+d-capable view of the currently picked playlist)
 * - a close button   TODO
 *
 * This file also maintains the playlist menu, allowing playlists to be
 * activated from the main window's menu.
 *
 * Internally we maintain the playlist list, which is just the current list of
 * playlists.  Changes to this are reflected in the playlist menu and the
 * playlist picker.
 *
 */
#include "disobedience.h"
#include "queue-generic.h"
#include "popup.h"
#include "validity.h"

static void playlist_list_received_playlists(void *v,
                                             const char *err,
                                             int nvec, char **vec);
static void playlist_editor_fill(const char *event,
                                 void *eventdata,
                                 void *callbackdata);
static int playlist_playall_sensitive(void *extra);
static void playlist_playall_activate(GtkMenuItem *menuitem,
                                      gpointer user_data);
static int playlist_remove_sensitive(void *extra) ;
static void playlist_remove_activate(GtkMenuItem *menuitem,
                                     gpointer user_data);
static void playlist_new_locked(void *v, const char *err);
static void playlist_new_retrieved(void *v, const char *err,
                                   int nvec,
                                   char **vec);
static void playlist_new_created(void *v, const char *err);
static void playlist_new_unlocked(void *v, const char *err);
static void playlist_new_entry_edited(GtkEditable *editable,
                                      gpointer user_data);
static void playlist_new_button_toggled(GtkToggleButton *tb,
                                        gpointer userdata);
static void playlist_new_changed(const char *event,
                                 void *eventdata,
                                 void *callbackdata);
static const char *playlist_new_valid(void);
static void playlist_new_details(char **namep,
                                 char **fullnamep,
                                 gboolean *sharedp,
                                 gboolean *publicp,
                                 gboolean *privatep);
static void playlist_new_ok(GtkButton *button,
                            gpointer userdata);
static void playlist_new_cancel(GtkButton *button,
                                gpointer userdata);
static void playlists_editor_received_tracks(void *v,
                                             const char *err,
                                             int nvec, char **vec);
static void playlist_window_destroyed(GtkWidget *widget,
                                      GtkWidget **widget_pointer);
static gboolean playlist_window_keypress(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer user_data);
static int playlistcmp(const void *ap, const void *bp);
static void playlist_modify_locked(void *v, const char *err);
void playlist_modify_retrieved(void *v, const char *err,
                               int nvec,
                               char **vec);
static void playlist_modify_updated(void *v, const char *err);
static void playlist_modify_unlocked(void *v, const char *err);
static void playlist_drop(struct queuelike *ql,
                          int ntracks,
                          char **tracks, char **ids,
                          struct queue_entry *after_me);
struct playlist_modify_data;
static void playlist_drop_modify(struct playlist_modify_data *mod,
                                 int nvec, char **vec);
static void playlist_remove_modify(struct playlist_modify_data *mod,
                                 int nvec, char **vec);
static gboolean playlist_new_keypress(GtkWidget *widget,
                                      GdkEventKey *event,
                                      gpointer user_data);
static gboolean playlist_picker_keypress(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer user_data);
static void playlist_editor_button_toggled(GtkToggleButton *tb,
                                           gpointer userdata);
static void playlist_editor_set_buttons(const char *event,
                                        void *eventdata,
                                        void *callbackdata);
static void playlist_editor_got_share(void *v,
                                      const char *err,
                                      const char *value);
static void playlist_editor_share_set(void *v, const char *err);
static void playlist_picker_update_section(const char *title, const char *key,
                                           int start, int end);
static gboolean playlist_picker_find(GtkTreeIter *parent,
                                     const char *title, const char *key,
                                     GtkTreeIter iter[1],
                                     gboolean create);
static void playlist_picker_delete_obsolete(GtkTreeIter parent[1],
                                            char **exists,
                                            int nexists);
static gboolean playlist_picker_button(GtkWidget *widget,
                                       GdkEventButton *event,
                                       gpointer user_data);
static gboolean playlist_editor_keypress(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer user_data);
static void playlist_editor_ok(GtkButton *button, gpointer userdata);
static void playlist_editor_help(GtkButton *button, gpointer userdata);

/** @brief Playlist editing window */
static GtkWidget *playlist_window;

/** @brief Columns for the playlist editor */
static const struct queue_column playlist_columns[] = {
  { "Artist", column_namepart, "artist", COL_EXPAND|COL_ELLIPSIZE },
  { "Album",  column_namepart, "album",  COL_EXPAND|COL_ELLIPSIZE },
  { "Title",  column_namepart, "title",  COL_EXPAND|COL_ELLIPSIZE },
};

/** @brief Pop-up menu for playlist editor
 *
 * Status:
 * - track properties works but, bizarrely, raises the main window
 * - play track works
 * - play playlist works
 * - select/deselect all work
 */
static struct menuitem playlist_menuitems[] = {
  { "Track properties", GTK_STOCK_PROPERTIES, ql_properties_activate, ql_properties_sensitive, 0, 0 },
  { "Play track", GTK_STOCK_MEDIA_PLAY, ql_play_activate, ql_play_sensitive, 0, 0 },
  { "Play playlist", NULL, playlist_playall_activate, playlist_playall_sensitive, 0, 0 },
  { "Remove track from playlist", GTK_STOCK_DELETE, playlist_remove_activate, playlist_remove_sensitive, 0, 0 },
  { "Select all tracks", GTK_STOCK_SELECT_ALL, ql_selectall_activate, ql_selectall_sensitive, 0, 0 },
  { "Deselect all tracks", NULL, ql_selectnone_activate, ql_selectnone_sensitive, 0, 0 },
};

static const GtkTargetEntry playlist_targets[] = {
  {
    PLAYLIST_TRACKS,                    /* drag type */
    GTK_TARGET_SAME_WIDGET,             /* rearrangement within a widget */
    PLAYLIST_TRACKS_ID                  /* ID value */
  },
  {
    PLAYABLE_TRACKS,                             /* drag type */
    GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET, /* copying between widgets */
    PLAYABLE_TRACKS_ID,                          /* ID value */
  },
  {
    .target = NULL
  }
};

/** @brief Queuelike for editing a playlist */
static struct queuelike ql_playlist = {
  .name = "playlist",
  .columns = playlist_columns,
  .ncolumns = sizeof playlist_columns / sizeof *playlist_columns,
  .menuitems = playlist_menuitems,
  .nmenuitems = sizeof playlist_menuitems / sizeof *playlist_menuitems,
  .drop = playlist_drop,
  .drag_source_targets = playlist_targets,
  .drag_source_actions = GDK_ACTION_MOVE|GDK_ACTION_COPY,
  .drag_dest_targets = playlist_targets,
  .drag_dest_actions = GDK_ACTION_MOVE|GDK_ACTION_COPY,
};

/* Maintaining the list of playlists ---------------------------------------- */

/** @brief Current list of playlists or NULL */
char **playlists;

/** @brief Count of playlists */
int nplaylists;

/** @brief Schedule an update to the list of playlists
 *
 * Called periodically and when a playlist is created or deleted.
 */
static void playlist_list_update(const char attribute((unused)) *event,
                                 void attribute((unused)) *eventdata,
                                 void attribute((unused)) *callbackdata) {
  disorder_eclient_playlists(client, playlist_list_received_playlists, 0);
}

/** @brief Called with a new list of playlists */
static void playlist_list_received_playlists(void attribute((unused)) *v,
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

/* Playlists menu ----------------------------------------------------------- */

static void playlist_menu_playing(void attribute((unused)) *v,
                                  const char *err,
                                  const char attribute((unused)) *id) {
  if(err)
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
}

/** @brief Play received playlist contents
 *
 * Passed as a completion callback by menu_activate_playlist().
 */
static void playlist_menu_received_content(void attribute((unused)) *v,
                                           const char *err,
                                           int nvec, char **vec) {
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    return;
  }
  for(int n = 0; n < nvec; ++n)
    disorder_eclient_play(client, playlist_menu_playing, vec[n], NULL);
}

/** @brief Called to activate a playlist
 *
 * Called when the menu item for a playlist is clicked.
 */
static void playlist_menu_activate(GtkMenuItem *menuitem,
                                   gpointer attribute((unused)) user_data) {
  GtkLabel *label = GTK_LABEL(GTK_BIN(menuitem)->child);
  const char *playlist = gtk_label_get_text(label);

  disorder_eclient_playlist_get(client, playlist_menu_received_content,
                                playlist, NULL);
}

/** @brief Called when the playlists change
 *
 * Naively refills the menu.  The results might be unsettling if the menu is
 * currently open, but this is hopefuly fairly rare.
 */
static void playlist_menu_changed(const char attribute((unused)) *event,
                                  void attribute((unused)) *eventdata,
                                  void attribute((unused)) *callbackdata) {
  if(!playlists_menu)
    return;                             /* OMG too soon */
  GtkMenuShell *menu = GTK_MENU_SHELL(playlists_menu);
  while(menu->children)
    gtk_container_remove(GTK_CONTAINER(menu), GTK_WIDGET(menu->children->data));
  /* NB nplaylists can be -1 as well as 0 */
  for(int n = 0; n < nplaylists; ++n) {
    GtkWidget *w = gtk_menu_item_new_with_label(playlists[n]);
    g_signal_connect(w, "activate", G_CALLBACK(playlist_menu_activate), 0);
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

/** @brief Label for displaying feedback on what's wrong */
static GtkWidget *playlist_new_info;

/** @brief "Shared" radio button */
static GtkWidget *playlist_new_shared;

/** @brief "Public" radio button */
static GtkWidget *playlist_new_public;

/** @brief "Private" radio button */
static GtkWidget *playlist_new_private;

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

/** @brief Pop up a new window to enter the playlist name and details */
static void playlist_new_playlist(void) {
  assert(playlist_new_window == NULL);
  playlist_new_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(playlist_new_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &playlist_new_window);
  gtk_window_set_title(GTK_WINDOW(playlist_new_window), "Create new playlist");
  /* Window will be modal, suppressing access to other windows */
  gtk_window_set_modal(GTK_WINDOW(playlist_new_window), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(playlist_new_window),
                               GTK_WINDOW(playlist_window));

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

  g_signal_connect(playlist_new_window, "key-press-event",
                   G_CALLBACK(playlist_new_keypress), 0);
  
  /* Display the window */
  gtk_widget_show_all(playlist_new_window);
}

/** @brief Keypress handler */
static gboolean playlist_new_keypress(GtkWidget attribute((unused)) *widget,
                                      GdkEventKey *event,
                                      gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_Return:
    playlist_new_ok(NULL, NULL);
    return TRUE;
  case GDK_Escape:
    gtk_widget_destroy(playlist_new_window);
    return TRUE;
  default:
    return FALSE;
  }
}

/** @brief Called when 'ok' is clicked in new-playlist popup */
static void playlist_new_ok(GtkButton attribute((unused)) *button,
                            gpointer attribute((unused)) userdata) {
  if(playlist_new_valid())
    return;
  gboolean shared, public, private;
  char *name, *fullname;
  playlist_new_details(&name, &fullname, &shared, &public, &private);

  /* We need to:
   * - lock the playlist
   * - check it doesn't exist
   * - set sharing (which will create it empty
   * - unlock it
   *
   * TODO we should freeze the window while this is going on to stop a second
   * click.
   */
  disorder_eclient_playlist_lock(client, playlist_new_locked, fullname,
                                 fullname);
}

/** @brief Called when the proposed new playlist has been locked */
static void playlist_new_locked(void *v, const char *err) {
  char *fullname = v;
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    return;
  }
  disorder_eclient_playlist_get(client, playlist_new_retrieved,
                                fullname, fullname);
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
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
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

/** @brief Called when the new playlist has been created */
static void playlist_new_created(void attribute((unused)) *v, const char *err) {
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    return;
  }
  disorder_eclient_playlist_unlock(client, playlist_new_unlocked, NULL);
  // TODO arrange for the new playlist to be selected
}

/** @brief Called when the newly created playlist has unlocked */
static void playlist_new_unlocked(void attribute((unused)) *v, const char *err) {
  if(err)
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
  /* Pop down the creation window */
  gtk_widget_destroy(playlist_new_window);
}

/** @brief Called when 'cancel' is clicked in new-playlist popup */
static void playlist_new_cancel(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(playlist_new_window);
}

/** @brief Called when some radio button in the new-playlist popup changes */
static void playlist_new_button_toggled(GtkToggleButton attribute((unused)) *tb,
                                        gpointer attribute((unused)) userdata) {
  playlist_new_changed(0,0,0);
}

/** @brief Called when the text entry field in the new-playlist popup changes */
static void playlist_new_entry_edited(GtkEditable attribute((unused)) *editable,
                                      gpointer attribute((unused)) user_data) {
  playlist_new_changed(0,0,0);
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
  /* See if the result clashes with an existing name.  This is not a perfect
   * check, the playlist might be created after this point but before we get a
   * chance to disable the "OK" button.  However when we try to create the
   * playlist we will first try to retrieve it, with a lock held, so we
   * shouldn't end up overwriting anything. */
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

/** @brief Get entered new-playlist details
 * @param namep Where to store entered name (or NULL)
 * @param fullnamep Where to store computed full name (or NULL)
 * @param sharedp Where to store 'shared' flag (or NULL)
 * @param publicp Where to store 'public' flag (or NULL)
 * @param privatep Where to store 'private' flag (or NULL)
 */
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
    if(shared) *fullnamep = name;
    else byte_xasprintf(fullnamep, "%s.%s", config->username, name);
  }
}

/* Playlist picker ---------------------------------------------------------- */

/** @brief Delete button */
static GtkWidget *playlist_picker_delete_button;

/** @brief Tree model for list of playlists
 *
 * This has two columns:
 * - column 0 will be the display name
 * - column 1 will be the sort key/playlist name (and will not be displayed)
 */
static GtkTreeStore *playlist_picker_list;

/** @brief Selection for list of playlists */
static GtkTreeSelection *playlist_picker_selection;

/** @brief Currently selected playlist */
static const char *playlist_picker_selected;

/** @brief (Re-)populate the playlist picker tree model */
static void playlist_picker_fill(const char attribute((unused)) *event,
                                 void attribute((unused)) *eventdata,
                                 void attribute((unused)) *callbackdata) {
  if(!playlist_window)
    return;
  if(!playlist_picker_list)
    playlist_picker_list = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  /* We will accumulate a list of all the sections that exist */
  char **sections = xcalloc(nplaylists, sizeof (char *));
  int nsections = 0;
  /* Make sure shared playlists are there */
  int start = 0, end;
  for(end = start; end < nplaylists && !strchr(playlists[end], '.'); ++end)
    ;
  if(start != end) {
    playlist_picker_update_section("Shared playlists", "",
                                   start, end);
    sections[nsections++] = (char *)"";
  }
  /* Make sure owned playlists are there */
  while((start = end) < nplaylists) {
    const int nl = strchr(playlists[start], '.') - playlists[start];
    char *name = xstrndup(playlists[start], nl);
    for(end = start;
        end < nplaylists
          && playlists[end][nl] == '.'
          && !strncmp(playlists[start], playlists[end], nl);
        ++end)
      ;
    playlist_picker_update_section(name, name, start, end);
    sections[nsections++] = name;
  }
  /* Delete obsolete sections */
  playlist_picker_delete_obsolete(NULL, sections, nsections);
}

/** @brief Update a section in the picker tree model
 * @param title Display name of section
 * @param key Key to search for
 * @param start First entry in @ref playlists
 * @param end Past last entry in @ref playlists
 */
static void playlist_picker_update_section(const char *title, const char *key,
                                           int start, int end) {
  /* Find the section, creating it if necessary */
  GtkTreeIter section_iter[1];
  playlist_picker_find(NULL, title, key, section_iter, TRUE);
  /* Add missing rows */
  for(int n = start; n < end; ++n) {
    GtkTreeIter child[1];
    char *name;
    if((name = strchr(playlists[n], '.')))
      ++name;
    else
      name = playlists[n];
    playlist_picker_find(section_iter,
                         name, playlists[n],
                         child,
                         TRUE);
  }
  /* Delete anything that shouldn't exist. */
  playlist_picker_delete_obsolete(section_iter, playlists + start, end - start);
}

/** @brief Find and maybe create a row in the picker tree model
 * @param parent Parent iterator (or NULL for top level)
 * @param title Display name of section
 * @param key Key to search for
 * @param iter Iterator to point at key
 * @param create Whether to create the row
 * @return TRUE if key exists else FALSE
 *
 * If the @p key exists then @p iter will point to it and TRUE will be
 * returned.
 *
 * If the @p key does not exist and @p create is TRUE then it will be created.
 * @p iter wil point to it and TRUE will be returned.
 *
 * If the @p key does not exist and @p create is FALSE then FALSE will be
 * returned.
 */
static gboolean playlist_picker_find(GtkTreeIter *parent,
                                     const char *title,
                                     const char *key,
                                     GtkTreeIter iter[1],
                                     gboolean create) {
  gchar *candidate;
  GtkTreeIter next[1];
  gboolean it;
  int row = 0;

  it = gtk_tree_model_iter_children(GTK_TREE_MODEL(playlist_picker_list),
                                    next,
                                    parent);
  while(it) {
    /* Find the value at row 'next' */
    gtk_tree_model_get(GTK_TREE_MODEL(playlist_picker_list),
                       next,
                       1, &candidate,
                       -1);
    /* See how it compares with @p key */
    int c = strcmp(key, candidate);
    g_free(candidate);
    if(!c) {
      *iter = *next;
      return TRUE;                      /* we found our key */
    }
    if(c < 0) {
      /* @p key belongs before row 'next' */
      if(create) {
        gtk_tree_store_insert_with_values(playlist_picker_list,
                                          iter,
                                          parent,
                                          row,     /* insert here */
                                          0, title, 1, key, -1);
        return TRUE;
      } else
        return FALSE;
      ++row;
    }
    it = gtk_tree_model_iter_next(GTK_TREE_MODEL(playlist_picker_list), next);
  }
  /* We have reached the end and not found a row that should be later than @p
   * key. */
  if(create) {
    gtk_tree_store_insert_with_values(playlist_picker_list,
                                      iter,
                                      parent,
                                      INT_MAX, /* insert at end */
                                      0, title, 1, key, -1);
    return TRUE;
  } else
    return FALSE;
}

/** @brief Delete obsolete rows
 * @param parent Parent or NULL
 * @param exists List of rows that should exist (by key)
 * @param nexists Length of @p exists
 */
static void playlist_picker_delete_obsolete(GtkTreeIter parent[1],
                                            char **exists,
                                            int nexists) {
  /* Delete anything that shouldn't exist. */
  GtkTreeIter iter[1];
  gboolean it = gtk_tree_model_iter_children(GTK_TREE_MODEL(playlist_picker_list),
                                             iter,
                                             parent);
  while(it) {
    /* Find the value at row 'next' */
    gchar *candidate;
    gtk_tree_model_get(GTK_TREE_MODEL(playlist_picker_list),
                       iter,
                       1, &candidate,
                       -1);
    gboolean found = FALSE;
    for(int n = 0; n < nexists; ++n)
      if((found = !strcmp(candidate, exists[n])))
        break;
    if(!found)
      it = gtk_tree_store_remove(playlist_picker_list, iter);
    else
      it = gtk_tree_model_iter_next(GTK_TREE_MODEL(playlist_picker_list),
                                    iter);
    g_free(candidate);
  }
}

/** @brief Called when the selection might have changed */
static void playlist_picker_selection_changed(GtkTreeSelection attribute((unused)) *treeselection,
                                              gpointer attribute((unused)) user_data) {
  GtkTreeIter iter;
  char *gselected, *selected;
  
  /* Identify the current selection */
  if(gtk_tree_selection_get_selected(playlist_picker_selection, 0, &iter)
     && gtk_tree_store_iter_depth(playlist_picker_list, &iter) > 0) {
    gtk_tree_model_get(GTK_TREE_MODEL(playlist_picker_list), &iter,
                       1, &gselected, -1);
    selected = xstrdup(gselected);
    g_free(gselected);
  } else
    selected = 0;
  /* Set button sensitivity according to the new state */
  int deletable = FALSE;
  if(selected) {
    if(strchr(selected, '.')) {
      if(!strncmp(selected, config->username, strlen(config->username)))
        deletable = TRUE;
    } else
      deletable = TRUE;
  }
  gtk_widget_set_sensitive(playlist_picker_delete_button, deletable);
  /* Eliminate no-change cases */
  if(!selected && !playlist_picker_selected)
    return;
  if(selected
     && playlist_picker_selected
     && !strcmp(selected, playlist_picker_selected))
    return;
  /* Record the new state */
  playlist_picker_selected = selected;
  /* Re-initalize the queue */
  ql_new_queue(&ql_playlist, NULL);
  /* Synthesize a playlist-modified to re-initialize the editor etc */
  event_raise("playlist-modified", (void *)playlist_picker_selected);
}

/** @brief Called when the 'add' button is pressed */
static void playlist_picker_add(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  /* Unselect whatever is selected TODO why?? */
  gtk_tree_selection_unselect_all(playlist_picker_selection);
  playlist_new_playlist();
}

/** @brief Called when playlist deletion completes */
static void playlists_picker_delete_completed(void attribute((unused)) *v,
                                              const char *err) {
  if(err)
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
}

/** @brief Called when the 'Delete' button is pressed */
static void playlist_picker_delete(GtkButton attribute((unused)) *button,
                                   gpointer attribute((unused)) userdata) {
  GtkWidget *yesno;
  int res;

  if(!playlist_picker_selected)
    return;
  yesno = gtk_message_dialog_new(GTK_WINDOW(playlist_window),
                                 GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION,
                                 GTK_BUTTONS_YES_NO,
                                 "Do you really want to delete playlist %s?"
                                 " This action cannot be undone.",
                                 playlist_picker_selected);
  res = gtk_dialog_run(GTK_DIALOG(yesno));
  gtk_widget_destroy(yesno);
  if(res == GTK_RESPONSE_YES) {
    disorder_eclient_playlist_delete(client,
                                     playlists_picker_delete_completed,
                                     playlist_picker_selected,
                                     NULL);
  }
}

/** @brief Table of buttons below the playlist list */
static struct button playlist_picker_buttons[] = {
  {
    GTK_STOCK_ADD,
    playlist_picker_add,
    "Create a new playlist",
    0,
    NULL,
  },
  {
    GTK_STOCK_REMOVE,
    playlist_picker_delete,
    "Delete a playlist",
    0,
    NULL,
  },
};
#define NPLAYLIST_PICKER_BUTTONS (sizeof playlist_picker_buttons / sizeof *playlist_picker_buttons)

/** @brief Create the list of playlists for the edit playlists window */
static GtkWidget *playlist_picker_create(void) {
  /* Create the list of playlist and populate it */
  playlist_picker_fill(NULL, NULL, NULL);
  /* Create the tree view */
  GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlist_picker_list));
  /* ...and the renderers for it */
  GtkCellRenderer *cr = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes("Playlist",
                                                                    cr,
                                                                    "text", 0,
                                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
  /* Get the selection for the view; set its mode; arrange for a callback when
   * it changes */
  playlist_picker_selected = NULL;
  playlist_picker_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
  gtk_tree_selection_set_mode(playlist_picker_selection, GTK_SELECTION_BROWSE);
  g_signal_connect(playlist_picker_selection, "changed",
                   G_CALLBACK(playlist_picker_selection_changed), NULL);

  /* Create the control buttons */
  GtkWidget *buttons = create_buttons_box(playlist_picker_buttons,
                                          NPLAYLIST_PICKER_BUTTONS,
                                          gtk_hbox_new(FALSE, 1));
  playlist_picker_delete_button = playlist_picker_buttons[1].widget;

  playlist_picker_selection_changed(NULL, NULL);

  /* Buttons live below the list */
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scroll_widget(tree), TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE/*expand*/, FALSE, 0);

  g_signal_connect(tree, "key-press-event",
                   G_CALLBACK(playlist_picker_keypress), 0);
  g_signal_connect(tree, "button-press-event",
                   G_CALLBACK(playlist_picker_button), 0);

  return vbox;
}

static gboolean playlist_picker_keypress(GtkWidget attribute((unused)) *widget,
                                         GdkEventKey *event,
                                         gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_BackSpace:
  case GDK_Delete:
    playlist_picker_delete(NULL, NULL);
    return TRUE;
  default:
    return FALSE;
  }
}

static void playlist_picker_select_activate(GtkMenuItem attribute((unused)) *item,
                                            gpointer attribute((unused)) userdata) {
  /* nothing */
}

static int playlist_picker_select_sensitive(void *extra) {
  GtkTreeIter *iter = extra;
  return gtk_tree_store_iter_depth(playlist_picker_list, iter) > 0;
}

static void playlist_picker_play_activate(GtkMenuItem attribute((unused)) *item,
                                          gpointer attribute((unused)) userdata) {
  /* Re-use the menu-based activation callback */
  disorder_eclient_playlist_get(client, playlist_menu_received_content,
                                playlist_picker_selected, NULL);
}

static int playlist_picker_play_sensitive(void *extra) {
  GtkTreeIter *iter = extra;
  return gtk_tree_store_iter_depth(playlist_picker_list, iter) > 0;
}

static void playlist_picker_remove_activate(GtkMenuItem attribute((unused)) *item,
                                            gpointer attribute((unused)) userdata) {
  /* Re-use the 'Remove' button' */
  playlist_picker_delete(NULL, NULL);
}

static int playlist_picker_remove_sensitive(void *extra) {
  GtkTreeIter *iter = extra;
  if(gtk_tree_store_iter_depth(playlist_picker_list, iter) > 0) {
    if(strchr(playlist_picker_selected, '.')) {
      if(!strncmp(playlist_picker_selected, config->username,
                  strlen(config->username)))
        return TRUE;
    } else
      return TRUE;
  }
  return FALSE;
}

/** @brief Pop-up menu for picker */
static struct menuitem playlist_picker_menuitems[] = {
  {
    "Select playlist",
    NULL,
    playlist_picker_select_activate,
    playlist_picker_select_sensitive,
    0,
    0
  },
  {
    "Play playlist",
    GTK_STOCK_MEDIA_PLAY, 
    playlist_picker_play_activate,
    playlist_picker_play_sensitive,
    0,
    0
  },
  {
    "Remove playlist",
    GTK_STOCK_DELETE,
    playlist_picker_remove_activate,
    playlist_picker_remove_sensitive,
    0,
    0
  },
};

static gboolean playlist_picker_button(GtkWidget *widget,
                                       GdkEventButton *event,
                                       gpointer attribute((unused)) user_data) {
  if(event->type == GDK_BUTTON_PRESS && event->button == 3) {
    static GtkWidget *playlist_picker_menu;

    /* Right click press pops up a menu */
    ensure_selected(GTK_TREE_VIEW(widget), event);
    /* Find the selected row */
    GtkTreeIter iter[1];
    if(!gtk_tree_selection_get_selected(playlist_picker_selection, 0, iter))
      return TRUE;
    popup(&playlist_picker_menu, event,
          playlist_picker_menuitems,
          sizeof playlist_picker_menuitems / sizeof *playlist_picker_menuitems,
          iter);
    return TRUE;
  }
  return FALSE;
}

static void playlist_picker_destroy(void) {
  playlist_picker_delete_button = NULL;
  g_object_unref(playlist_picker_list);
  playlist_picker_list = NULL;
  playlist_picker_selection = NULL;
  playlist_picker_selected = NULL;
}

/* Playlist editor ---------------------------------------------------------- */

static GtkWidget *playlist_editor_shared;
static GtkWidget *playlist_editor_public;
static GtkWidget *playlist_editor_private;
static int playlist_editor_setting_buttons;

/** @brief Buttons for the playlist window */
static struct button playlist_editor_buttons[] = {
  {
    GTK_STOCK_OK,
    playlist_editor_ok,
    "Close window",
    0,
    gtk_box_pack_end,
  },
  {
    GTK_STOCK_HELP,
    playlist_editor_help,
    "Go to manual",
    0,
    gtk_box_pack_end,
  },
};

#define NPLAYLIST_EDITOR_BUTTONS (int)(sizeof playlist_editor_buttons / sizeof *playlist_editor_buttons)

static GtkWidget *playlists_editor_create(void) {
  assert(ql_playlist.view == NULL);     /* better not be set up already */

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  playlist_editor_shared = gtk_radio_button_new_with_label(NULL, "shared");
  playlist_editor_public
    = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(playlist_editor_shared),
                                                  "public");
  playlist_editor_private
    = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(playlist_editor_shared),
                                                  "private");
  g_signal_connect(playlist_editor_public, "toggled",
                   G_CALLBACK(playlist_editor_button_toggled),
                   (void *)"public");
  g_signal_connect(playlist_editor_private, "toggled",
                   G_CALLBACK(playlist_editor_button_toggled),
                   (void *)"private");
  gtk_box_pack_start(GTK_BOX(hbox), playlist_editor_shared,
                     FALSE/*expand*/, FALSE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(hbox), playlist_editor_public,
                     FALSE/*expand*/, FALSE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(hbox), playlist_editor_private,
                     FALSE/*expand*/, FALSE/*fill*/, 0);
  playlist_editor_set_buttons(0,0,0);
  create_buttons_box(playlist_editor_buttons,
                     NPLAYLIST_EDITOR_BUTTONS,
                     hbox);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *view = init_queuelike(&ql_playlist);
  gtk_box_pack_start(GTK_BOX(vbox), view,
                     TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox,
                     FALSE/*expand*/, FALSE/*fill*/, 0);
  g_signal_connect(view, "key-press-event",
                   G_CALLBACK(playlist_editor_keypress), 0);
  return vbox;
}

static gboolean playlist_editor_keypress(GtkWidget attribute((unused)) *widget,
                                         GdkEventKey *event,
                                         gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_BackSpace:
  case GDK_Delete:
    playlist_remove_activate(NULL, NULL);
    return TRUE;
  default:
    return FALSE;
  }
}

/** @brief Called when the public/private buttons are set */
static void playlist_editor_button_toggled(GtkToggleButton *tb,
                                           gpointer userdata) {
  const char *state = userdata;
  if(!gtk_toggle_button_get_active(tb)
     || !playlist_picker_selected
     || playlist_editor_setting_buttons)
    return;
  disorder_eclient_playlist_set_share(client, playlist_editor_share_set,
                                      playlist_picker_selected, state, NULL);
}

static void playlist_editor_share_set(void attribute((unused)) *v,
                                      const attribute((unused)) char *err) {
  if(err)
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
}
  
/** @brief Set the editor button state and sensitivity */
static void playlist_editor_set_buttons(const char attribute((unused)) *event,
                                        void *eventdata,
                                        void attribute((unused)) *callbackdata) {
  /* If this event is for a non-selected playlist do nothing */
  if(eventdata
     && playlist_picker_selected
     && strcmp(eventdata, playlist_picker_selected))
    return;
  if(playlist_picker_selected) {
    if(strchr(playlist_picker_selected, '.'))
      disorder_eclient_playlist_get_share(client,
                                          playlist_editor_got_share,
                                          playlist_picker_selected,
                                          (void *)playlist_picker_selected);
    else
      playlist_editor_got_share((void *)playlist_picker_selected, NULL,
                                "shared");
  } else
    playlist_editor_got_share(NULL, NULL, NULL);
}

/** @brief Called with playlist sharing details */
static void playlist_editor_got_share(void *v,
                                      const char *err,
                                      const char *value) {
  const char *playlist = v;
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    value = NULL;
  }
  /* Set the currently active button */
  ++playlist_editor_setting_buttons;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_editor_shared),
                               value && !strcmp(value, "shared"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_editor_public),
                               value && !strcmp(value, "public"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_editor_private),
                               value && !strcmp(value, "private"));
  /* Set button sensitivity */
  gtk_widget_set_sensitive(playlist_editor_shared, FALSE);
  int sensitive = (playlist
                   && strchr(playlist, '.')
                   && !strncmp(playlist, config->username,
                               strlen(config->username)));
  gtk_widget_set_sensitive(playlist_editor_public, sensitive);
  gtk_widget_set_sensitive(playlist_editor_private, sensitive);
  --playlist_editor_setting_buttons;
}

/** @brief (Re-)populate the playlist tree model */
static void playlist_editor_fill(const char attribute((unused)) *event,
                                 void *eventdata,
                                 void attribute((unused)) *callbackdata) {
  const char *modified_playlist = eventdata;
  if(!playlist_window)
    return;
  if(!playlist_picker_selected)
    return;
  if(!strcmp(playlist_picker_selected, modified_playlist))
    disorder_eclient_playlist_get(client, playlists_editor_received_tracks,
                                  playlist_picker_selected,
                                  (void *)playlist_picker_selected);
}

/** @brief Called with new tracks for the playlist */
static void playlists_editor_received_tracks(void *v,
                                             const char *err,
                                             int nvec, char **vec) {
  const char *playlist = v;
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    return;
  }
  if(!playlist_picker_selected
     || strcmp(playlist, playlist_picker_selected)) {
    /* The tracks are for the wrong playlist - something must have changed
     * while the fetch command was in flight.  We just ignore this callback,
     * the right answer will be requested and arrive in due course. */
    return;
  }
  if(nvec == -1)
    /* No such playlist, presumably we'll get a deleted event shortly */
    return;
  /* Translate the list of tracks into queue entries */
  struct queue_entry *newq, **qq = &newq, *qprev = NULL;
  hash *h = hash_new(sizeof(int));
  for(int n = 0; n < nvec; ++n) {
    struct queue_entry *q = xmalloc(sizeof *q);
    q->prev = qprev;
    q->track = vec[n];
    /* Synthesize a unique ID so that the selection survives updates.  Tracks
     * can appear more than once in the queue so we can't use raw track names,
     * so we add a serial number to the start. */
    int *serialp = hash_find(h, vec[n]), serial = serialp ? *serialp : 0;
    byte_xasprintf((char **)&q->id, "%d-%s", serial++, vec[n]);
    hash_add(h, vec[n], &serial, HASH_INSERT_OR_REPLACE);
    *qq = q;
    qq = &q->next;
    qprev = q;
  }
  *qq = NULL;
  ql_new_queue(&ql_playlist, newq);
}

static void playlist_editor_ok(GtkButton attribute((unused)) *button, 
                               gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(playlist_window);
}

static void playlist_editor_help(GtkButton attribute((unused)) *button, 
                                 gpointer attribute((unused)) userdata) {
  popup_help("playlists.html");
}

/* Playlist mutation -------------------------------------------------------- */

/** @brief State structure for guarded playlist modification
 *
 * To safely move, insert or delete rows we must:
 * - take a lock
 * - fetch the playlist
 * - verify it's not changed
 * - update the playlist contents
 * - store the playlist
 * - release the lock
 *
 * The playlist_modify_ functions do just that.
 *
 * To kick things off create one of these and disorder_eclient_playlist_lock()
 * with playlist_modify_locked() as its callback.  @c modify will be called; it
 * should disorder_eclient_playlist_set() to set the new state with
 * playlist_modify_updated() as its callback.
 */
struct playlist_modify_data {
  /** @brief Affected playlist */
  const char *playlist;
  /** @brief Modification function
   * @param mod Pointer back to state structure
   * @param ntracks Length of playlist
   * @param tracks Tracks in playlist
   */
  void (*modify)(struct playlist_modify_data *mod,
                int ntracks, char **tracks);

  /** @brief Number of tracks dropped */
  int ntracks;
  /** @brief Track names dropped */
  char **tracks;
  /** @brief Track IDs dropped */
  char **ids;
  /** @brief Drop after this point */
  struct queue_entry *after_me;
};

/** @brief Called with playlist locked
 *
 * This is the entry point for guarded modification ising @ref
 * playlist_modify_data.
 */
static void playlist_modify_locked(void *v, const char *err) {
  struct playlist_modify_data *mod = v;
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    return;
  }
  disorder_eclient_playlist_get(client, playlist_modify_retrieved,
                                mod->playlist, mod);
}

/** @brief Called with current playlist contents
 * Checks that the playlist is still current and has not changed.
 */
void playlist_modify_retrieved(void *v, const char *err,
                               int nvec,
                               char **vec) {
  struct playlist_modify_data *mod = v;
  if(err) {
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
    disorder_eclient_playlist_unlock(client, playlist_modify_unlocked, NULL);
    return;
  }
  if(nvec < 0
     || !playlist_picker_selected
     || strcmp(mod->playlist, playlist_picker_selected)) {
    disorder_eclient_playlist_unlock(client, playlist_modify_unlocked, NULL);
    return;
  }
  /* We check that the contents haven't changed.  If they have we just abandon
   * the operation.  The user will have to try again. */
  struct queue_entry *q;
  int n;
  for(n = 0, q = ql_playlist.q; q && n < nvec; ++n, q = q->next)
    if(strcmp(q->track, vec[n]))
      break;
  if(n != nvec || q != NULL)  {
    disorder_eclient_playlist_unlock(client, playlist_modify_unlocked, NULL);
    return;
  }
  mod->modify(mod, nvec, vec);
}

/** @brief Called when the playlist has been updated */
static void playlist_modify_updated(void attribute((unused)) *v,
                                    const char *err) {
  if(err) 
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
  disorder_eclient_playlist_unlock(client, playlist_modify_unlocked, NULL);
}

/** @brief Called when the playlist has been unlocked */
static void playlist_modify_unlocked(void attribute((unused)) *v,
                                     const char *err) {
  if(err) 
    popup_submsg(playlist_window, GTK_MESSAGE_ERROR, err);
}

/* Drop tracks into a playlist ---------------------------------------------- */

static void playlist_drop(struct queuelike attribute((unused)) *ql,
                          int ntracks,
                          char **tracks, char **ids,
                          struct queue_entry *after_me) {
  struct playlist_modify_data *mod = xmalloc(sizeof *mod);

  mod->playlist = playlist_picker_selected;
  mod->modify = playlist_drop_modify;
  mod->ntracks = ntracks;
  mod->tracks = tracks;
  mod->ids = ids;
  mod->after_me = after_me;
  disorder_eclient_playlist_lock(client, playlist_modify_locked,
                                 mod->playlist, mod);
}

/** @brief Return true if track @p i is in the moved set */
static int playlist_drop_is_moved(struct playlist_modify_data *mod,
                                  int i) {
  struct queue_entry *q;

  /* Find the q corresponding to i, so we can get the ID */
  for(q = ql_playlist.q; i; q = q->next, --i)
    ;
  /* See if track i matches any of the moved set by ID */
  for(int n = 0; n < mod->ntracks; ++n)
    if(!strcmp(q->id, mod->ids[n]))
      return 1;
  return 0;
}

static void playlist_drop_modify(struct playlist_modify_data *mod,
                                 int nvec, char **vec) {
  char **newvec;
  int nnewvec;

  //fprintf(stderr, "\nplaylist_drop_modify\n");
  /* after_me is the queue_entry to insert after, or NULL to insert at the
   * beginning (including the case when the playlist is empty) */
  //fprintf(stderr, "after_me = %s\n",
  //        mod->after_me ? mod->after_me->track : "NULL");
  struct queue_entry *q = ql_playlist.q;
  int ins = 0;
  if(mod->after_me) {
    ++ins;
    while(q && q != mod->after_me) {
      q = q->next;
      ++ins;
    }
  }
  /* Now ins is the index to insert at; equivalently, the row to insert before,
   * and so equal to nvec to append. */
#if 0
  fprintf(stderr, "ins = %d = %s\n",
          ins, ins < nvec ? vec[ins] : "NULL");
  for(int n = 0; n < nvec; ++n)
    fprintf(stderr, "%d: %s %s\n", n, n == ins ? "->" : "  ", vec[n]);
  fprintf(stderr, "nvec = %d\n", nvec);
#endif
  if(mod->ids) {
    /* This is a rearrangement */
    /* We have:
     * - vec[], the current layout
     * - ins, pointing into vec
     * - mod->tracks[], a subset of vec[] which is to be moved
     *
     * ins is the insertion point BUT it is in terms of the whole
     * array, i.e. before mod->tracks[] have been removed.  The first
     * step then is to remove everything in mod->tracks[] and adjust
     * ins downwards as necessary.
     */
    /* First zero out anything that's moved */
    int before_ins = 0;
    for(int n = 0; n < nvec; ++n) {
      if(playlist_drop_is_moved(mod, n)) {
        vec[n] = NULL;
        if(n < ins)
          ++before_ins;
      }
    }
    /* Now collapse down the array */
    int i = 0;
    for(int n = 0; n < nvec; ++n) {
      if(vec[n])
        vec[i++] = vec[n];
    }
    assert(i + mod->ntracks == nvec);
    nvec = i;
    /* Adjust the insertion point to take account of things moved from before
     * it */
    ins -= before_ins;
    /* The effect is now the same as an insertion */
  }
  /* This is (now) an insertion */
  nnewvec = nvec + mod->ntracks;
  newvec = xcalloc(nnewvec, sizeof (char *));
  memcpy(newvec, vec,
         ins * sizeof (char *));
  memcpy(newvec + ins, mod->tracks,
         mod->ntracks * sizeof (char *));
  memcpy(newvec + ins + mod->ntracks, vec + ins,
         (nvec - ins) * sizeof (char *));
  disorder_eclient_playlist_set(client, playlist_modify_updated, mod->playlist,
                                newvec, nnewvec, mod);
}

/* Playlist editor right-click menu ---------------------------------------- */

/** @brief Called to determine whether the playlist is playable */
static int playlist_playall_sensitive(void attribute((unused)) *extra) {
  /* If there's no playlist obviously we can't play it */
  if(!playlist_picker_selected)
    return FALSE;
  /* If it's empty we can't play it */
  if(!ql_playlist.q)
    return FALSE;
  /* Otherwise we can */
  return TRUE;
}

/** @brief Called to play the selected playlist */
static void playlist_playall_activate(GtkMenuItem attribute((unused)) *menuitem,
                                      gpointer attribute((unused)) user_data) {
  if(!playlist_picker_selected)
    return;
  /* Re-use the menu-based activation callback */
  disorder_eclient_playlist_get(client, playlist_menu_received_content,
                                playlist_picker_selected, NULL);
}

/** @brief Called to determine whether the playlist is playable */
static int playlist_remove_sensitive(void attribute((unused)) *extra) {
  /* If there's no playlist obviously we can't remove from it */
  if(!playlist_picker_selected)
    return FALSE;
  /* If no tracks are selected we cannot remove them */
  if(!gtk_tree_selection_count_selected_rows(ql_playlist.selection))
    return FALSE;
  /* We're good to go */
  return TRUE;
}

/** @brief Called to remove the selected playlist */
static void playlist_remove_activate(GtkMenuItem attribute((unused)) *menuitem,
                                     gpointer attribute((unused)) user_data) {
  if(!playlist_picker_selected)
    return;
  struct playlist_modify_data *mod = xmalloc(sizeof *mod);

  mod->playlist = playlist_picker_selected;
  mod->modify = playlist_remove_modify;
  disorder_eclient_playlist_lock(client, playlist_modify_locked,
                                 mod->playlist, mod);
}

static void playlist_remove_modify(struct playlist_modify_data *mod,
                                   int attribute((unused)) nvec, char **vec) {
  GtkTreeIter iter[1];
  gboolean it = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ql_playlist.store),
                                              iter);
  int n = 0, m = 0;
  while(it) {
    if(!gtk_tree_selection_iter_is_selected(ql_playlist.selection, iter))
      vec[m++] = vec[n++];
    else
      n++;
    it = gtk_tree_model_iter_next(GTK_TREE_MODEL(ql_playlist.store), iter);
  }
  disorder_eclient_playlist_set(client, playlist_modify_updated, mod->playlist,
                                vec, m, mod);
}

/* Playlists window --------------------------------------------------------- */

/** @brief Pop up the playlists window
 *
 * Called when the playlists menu item is selected
 */
void playlist_window_create(gpointer attribute((unused)) callback_data,
                            guint attribute((unused)) callback_action,
                            GtkWidget attribute((unused)) *menu_item) {
  /* If the window already exists, raise it */
  if(playlist_window) {
    gtk_window_present(GTK_WINDOW(playlist_window));
    return;
  }
  /* Create the window */
  playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(playlist_window, tool_style);
  g_signal_connect(playlist_window, "destroy",
		   G_CALLBACK(playlist_window_destroyed), &playlist_window);
  gtk_window_set_title(GTK_WINDOW(playlist_window), "Playlists Management");
  /* TODO loads of this is very similar to (copied from!) users.c - can we
   * de-dupe? */
  /* Keyboard shortcuts */
  g_signal_connect(playlist_window, "key-press-event",
                   G_CALLBACK(playlist_window_keypress), 0);
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(playlist_window), 640, 320);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), playlist_picker_create(),
                     FALSE/*expand*/, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_event_box_new(),
                     FALSE/*expand*/, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), playlists_editor_create(),
                     TRUE/*expand*/, TRUE/*fill*/, 0);

  gtk_container_add(GTK_CONTAINER(playlist_window), frame_widget(hbox, NULL));
  gtk_widget_show_all(playlist_window);
}

/** @brief Keypress handler */
static gboolean playlist_window_keypress(GtkWidget attribute((unused)) *widget,
                                         GdkEventKey *event,
                                         gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_Escape:
    gtk_widget_destroy(playlist_window);
    return TRUE;
  default:
    return FALSE;
  }
}

/** @brief Called when the playlist window is destroyed */
static void playlist_window_destroyed(GtkWidget attribute((unused)) *widget,
                                      GtkWidget **widget_pointer) {
  destroy_queuelike(&ql_playlist);
  playlist_picker_destroy();
  *widget_pointer = NULL;
}

/** @brief Initialize playlist support */
void playlists_init(void) {
  /* We re-get all playlists upon any change... */
  event_register("playlist-created", playlist_list_update, 0);
  event_register("playlist-deleted", playlist_list_update, 0);
  /* ...and on reconnection */
  event_register("log-connected", playlist_list_update, 0);
  /* ...and from time to time */
  event_register("periodic-slow", playlist_list_update, 0);
  /* ...and at startup */
  playlist_list_update(0, 0, 0);

  /* Update the playlists menu when the set of playlists changes */
  event_register("playlists-updated", playlist_menu_changed, 0);
  /* Update the new-playlist OK button when the set of playlists changes */
  event_register("playlists-updated", playlist_new_changed, 0);
  /* Update the list of playlists in the edit window when the set changes */
  event_register("playlists-updated", playlist_picker_fill, 0);
  /* Update the displayed playlist when it is modified */
  event_register("playlist-modified", playlist_editor_fill, 0);
  /* Update the shared/public/etc buttons when a playlist is modified */
  event_register("playlist-modified", playlist_editor_set_buttons, 0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
