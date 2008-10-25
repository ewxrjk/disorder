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
/** @file disobedience/popup.h
 * @brief Disobedience popup menus
 */
#ifndef POPUP_H
#define POPUP_H

/** @brief A popup menu item */
struct menuitem {
  /** @brief Menu item name */
  const char *name;

  /** @brief Called to activate the menu item */
  void (*activate)(GtkMenuItem *menuitem,
                   gpointer user_data);
  
  /** @brief Called to determine whether the menu item is usable.
   *
   * Returns @c TRUE if it should be sensitive and @c FALSE otherwise.
   */
  int (*sensitive)(void *extra);

  /** @brief Signal handler ID */
  gulong handlerid;

  /** @brief Widget for menu item */
  GtkWidget *w;
};

void popup(GtkWidget **menup,
           GdkEventButton *event,
           struct menuitem *items,
           int nitems,
           void *extra);
void ensure_selected(GtkTreeView *treeview,
                     GdkEventButton *event);

#endif /* POPUP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
