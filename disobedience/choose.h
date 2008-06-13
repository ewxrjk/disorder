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

/** @brief Column numbers */
enum {
  /* Visible columns */
  STATE_COLUMN,                 /* Track state */
  NAME_COLUMN,                  /* Track name (display context) */
  LENGTH_COLUMN,                /* Track length */
  /* Hidden columns */
  ISFILE_COLUMN,                /* TRUE for a track, FALSE for a directory */
  TRACK_COLUMN,                 /* Full track name, "" for placeholder */
  SORT_COLUMN,                  /* Sort key */
  BG_COLUMN,                    /* Background color */
  FG_COLUMN,                    /* Foreground color */

  CHOOSE_COLUMNS                /* column count */
};

#ifndef SEARCH_RESULT_BG
# define SEARCH_RESULT_BG "#ffffc0"
# define SEARCH_RESULT_FG "black"
#endif

extern GtkTreeStore *choose_store;
extern GtkWidget *choose_view;
extern GtkTreeSelection *choose_selection;
extern const struct tabtype choose_tabtype;

struct choosedata *choose_iter_to_data(GtkTreeIter *iter);
struct choosedata *choose_path_to_data(GtkTreePath *path);
gboolean choose_button_event(GtkWidget *widget,
                             GdkEventButton *event,
                             gpointer user_data);
void choose_play_completed(void attribute((unused)) *v,
                           const char *error);
char *choose_get_track(GtkTreeIter *iter);
char *choose_get_sort(GtkTreeIter *iter);
int choose_is_file(GtkTreeIter *iter);
int choose_is_dir(GtkTreeIter *iter);
int choose_is_placeholder(GtkTreeIter *iter);
GtkWidget *choose_search_widget(void);
int choose_is_search_result(const char *track);

#endif /* CHOOSE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
