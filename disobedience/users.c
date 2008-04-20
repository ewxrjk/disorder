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
 *
 * The user management window contains:
 * - a list of all the users
 * - an add button
 * - a delete button
 * - a user details panel
 * - an apply button
 *
 * When you select a user that user's details are displayed to the right of the
 * list.  Hit the Apply button and any changes are applied.
 *
 * When you select 'add' a new empty set of details are displayed to be edited.
 * Again Apply will commit them.
 *
 * TODO: @ref RIGHT_ADMIN and @ref RIGHT_USERINFO should be applied here, so we
 * can give decent error messages.
 *
 * TODO: it would be really nice if the Username entry could be removed and new
 * user names entered in the list, rather off in the details panel.  This may
 * be possible with a sufficiently clever GtkCellRenderer.
 */

#include "disobedience.h"
#include "bits.h"

static GtkWidget *users_window;
static GtkListStore *users_list;
static GtkTreeSelection *users_selection;

static GtkWidget *users_details_table;
static GtkWidget *users_apply_button;
static GtkWidget *users_delete_button;
static GtkWidget *users_details_name;
static GtkWidget *users_details_email;
static GtkWidget *users_details_password;
static GtkWidget *users_details_password2;
static GtkWidget *users_details_rights[32];
static int users_details_row;
static const char *users_selected;
static const char *users_deferred_select;

static int users_mode;
#define MODE_NONE 0
#define MODE_ADD 1
#define MODE_EDIT 2

#define mode(X) do {                                    \
  users_mode = MODE_##X;                                \
  if(0) fprintf(stderr, "%s:%d: %s(): mode -> %s\n",    \
          __FILE__, __LINE__, __FUNCTION__, #X);        \
  users_details_sensitize_all();                        \
} while(0)

static const char *users_email, *users_rights, *users_password;

/** @brief qsort() callback for username comparison */
static int usercmp(const void *a, const void *b) {
  return strcmp(*(char **)a, *(char **)b);
}

/** @brief Find a user
 * @param user User to find
 * @param iter Iterator to point at user
 * @return 0 on success, -1 if not found
 */
static int users_find_user(const char *user,
                           GtkTreeIter *iter) {
  char *who;

  /* Find the user */
  if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(users_list), iter))
    return -1;
  do {
    gtk_tree_model_get(GTK_TREE_MODEL(users_list), iter,
		       0, &who, -1);
    if(!strcmp(who, user)) {
      g_free(who);
      return 0;
    }
    g_free(who);
  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(users_list), iter));
  return -1;
}

/** @brief Called with the list of users
 *
 * Called:
 * - at startup to populate the initial list
 * - when we add a user
 * - maybe in the future when we delete a user
 *
 * If users_deferred_select is set then that user is selected.
 */
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
  if(users_deferred_select) {
    if(!users_find_user(users_deferred_select, &iter))
      gtk_tree_selection_select_iter(users_selection, &iter);
    users_deferred_select = 0;
  }
}

/** @brief Text should be visible */
#define DETAIL_VISIBLE 1

/** @brief Text should be editable */
#define DETAIL_EDITABLE 2

/** @brief Add a row to the user detail table */
static void users_detail_generic(const char *title,
                                 GtkWidget *selector) {
  const int row = users_details_row++;
  GtkWidget *const label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), 1, 0);
  gtk_table_attach(GTK_TABLE(users_details_table),
                   label,
                   0, 1,                /* left/right_attach */
                   row, row+1,          /* top/bottom_attach */
                   GTK_FILL,            /* xoptions */
                   0,                   /* yoptions */
                   1, 1);               /* x/ypadding */
  gtk_table_attach(GTK_TABLE(users_details_table),
                   selector,
                   1, 2,                /* left/right_attach */
                   row, row + 1,        /* top/bottom_attach */
                   GTK_EXPAND|GTK_FILL, /* xoptions */
                   GTK_FILL,            /* yoptions */
                   1, 1);               /* x/ypadding */
}

/** @brief Add a row to the user details table
 * @param entryp Where to put GtkEntry
 * @param title Label for this row
 * @param value Initial value or NULL
 * @param flags Flags word
 */
