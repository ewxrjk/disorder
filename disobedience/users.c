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

static GtkWidget *users_details_window;
static GtkWidget *users_details_name;
static GtkWidget *users_details_email;
static GtkWidget *users_details_password;
static GtkWidget *users_details_password2;
//static GtkWidget *users_details_rights;

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

static char *users_getuser(void) {
  GtkTreeIter iter;
  char *who, *c;

  if(gtk_tree_selection_get_selected(users_selection, 0, &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(users_list), &iter,
                       0, &who, -1);
    if(who) {
      c = xstrdup(who);
      g_free(who);
      return c;
    }
  }
  return 0;
}

/** @brief Text should be visible */
#define DETAIL_VISIBLE 1

/** @brief Text should be editable */
#define DETAIL_EDITABLE 2

static GtkWidget *users_detail_generic(GtkWidget *table,
                                       int *rowp,
                                       const char *title,
                                       GtkWidget *selector) {
  const int row = (*rowp)++;
  GtkWidget *const label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), 1, 0);
  gtk_table_attach(GTK_TABLE(table),
                   label,
                   0, 1,                /* left/right_attach */
                   row, row+1,          /* top/bottom_attach */
                   GTK_FILL,            /* xoptions */
                   0,                   /* yoptions */
                   1, 1);               /* x/ypadding */
  gtk_table_attach(GTK_TABLE(table),
                   selector,
                   1, 2,                /* left/right_attach */
                   row, row + 1,        /* top/bottom_attach */
                   GTK_EXPAND|GTK_FILL, /* xoptions */
                   GTK_FILL,            /* yoptions */
                   1, 1);               /* x/ypadding */
  return selector;
}

/** @brief Add a row to the user details table
 * @param table Containin table widget
 * @param rowp Pointer to row number, incremented
 * @param title Label for this row
 * @param value Initial value or NULL
 * @param flags Flags word
 * @return The text entry widget
 */
static GtkWidget *users_add_detail(GtkWidget *table,
                                   int *rowp,
                                   const char *title,
                                   const char *value,
                                   unsigned flags) {
  GtkWidget *entry = gtk_entry_new();

  gtk_entry_set_visibility(GTK_ENTRY(entry),
                           !!(flags & DETAIL_VISIBLE));
  gtk_editable_set_editable(GTK_EDITABLE(entry),
                            !!(flags & DETAIL_EDITABLE));
  if(value)
    gtk_entry_set_text(GTK_ENTRY(entry), value);
  return users_detail_generic(table, rowp, title, entry);
}

static GtkWidget *users_add_right(GtkWidget *table,
                                  int *rowp,
                                  const char *title,
                                  rights_type value) {
  GtkWidget *check = gtk_check_button_new();
  
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), !!value);
  return users_detail_generic(table, rowp, title, check);
}
                                                   

/** @brief Create the user details window
 * @param title Window title
 * @param name User name (users_edit()) or NULL (users_add())
 * @param email Email address
 * @param rights User rights string
 * @param password Password
 *
 * This is used both by users_add() and users_edit().
 *
 * The window contains:
 * - display or edit fields for the username
 * - edit fields for the email address
 * - two hidden edit fields for the password (they must match)
 * - check boxes for the rights
 */
static void users_makedetails(const char *title,
                              const char *name,
                              const char *email,
                              const char *rights,
                              const char *password) {
  GtkWidget *table;
  int row = 0;
  rights_type r = 0;
  
  /* Create the window */
  users_details_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(users_details_window, tool_style);
  g_signal_connect(users_details_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &users_details_window);
  gtk_window_set_title(GTK_WINDOW(users_details_window), title);
  gtk_window_set_transient_for(GTK_WINDOW(users_details_window),
                               GTK_WINDOW(users_window));
  table = gtk_table_new(4, 2, FALSE/*!homogeneous*/);

  users_details_name = users_add_detail(table, &row, "Username", name,
                                        (name ? 0 : DETAIL_EDITABLE)
                                        |DETAIL_VISIBLE);

  users_details_email = users_add_detail(table, &row, "Email", email,
                                         DETAIL_EDITABLE|DETAIL_VISIBLE);

  users_details_password = users_add_detail(table, &row, "Password",
                                            password,
                                            DETAIL_EDITABLE);
  users_details_password2 = users_add_detail(table, &row, "Password",
                                             password,
                                             DETAIL_EDITABLE);

  parse_rights(rights, &r, 1);
  users_add_right(table, &row, "Read operations", r & RIGHT_READ);
  users_add_right(table, &row, "Play track", r & RIGHT_PLAY);
  /* TODO move */
  /* TODO remove */
  /* TODO scratch */
  users_add_right(table, &row, "Set volume", r & RIGHT_VOLUME);
  users_add_right(table, &row, "Admin operations", r & RIGHT_ADMIN);
  users_add_right(table, &row, "Rescan", r & RIGHT_RESCAN);
  users_add_right(table, &row, "Register new users", r & RIGHT_REGISTER);
  users_add_right(table, &row, "Modify own userinfo", r & RIGHT_USERINFO);
  users_add_right(table, &row, "Modify track preferences", r & RIGHT_PREFS);
  users_add_right(table, &row, "Modify global preferences", r & RIGHT_GLOBAL_PREFS);
  users_add_right(table, &row, "Pause/resume tracks", r & RIGHT_PAUSE);

  gtk_container_add(GTK_CONTAINER(users_details_window), table);
  gtk_widget_show_all(users_details_window);
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
  GtkWidget *yesno;
  char *who;
  int res;
  struct callbackdata *cbd;

  if(!(who = users_getuser()))
    return;
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
    cbd->u.user = who;
    disorder_eclient_deluser(client, users_deleted, cbd->u.user, cbd);
  }
}

static void users_edit(GtkButton attribute((unused)) *button,
		       gpointer attribute((unused)) userdata) {
  char *who;
  
  if(!(who = users_getuser()))
    return;
  users_makedetails("editing user details", who, "foo@bar", "play", "wobble");
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
