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
#include "popup.h"
#include "choose.h"

VECTOR_TYPE(cdvector, struct choosedata *, xrealloc);

/** @brief Popup menu */
static GtkWidget *choose_menu;

/** @brief Callback for choose_get_selected() */
static void choose_gather_selected_callback(GtkTreeModel attribute((unused)) *model,
                                            GtkTreePath attribute((unused)) *path,
                                            GtkTreeIter *iter,
                                            gpointer data) {
  struct cdvector *v = data;
  struct choosedata *cd = choose_iter_to_data(iter);

  if(cd)
    cdvector_append(v, cd);
}

/** @brief Get a list of all selected tracks and directories */
static struct choosedata **choose_get_selected(int *nselected) {
  struct cdvector v[1];

  cdvector_init(v);
  gtk_tree_selection_selected_foreach(choose_selection,
                                      choose_gather_selected_callback,
                                      v);
  cdvector_terminate(v);
  if(nselected)
    *nselected = v->nvec;
  return v->vec;
}

static int choose_selectall_sensitive(void attribute((unused)) *extra) {
  return TRUE;
}
  
static void choose_selectall_activate(GtkMenuItem attribute((unused)) *item,
                                      gpointer attribute((unused)) userdata) {
  gtk_tree_selection_select_all(choose_selection);
}
  
static int choose_selectnone_sensitive(void attribute((unused)) *extra) {
  return gtk_tree_selection_count_selected_rows(choose_selection) > 0;
}
  
static void choose_selectnone_activate(GtkMenuItem attribute((unused)) *item,
                                       gpointer attribute((unused)) userdata) {
  gtk_tree_selection_unselect_all(choose_selection);
}
  
static int choose_play_sensitive(void attribute((unused)) *extra) {
  struct choosedata *cd, **cdp = choose_get_selected(NULL);
  int counts[2] = { 0, 0 };
  while((cd = *cdp++))
    ++counts[cd->type];
  return !counts[CHOOSE_DIRECTORY] && counts[CHOOSE_FILE];
}

static void choose_play_completed(void attribute((unused)) *v,
                                  const char *error) {
  if(error)
    popup_protocol_error(0, error);
}
  
static void choose_play_activate(GtkMenuItem attribute((unused)) *item,
                                 gpointer attribute((unused)) userdata) {
  struct choosedata *cd, **cdp = choose_get_selected(NULL);
  while((cd = *cdp++)) {
    if(cd->type == CHOOSE_FILE)
      disorder_eclient_play(client, xstrdup(cd->track),
                            choose_play_completed, 0);
  }
}
  
static int choose_properties_sensitive(void *extra) {
  return choose_play_sensitive(extra);
}
  
static void choose_properties_activate(GtkMenuItem attribute((unused)) *item,
                                       gpointer attribute((unused)) userdata) {
  struct choosedata *cd, **cdp = choose_get_selected(NULL);
  struct vector v[1];
  vector_init(v);
  while((cd = *cdp++))
    vector_append(v, xstrdup(cd->track));
  properties(v->nvec, (const char **)v->vec);
}

/** @brief Pop-up menu for choose */
static struct menuitem choose_menuitems[] = {
  {
    "Play track",
    choose_play_activate,
    choose_play_sensitive,
    0,
    0
  },
  {
    "Track properties",
    choose_properties_activate,
    choose_properties_sensitive,
    0,
    0
  },
  {
    "Select all tracks",
    choose_selectall_activate,
    choose_selectall_sensitive,
    0,
    0
  },
  {
    "Deselect all tracks",
    choose_selectnone_activate,
    choose_selectnone_sensitive,
    0,
    0
  },
};

const struct tabtype choose_tabtype = {
  choose_properties_sensitive,
  choose_selectall_sensitive,
  choose_selectnone_sensitive,
  choose_properties_activate,
  choose_selectall_activate,
  choose_selectnone_activate,
  0,
  0
};

/** @brief Called when a mouse button is pressed or released */
gboolean choose_button_event(GtkWidget attribute((unused)) *widget,
                             GdkEventButton *event,
                             gpointer attribute((unused)) user_data) {
  if(event->type == GDK_BUTTON_RELEASE && event->button == 2) {
    /* Middle click release - play track */
    ensure_selected(GTK_TREE_VIEW(choose_view), event);
    choose_play_activate(NULL, NULL);
  } else if(event->type == GDK_BUTTON_PRESS && event->button == 3) {
    /* Right click press - pop up the menu */
    ensure_selected(GTK_TREE_VIEW(choose_view), event);
    popup(&choose_menu, event,
          choose_menuitems, sizeof choose_menuitems / sizeof *choose_menuitems,
          0);
    return TRUE;
  }
  return FALSE;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