static void users_add_detail(GtkWidget **entryp,
                             const char *title,
                             const char *value,
                             unsigned flags) {
  GtkWidget *entry;

  if(!(entry = *entryp)) {
    *entryp = entry = gtk_entry_new();
    users_detail_generic(title, entry);
  }
  gtk_entry_set_visibility(GTK_ENTRY(entry),
                           !!(flags & DETAIL_VISIBLE));
  gtk_editable_set_editable(GTK_EDITABLE(entry),
                            !!(flags & DETAIL_EDITABLE));
  gtk_entry_set_text(GTK_ENTRY(entry), value ? value : "");
}

/** @brief Add a checkbox for a right
 * @param title Label for this row
 * @param value Current value
 * @param right Right bit
 */
static void users_add_right(const char *title,
                            rights_type value,
                            rights_type right) {
  GtkWidget *check;
  GtkWidget **checkp = &users_details_rights[leftmost_bit(right)];

  if(!(check = *checkp)) {
    *checkp = check = gtk_check_button_new_with_label("");
    users_detail_generic(title, check);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), !!(value & right));
}

/** @brief Set sensitivity of particular mine/random rights bits */
static void users_details_sensitize(rights_type r) {
  const int bit = leftmost_bit(r);
  const GtkWidget *all = users_details_rights[bit];
  const int sensitive = (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(all))
                         && users_mode != MODE_NONE);

  gtk_widget_set_sensitive(users_details_rights[bit + 1], sensitive);
  gtk_widget_set_sensitive(users_details_rights[bit + 2], sensitive);
}

/** @brief Set sensitivity of everything in sight */
static void users_details_sensitize_all(void) {
  int n;

  for(n = 0; n < 32; ++n)
    if(users_details_rights[n])
      gtk_widget_set_sensitive(users_details_rights[n], users_mode != MODE_NONE);
  gtk_widget_set_sensitive(users_details_name, users_mode != MODE_NONE);
  gtk_widget_set_sensitive(users_details_email, users_mode != MODE_NONE);
  gtk_widget_set_sensitive(users_details_password, users_mode != MODE_NONE);
  gtk_widget_set_sensitive(users_details_password2, users_mode != MODE_NONE);
  users_details_sensitize(RIGHT_MOVE_ANY);
  users_details_sensitize(RIGHT_REMOVE_ANY);
  users_details_sensitize(RIGHT_SCRATCH_ANY);
  gtk_widget_set_sensitive(users_apply_button, users_mode != MODE_NONE);
  gtk_widget_set_sensitive(users_delete_button, !!users_selected);
}

/** @brief Called when an _ALL widget is toggled
 *
 * Modifies sensitivity of the corresponding _MINE and _RANDOM widgets.  We
 * just do the lot rather than trying to figure out which one changed,
 */
static void users_any_toggled(GtkToggleButton attribute((unused)) *togglebutton,
                              gpointer attribute((unused)) user_data) {
  users_details_sensitize_all();
}

/** @brief Add a checkbox for a three-right group
 * @param title Label for this row
 * @param bits Rights bits (not masked or normalized)
 * @param mask Mask for this group (must be 7*2^n)
 */
static void users_add_right_group(const char *title,
                                  rights_type bits,
                                  rights_type mask) {
  const uint32_t first = mask / 7;
  const int bit = leftmost_bit(first);
  GtkWidget **widgets = &users_details_rights[bit], *any, *mine, *random;

  if(!*widgets) {
    GtkWidget *hbox = gtk_hbox_new(FALSE, 2);

    any = widgets[0] = gtk_check_button_new_with_label("Any");
    mine = widgets[1] = gtk_check_button_new_with_label("Own");
    random = widgets[2] = gtk_check_button_new_with_label("Random");
    gtk_box_pack_start(GTK_BOX(hbox), any, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), mine, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), random, FALSE, FALSE, 0);
    users_detail_generic(title, hbox);
    g_signal_connect(any, "toggled", G_CALLBACK(users_any_toggled), NULL);
    users_details_rights[bit] = any;
    users_details_rights[bit + 1] = mine;
    users_details_rights[bit + 2] = random;
  } else {
    any = widgets[0];
    mine = widgets[1];
    random = widgets[2];
  }
  /* Discard irrelevant bits */
  bits &= mask;
  /* Shift down to bits 0-2; the mask is always 3 contiguous bits */
  bits >>= bit;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(any), !!(bits & 1));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mine), !!(bits & 2));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(random), !!(bits & 4));
}

