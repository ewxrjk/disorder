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
/** @file disobedience/popup.c
 * @brief Disobedience popup menus
 */
#include "disobedience.h"
#include "popup.h"

void popup(GtkWidget **menup,
           GdkEventButton *event,
           struct menuitem *items,
           int nitems,
           void *extra) {
  GtkWidget *menu = *menup;

  /* Create the menu if it does not yet exist */
  if(!menu) {
    menu = *menup = gtk_menu_new();
    g_signal_connect(menu, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), menup);
    for(int n = 0; n < nitems; ++n) {
      items[n].w = gtk_menu_item_new_with_label(items[n].name);
      gtk_menu_attach(GTK_MENU(menu), items[n].w, 0, 1, n, n + 1);
    }
    set_tool_colors(menu);
  }
  /* Configure item sensitivity */
  for(int n = 0; n < nitems; ++n) {
    if(items[n].handlerid)
      g_signal_handler_disconnect(items[n].w,
                                  items[n].handlerid);
    gtk_widget_set_sensitive(items[n].w,
                             items[n].sensitive(extra));
    items[n].handlerid = g_signal_connect(items[n].w,
                                          "activate",
                                          G_CALLBACK(items[n].activate),
                                          extra);
  }
  /* Pop up the menu */
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), 0, 0, 0, 0,
                 event->button, event->time);
}

/** @brief Make sure the right thing is selected
 * @param treeview Tree view
 * @param event Mouse event
 */
void ensure_selected(GtkTreeView *treeview,
                     GdkEventButton *event) {
  GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
  /* Get the path of the hovered item */
  GtkTreePath *path;
  if(!gtk_tree_view_get_path_at_pos(treeview,
                                    event->x, event->y,
                                    &path,
                                    NULL,
                                    NULL, NULL))
    return;                     /* If there isn't one, do nothing */
  if(!gtk_tree_selection_path_is_selected(selection, path)) {
    /* We're hovered over one thing but it's not the selected row.  This is
     * very confusing for the poor old user so we select the hovered row. */
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);
  }
  gtk_tree_path_free(path);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
