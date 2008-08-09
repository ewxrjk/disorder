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
 */
#include "disobedience.h"

static void playlists_updated(void *v,
                              const char *err,
                              int nvec, char **vec);

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
}

 void edit_playlists(gpointer attribute((unused)) callback_data,
                     guint attribute((unused)) callback_action,
                     GtkWidget attribute((unused)) *menu_item) {
  fprintf(stderr, "edit playlists\n");  /* TODO */
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