/** @brief Called when the details table is destroyed */
static void users_details_destroyed(GtkWidget attribute((unused)) *widget,
                                    GtkWidget attribute((unused)) **wp) {
  users_details_table = 0;
  g_object_unref(users_list);
  users_list = 0;
  users_details_name = 0;
  users_details_email = 0;
  users_details_password = 0;
  users_details_password2 = 0;
  memset(users_details_rights, 0, sizeof users_details_rights);
  /* also users_selection?  Not AFAICT; _get_selection does upref */
}

/** @brief Create or modify the user details table
 * @param name User name (users_edit()) or NULL (users_add())
 * @param email Email address
 * @param rights User rights string
 * @param password Password
 */
static void users_makedetails(const char *name,
                              const char *email,
                              const char *rights,
                              const char *password,
                              unsigned nameflags,
                              unsigned flags) {
  rights_type r = 0;
  
  /* Create the table if it doesn't already exist */
  if(!users_details_table) {
    users_details_table = gtk_table_new(4, 2, FALSE/*!homogeneous*/);
    g_signal_connect(users_details_table, "destroy",
                     G_CALLBACK(users_details_destroyed), 0);
  }

  /* Create or update the widgets */
  users_add_detail(&users_details_name, "Username", name,
                   (DETAIL_EDITABLE|DETAIL_VISIBLE) & nameflags);

  users_add_detail(&users_details_email, "Email", email,
                   (DETAIL_EDITABLE|DETAIL_VISIBLE) & flags);

  users_add_detail(&users_details_password, "Password", password,
                   DETAIL_EDITABLE & flags);
  users_add_detail(&users_details_password2, "Password", password,
                   DETAIL_EDITABLE & flags);

  parse_rights(rights, &r, 0);
  users_add_right("Read operations", r, RIGHT_READ);
  users_add_right("Play track", r, RIGHT_PLAY);
  users_add_right_group("Move", r, RIGHT_MOVE__MASK);
  users_add_right_group("Remove", r, RIGHT_REMOVE__MASK);
  users_add_right_group("Scratch", r, RIGHT_SCRATCH__MASK);
  users_add_right("Set volume", r, RIGHT_VOLUME);
  users_add_right("Admin operations", r, RIGHT_ADMIN);
  users_add_right("Rescan", r, RIGHT_RESCAN);
  users_add_right("Register new users", r, RIGHT_REGISTER);
  users_add_right("Modify own userinfo", r, RIGHT_USERINFO);
  users_add_right("Modify track preferences", r, RIGHT_PREFS);
  users_add_right("Modify global preferences", r, RIGHT_GLOBAL_PREFS);
  users_add_right("Pause/resume tracks", r, RIGHT_PAUSE);
  users_details_sensitize_all();
}

/** @brief Called when the 'add' button is pressed */
static void users_add(GtkButton attribute((unused)) *button,
		      gpointer attribute((unused)) userdata) {
  /* Unselect whatever is selected */
  gtk_tree_selection_unselect_all(users_selection);
  /* Reset the form */
  /* TODO it would be better to use the server default_rights if there's no
   * client setting. */
  users_makedetails("",
                    "",
                    config->default_rights,
                    "",
                    DETAIL_EDITABLE|DETAIL_VISIBLE,
                    DETAIL_EDITABLE|DETAIL_VISIBLE);
  /* Remember we're adding a user */
  mode(ADD);
}

static rights_type users_get_rights(void) {
  rights_type r = 0;
  int n;

  /* Extract the rights value */
  for(n = 0; n < 32; ++n) {
    if(users_details_rights[n])
      if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(users_details_rights[n])))
         r |= 1 << n;
  }
  /* Throw out redundant bits */
  if(r & RIGHT_REMOVE_ANY)
    r &= ~(rights_type)(RIGHT_REMOVE_MINE|RIGHT_REMOVE_RANDOM);
  if(r & RIGHT_MOVE_ANY)
    r &= ~(rights_type)(RIGHT_MOVE_MINE|RIGHT_MOVE_RANDOM);
  if(r & RIGHT_SCRATCH_ANY)
    r &= ~(rights_type)(RIGHT_SCRATCH_MINE|RIGHT_SCRATCH_RANDOM);
  return r;
}

/** @brief Called when various things fail */
static void users_op_failed(struct callbackdata attribute((unused)) *cbd,
                            int attribute((unused)) code,
                            const char *msg) {
  popup_submsg(users_window, GTK_MESSAGE_ERROR, msg);
}

