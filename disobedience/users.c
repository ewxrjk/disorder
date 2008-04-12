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
/** @file disobedience/users.c
 * @brief User management for Disobedience
 */

#include "disobedience.h"

static GtkWidget *users_window;
static GtkListStore *users_list;
static GtkTreeSelection *users_selection;

static int usercmp(const void *a, const void *b) {
  return strcmp(*(char **)a, *(char **)b);
}

static void users_got_list(void attribute((unused)) *v, int nvec, char **vec) {
  int n;
  GtkTreeIter iter;

  /* Present users in alphabetical order */
  qsort(vec, nvec, sizeof (char *), usercmp);
  /* Set the list contents */
  gtk_list_store_clear(users_list);
  for(n = 0; n < nvec; ++n)
    gtk_list_store_insert_with_values(users_list, &iter, n/*position*/,
				      0, vec[n], /* column 0 */
				      -1);	 /* no more columns */
  /* Only show the window when the list is populated */
  gtk_widget_show_all(users_window);
}

static void users_add(GtkButton attribute((unused)) *button,
		      gpointer attribute((unused)) userdata) {
}

static void users_deleted_error(struct callbackdata attribute((unused)) *cbd,
				int attribute((unused)) code,
				const char *msg) {
  popup_msg(GTK_MESSAGE_ERROR, msg);
}

static void users_deleted(void *v) {
  const struct callbackdata *const cbd = v;
  GtkTreeIter iter;
  char *who;

  /* Find the user */
  if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(users_list), &iter))
    return;
  do {
    gtk_tree_model_get(GTK_TREE_MODEL(users_list), &iter,
		       0, &who, -1);
    if(!strcmp(who, cbd->u.user))
      break;
    g_free(who);
    who = 0;
  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(users_list), &iter));
  /* Remove them */
  gtk_list_store_remove(users_list, &iter);
  g_free(who);
}

static void users_delete(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata) {
  GtkTreeIter iter;
  GtkWidget *yesno;
  char *who;
  int res;
  struct callbackdata *cbd;

  if(gtk_tree_selection_get_selected(users_selection, 0, &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(users_list), &iter,
		       0, &who, -1);
    yesno = gtk_message_dialog_new(GTK_WINDOW(users_window),
				   GTK_DIALOG_MODAL,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_YES_NO,
				   "Do you really want to delete user %s?"
				   " This action cannot be undone.", who);
    res = gtk_dialog_run(GTK_DIALOG(yesno));
    gtk_widget_destroy(yesno);
    if(res == GTK_RESPONSE_YES) {
      cbd = xmalloc(sizeof *cbd);
      cbd->onerror = users_deleted_error;
      cbd->u.user = xstrdup(who);
      disorder_eclient_deluser(client, users_deleted, cbd->u.user, cbd);
    }
    g_free(who);
  }
}

static void users_edit(GtkButton attribute((unused)) *button,
		       gpointer attribute((unused)) userdata) {
}

static const struct button users_buttons[] = {
  {
    "Add user",
    users_add,
    "Create a new user"
  },
  {
    "Edit user",
    users_edit,
    "Edit a user"
  },
  {
    "Delete user",
    users_delete,
    "Delete a user"
  },
};
#define NUSERS_BUTTONS (sizeof users_buttons / sizeof *users_buttons)

void manage_users(void) {
  GtkWidget *tree, *buttons, *hbox;
  GtkCellRenderer *cr;
  GtkTreeViewColumn *col;
  
  /* If the window already exists just raise it */
  if(users_window) {
    gtk_window_present(GTK_WINDOW(users_window));
    return;
  }
  /* Create the window */
  users_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(users_window, tool_style);
  g_signal_connect(users_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &users_window);
  gtk_window_set_title(GTK_WINDOW(users_window), "User Management");
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(users_window), 240, 240);

  /* Create the list of users and populate it asynchronously */
  users_list = gtk_list_store_new(1, G_TYPE_STRING);
  disorder_eclient_users(client, users_got_list, 0);
  /* Create the view */
  tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(users_list));
  /* ...and the renderers for it */
  cr = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes("Username",
						 cr,
						 "text", 0,
						 NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
  /* Get the selection for the view and set its mode */
  users_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
  gtk_tree_selection_set_mode(users_selection, GTK_SELECTION_BROWSE);
  /* Create the control buttons */
  buttons = create_buttons_box(users_buttons,
			       NUSERS_BUTTONS,
			       gtk_vbox_new(FALSE, 1));
  /* Put it all together in an hbox */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), tree, TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(hbox), buttons, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(users_window), hbox);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
