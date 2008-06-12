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
#ifndef CHOOSE_H
#define CHOOSE_H

/** @brief Extra data at each node */
struct choosedata {
  /** @brief Node type */
  int type;

  /** @brief Full track or directory name */
  gchar *track;

  /** @brief Sort key */
  gchar *sort;
};

/** @brief Column numbers */
enum {
  STATE_COLUMN,
  NAME_COLUMN,
  LENGTH_COLUMN,
  CHOOSEDATA_COLUMN
};

/** @brief @ref choosedata node is a file */
#define CHOOSE_FILE 0

/** @brief @ref choosedata node is a directory */
#define CHOOSE_DIRECTORY 1

extern GtkTreeStore *choose_store;
extern GtkWidget *choose_view;
extern GtkTreeSelection *choose_selection;
extern const struct tabtype choose_tabtype;

struct choosedata *choose_iter_to_data(GtkTreeIter *iter);
struct choosedata *choose_path_to_data(GtkTreePath *path);
gboolean choose_button_event(GtkWidget *widget,
                             GdkEventButton *event,
                             gpointer user_data);

#endif /* CHOOSE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