/** @brief Called when a new user has been created */
static void users_adduser_completed(void *v) {
  struct callbackdata *cbd = v;

  /* Now the user is created we can go ahead and set the email address */
  if(*cbd->u.edituser.email) {
    struct callbackdata *ncbd = xmalloc(sizeof *cbd);
    ncbd->onerror = users_op_failed;
    disorder_eclient_edituser(client, NULL, cbd->u.edituser.user,
                              "email", cbd->u.edituser.email, ncbd);
  }
  /* Refresh the list of users */
  disorder_eclient_users(client, users_got_list, 0);
  /* We'll select the newly created user */
  users_deferred_select = cbd->u.edituser.user;
}

/** @brief Called if creating a new user fails */
static void users_adduser_failed(struct callbackdata attribute((unused)) *cbd,
                                 int attribute((unused)) code,
                                 const char *msg) {
  popup_submsg(users_window, GTK_MESSAGE_ERROR, msg);
  mode(ADD);                            /* Let the user try again */
}

/** @brief Called when the 'Apply' button is pressed */
static void users_apply(GtkButton attribute((unused)) *button,
                        gpointer attribute((unused)) userdata) {
  struct callbackdata *cbd;
  const char *password;
  const char *password2;
  const char *name;
  const char *email;

  switch(users_mode) {
  case MODE_NONE:
    return;
  case MODE_ADD:
    name = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_name)));
    email = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_email)));
    password = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_password)));
    password2 = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_password2)));
    if(!*name) {
      /* No username.  Really we wanted to desensitize the Apply button when
       * there's no userame but there doesn't seem to be a signal to detect
       * changes to the entry text.  Consequently we have error messages
       * instead.  */
      popup_submsg(users_window, GTK_MESSAGE_ERROR, "Must enter a username");
      return;
    }
    if(strcmp(password, password2)) {
      popup_submsg(users_window, GTK_MESSAGE_ERROR, "Passwords do not match");
      return;
    }
    if(*email && !strchr(email, '@')) {
      /* The server will complain about this but we can give a better error
       * message this way */
      popup_submsg(users_window, GTK_MESSAGE_ERROR, "Invalid email address");
      return;
    }
    cbd = xmalloc(sizeof *cbd);
    cbd->onerror = users_adduser_failed;
    cbd->u.edituser.user = name;
    cbd->u.edituser.email = email;
    disorder_eclient_adduser(client, users_adduser_completed,
                             name,
                             password,
                             rights_string(users_get_rights()),
                             cbd);
    /* We switch to no-op mode while creating the user */
    mode(NONE);
    break;
  case MODE_EDIT:
    /* Ugh, can we de-dupe with above? */
    email = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_email)));
    password = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_password)));
    password2 = xstrdup(gtk_entry_get_text(GTK_ENTRY(users_details_password2)));
    if(strcmp(password, password2)) {
      popup_submsg(users_window, GTK_MESSAGE_ERROR, "Passwords do not match");
      return;
    }
    if(*email && !strchr(email, '@')) {
      popup_submsg(users_window, GTK_MESSAGE_ERROR, "Invalid email address");
      return;
    }
    cbd = xmalloc(sizeof *cbd);
    cbd->onerror = users_op_failed;
    disorder_eclient_edituser(client, NULL, users_selected,
                              "email", email, cbd);
    disorder_eclient_edituser(client, NULL, users_selected,
                              "password", password, cbd);
    disorder_eclient_edituser(client, NULL, users_selected,
                              "rights", rights_string(users_get_rights()), cbd);
    /* We remain in edit mode */
    break;
  }
}

/** @brief Called when a user has been deleted */
static void users_deleted(void *v) {
  const struct callbackdata *const cbd = v;
  GtkTreeIter iter[1];

  if(!users_find_user(cbd->u.user, iter))    /* Find the user... */
    gtk_list_store_remove(users_list, iter); /* ...and remove them */
}

/** @brief Called when the 'Delete' button is pressed */
static void users_delete(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata) {
  GtkWidget *yesno;
  int res;
  struct callbackdata *cbd;

  if(!users_selected)
    return;
  yesno = gtk_message_dialog_new(GTK_WINDOW(users_window),
                                 GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION,
                                 GTK_BUTTONS_YES_NO,
                                 "Do you really want to delete user %s?"
                                 " This action cannot be undone.",
                                 users_selected);
  res = gtk_dialog_run(GTK_DIALOG(yesno));
  gtk_widget_destroy(yesno);
  if(res == GTK_RESPONSE_YES) {
    cbd = xmalloc(sizeof *cbd);
    cbd->onerror = users_op_failed;
    cbd->u.user = users_selected;
    disorder_eclient_deluser(client, users_deleted, cbd->u.user, cbd);
  }
}

static void users_got_email(void attribute((unused)) *v, const char *value) {
  users_email = value;
}

static void users_got_rights(void attribute((unused)) *v, const char *value) {
  users_rights = value;
}

static void users_got_password(void attribute((unused)) *v, const char *value) {
  users_password = value;
  users_makedetails(users_selected,
                    users_email,
                    users_rights,
                    users_password,
                    DETAIL_VISIBLE,
                    DETAIL_EDITABLE|DETAIL_VISIBLE);
  mode(EDIT);
}

/** @brief Called when the selection MIGHT have changed */
static void users_selection_changed(GtkTreeSelection attribute((unused)) *treeselection,
                                    gpointer attribute((unused)) user_data) {
  GtkTreeIter iter;
  char *gselected, *selected;
  
  /* Identify the current selection */
  if(gtk_tree_selection_get_selected(users_selection, 0, &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(users_list), &iter,
                       0, &gselected, -1);
    selected = xstrdup(gselected);
    g_free(gselected);
  } else
    selected = 0;
  /* Eliminate no-change cases */
  if(!selected && !users_selected)
    return;
  if(selected && users_selected && !strcmp(selected, users_selected))
    return;
  /* There's been a change; junk the old data and fetch new data in
   * background. */
  users_selected = selected;
  users_makedetails("", "", "", "",
                    DETAIL_VISIBLE,
                    DETAIL_VISIBLE);
  if(users_selected) {
    disorder_eclient_userinfo(client, users_got_email, users_selected,
                              "email", 0);
    disorder_eclient_userinfo(client, users_got_rights, users_selected,
                              "rights", 0);
    disorder_eclient_userinfo(client, users_got_password, users_selected,
                              "password", 0);
  }
  mode(NONE);                           /* not editing *yet* */
}

/** @brief Table of buttons below the user list */
static struct button users_buttons[] = {
  {
    GTK_STOCK_ADD,
    users_add,
    "Create a new user",
    0
  },
  {
    GTK_STOCK_REMOVE,
    users_delete,
    "Delete a user",
    0
  },
};
#define NUSERS_BUTTONS (sizeof users_buttons / sizeof *users_buttons)

/** @brief Pop up the user management window */
void manage_users(void) {
  GtkWidget *tree, *buttons, *hbox, *hbox2, *vbox, *vbox2;
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
  /* Get the selection for the view; set its mode; arrange for a callback when
   * it changes */
  users_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
  gtk_tree_selection_set_mode(users_selection, GTK_SELECTION_BROWSE);
  g_signal_connect(users_selection, "changed",
                   G_CALLBACK(users_selection_changed), NULL);

  /* Create the control buttons */
  buttons = create_buttons_box(users_buttons,
			       NUSERS_BUTTONS,
			       gtk_hbox_new(FALSE, 1));
  users_delete_button = users_buttons[1].widget;

  /* Buttons live below the list */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scroll_widget(tree), TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE/*expand*/, FALSE, 0);

  /* Create an empty user details table, and put an apply button below it */
  users_apply_button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
  users_makedetails("", "", "", "",
                    DETAIL_VISIBLE,
                    DETAIL_VISIBLE);
  g_signal_connect(users_apply_button, "clicked",
                   G_CALLBACK(users_apply), NULL);
  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox2), users_apply_button,
                   FALSE/*expand*/, FALSE, 0);
  
  vbox2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), users_details_table,
                     TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox2,
                     FALSE/*expand*/, FALSE, 0);
  
  /* User details are to the right of the list.  We put in a pointless event
   * box as as spacer, so that the longest label in the user details isn't
   * cuddled up to the user list. */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE/*expand*/, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_event_box_new(), FALSE/*expand*/, FALSE, 2);
  gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE/*expand*/, TRUE/*fill*/, 0);
  gtk_container_add(GTK_CONTAINER(users_window), frame_widget(hbox, NULL));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
