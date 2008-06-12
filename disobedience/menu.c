/*
 * This file is part of DisOrder.
 * Copyright (C) 2006-2008 Richard Kettlewell
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
/** @file disobedience/menu.c
 * @brief Main menu
 */

#include "disobedience.h"

static GtkWidget *selectall_widget;
static GtkWidget *selectnone_widget;
static GtkWidget *properties_widget;

/** @brief Main menu widgets */
GtkItemFactory *mainmenufactory;

static void about_popup_got_version(void *v,
                                    const char *error,
                                    const char *value);

/** @brief Called when the quit option is activated
 *
 * Just exits.
 */
static void quit_program(gpointer attribute((unused)) callback_data,
                         guint attribute((unused)) callback_action,
                         GtkWidget attribute((unused)) *menu_item) {
  D(("quit_program"));
  exit(0);
}

/* TODO can we have a single parameterized callback for all these */

/** @brief Called when the select all option is activated
 *
 * Calls the per-tab select all function.
 */
static void select_all(gpointer attribute((unused)) callback_data,
                       guint attribute((unused)) callback_action,
                       GtkWidget attribute((unused)) *menu_item) {
  GtkWidget *tab = gtk_notebook_get_nth_page
    (GTK_NOTEBOOK(tabs), gtk_notebook_current_page(GTK_NOTEBOOK(tabs)));
  const struct tabtype *t = g_object_get_data(G_OBJECT(tab), "type");

  if(t->selectall_activate)
    t->selectall_activate(NULL, t->extra);
}

/** @brief Called when the select none option is activated
 *
 * Calls the per-tab select none function.
 */
static void select_none(gpointer attribute((unused)) callback_data,
                        guint attribute((unused)) callback_action,
                        GtkWidget attribute((unused)) *menu_item) {
  GtkWidget *tab = gtk_notebook_get_nth_page
    (GTK_NOTEBOOK(tabs), gtk_notebook_current_page(GTK_NOTEBOOK(tabs)));
  const struct tabtype *t = g_object_get_data(G_OBJECT(tab), "type");

  if(t->selectnone_activate)
    t->selectnone_activate(NULL, t->extra);
}

/** @brief Called when the track properties option is activated
 *
 * Calls the per-tab properties function.
 */
static void properties_item(gpointer attribute((unused)) callback_data,
                            guint attribute((unused)) callback_action,
                            GtkWidget attribute((unused)) *menu_item) {
  GtkWidget *tab = gtk_notebook_get_nth_page
    (GTK_NOTEBOOK(tabs), gtk_notebook_current_page(GTK_NOTEBOOK(tabs)));
  const struct tabtype *t = g_object_get_data(G_OBJECT(tab), "type");

  if(t->properties_activate)
    t->properties_activate(NULL, t->extra);
}

/** @brief Called when the login option is activated */
static void login(gpointer attribute((unused)) callback_data,
                  guint attribute((unused)) callback_action,
                  GtkWidget attribute((unused)) *menu_item) {
  login_box();
}

/** @brief Called when the login option is activated */
static void users(gpointer attribute((unused)) callback_data,
                  guint attribute((unused)) callback_action,
                  GtkWidget attribute((unused)) *menu_item) {
  manage_users();
}

#if 0
/** @brief Called when the settings option is activated */
static void settings(gpointer attribute((unused)) callback_data,
                     guint attribute((unused)) callback_action,
                     GtkWidget attribute((unused)) *menu_item) {
  popup_settings();
}
#endif

/** @brief Update menu state
 *
 * Determines option sensitivity according to the current tab and adjusts the
 * widgets accordingly.  Knows about @ref DISORDER_CONNECTED so the callbacks
 * need not.
 *
 * TODO: base this on menu popup instead?
 */
void menu_update(int page) {
  if(tabs) {
    GtkWidget *tab = gtk_notebook_get_nth_page
      (GTK_NOTEBOOK(tabs),
       page < 0 ? gtk_notebook_current_page(GTK_NOTEBOOK(tabs)) : page);
    const struct tabtype *t = g_object_get_data(G_OBJECT(tab), "type");

    assert(t != 0);
    gtk_widget_set_sensitive(properties_widget,
                             (t->properties_sensitive
                              && t->properties_sensitive(t->extra)
                              && (disorder_eclient_state(client) & DISORDER_CONNECTED)));
    gtk_widget_set_sensitive(selectall_widget,
                             t->selectall_sensitive
                             && t->selectall_sensitive(t->extra));
    gtk_widget_set_sensitive(selectnone_widget,
                             t->selectnone_sensitive
                             && t->selectnone_sensitive(t->extra));
  }
}
   
/** @brief Fetch version in order to display the about... popup */
static void about_popup(gpointer attribute((unused)) callback_data,
                        guint attribute((unused)) callback_action,
                        GtkWidget attribute((unused)) *menu_item) {
  D(("about_popup"));

  gtk_label_set_text(GTK_LABEL(report_label), "getting server version");
  disorder_eclient_version(client,
                           about_popup_got_version,
                           0);
}

static void manual_popup(gpointer attribute((unused)) callback_data,
                       guint attribute((unused)) callback_action,
                       GtkWidget attribute((unused)) *menu_item) {
  D(("manual_popup"));

  popup_help();
}

/** @brief Called when version arrives, displays about... popup */
static void about_popup_got_version(void attribute((unused)) *v,
                                    const char attribute((unused)) *error,
                                    const char *value) {
  GtkWidget *w;
  char *server_version_string;
  char *short_version_string;
  GtkWidget *hbox, *vbox, *title;

  if(!value)
    value = "[error]";
  byte_xasprintf(&server_version_string, "Server version %s", value);
  byte_xasprintf(&short_version_string, "Disobedience %s",
                 disorder_short_version_string);
  w = gtk_dialog_new_with_buttons("About Disobedience",
                                  GTK_WINDOW(toplevel),
                                  (GTK_DIALOG_MODAL
                                   |GTK_DIALOG_DESTROY_WITH_PARENT),
                                  GTK_STOCK_OK,
                                  GTK_RESPONSE_ACCEPT,
                                  (char *)NULL);
  hbox = gtk_hbox_new(FALSE/*homogeneous*/, 1/*padding*/);
  vbox = gtk_vbox_new(FALSE/*homogeneous*/, 1/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox),
                     gtk_image_new_from_pixbuf(find_image("duck.png")),
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     4/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_label_new(short_version_string),
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     1/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_label_new(server_version_string),
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     1/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_label_new("\xC2\xA9 2004-2008 Richard Kettlewell"),
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     1/*padding*/);
  gtk_box_pack_end(GTK_BOX(hbox),
                   vbox,
                   FALSE/*expand*/,
                   FALSE/*fill*/,
                   0/*padding*/);
  title = gtk_label_new(0);
  gtk_label_set_markup(GTK_LABEL(title),
                       "<span font_desc=\"Sans 36\">Disobedience</span>");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(w)->vbox), title,
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     0/*padding*/);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(w)->vbox), hbox,
                     FALSE/*expand*/,
                     FALSE/*fill*/,
                     0/*padding*/);
  set_tool_colors(w);
  gtk_widget_show_all(w);
  gtk_dialog_run(GTK_DIALOG(w));
  gtk_widget_destroy(w);
}

/** @brief Set 'Manage Users' menu item sensitivity */
void users_set_sensitive(int sensitive) {
  GtkWidget *w = gtk_item_factory_get_widget(mainmenufactory,
                                             "<GdisorderMain>/Server/Manage users");
  gtk_widget_set_sensitive(w, sensitive);
}

/** @brief Called when our rights change */
static void menu_rights_changed(const char attribute((unused)) *event,
                                void attribute((unused)) *eventdata,
                                void attribute((unused)) *callbackdata) {
  users_set_sensitive(!!(last_rights & RIGHT_ADMIN));
}

/** @brief Create the menu bar widget */
GtkWidget *menubar(GtkWidget *w) {
  GtkWidget *m;

  static const GtkItemFactoryEntry entries[] = {
    {
      (char *)"/Server",                /* path */
      0,                                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<Branch>",               /* item_type */
      0                                 /* extra_data */
    },
    { 
      (char *)"/Server/Login",          /* path */
      (char *)"<CTRL>L",                /* accelerator */
      login,                            /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    { 
      (char *)"/Server/Manage users",   /* path */
      0,                                /* accelerator */
      users,                            /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
#if 0
    {
      (char *)"/Server/Settings",       /* path */
      0,                                /* accelerator */
      settings,                         /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
#endif
    {
      (char *)"/Server/Quit Disobedience", /* path */
      (char *)"<CTRL>Q",                /* accelerator */
      quit_program,                     /* callback */
      0,                                /* callback_action */
      (char *)"<StockItem>",            /* item_type */
      GTK_STOCK_QUIT                    /* extra_data */
    },
    
    {
      (char *)"/Edit",                  /* path */
      0,                                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<Branch>",               /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Edit/Select all tracks", /* path */
      (char *)"<CTRL>A",                /* accelerator */
      select_all,                       /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Edit/Deselect all tracks", /* path */
      (char *)"<CTRL><SHIFT>A",         /* accelerator */
      select_none,                      /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Edit/Track properties", /* path */
      0,                                /* accelerator */
      properties_item,                  /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    
    {
      (char *)"/Control",               /* path */
      0,                                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<Branch>",               /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Control/Scratch",       /* path */
      (char *)"<CTRL>S",                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Control/Playing",       /* path */
      (char *)"<CTRL>P",                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<CheckItem>",            /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Control/Random play",   /* path */
      (char *)"<CTRL>R",                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<CheckItem>",            /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Control/Network player", /* path */
      (char *)"<CTRL>N",                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<CheckItem>",            /* item_type */
      0                                 /* extra_data */
    },
    
    {
      (char *)"/Help",                  /* path */
      0,                                /* accelerator */
      0,                                /* callback */
      0,                                /* callback_action */
      (char *)"<Branch>",               /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Help/Manual page",      /* path */
      0,                                /* accelerator */
      manual_popup,                     /* callback */
      0,                                /* callback_action */
      0,                                /* item_type */
      0                                 /* extra_data */
    },
    {
      (char *)"/Help/About DisOrder",   /* path */
      0,                                /* accelerator */
      about_popup,                      /* callback */
      0,                                /* callback_action */
      (char *)"<StockItem>",            /* item_type */
      GTK_STOCK_ABOUT                   /* extra_data */
    },
  };

  GtkAccelGroup *accel = gtk_accel_group_new();

  D(("add_menubar"));
  /* TODO: item factories are deprecated in favour of some XML thing */
  mainmenufactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<GdisorderMain>",
                                         accel);
  gtk_item_factory_create_items(mainmenufactory,
                                sizeof entries / sizeof *entries,
                                (GtkItemFactoryEntry *)entries,
                                0);
  gtk_window_add_accel_group(GTK_WINDOW(w), accel);
  selectall_widget = gtk_item_factory_get_widget(mainmenufactory,
						 "<GdisorderMain>/Edit/Select all tracks");
  selectnone_widget = gtk_item_factory_get_widget(mainmenufactory,
						 "<GdisorderMain>/Edit/Deselect all tracks");
  properties_widget = gtk_item_factory_get_widget(mainmenufactory,
						  "<GdisorderMain>/Edit/Track properties");
  assert(selectall_widget != 0);
  assert(selectnone_widget != 0);
  assert(properties_widget != 0);
  event_register("rights-changed", menu_rights_changed, 0);
  users_set_sensitive(0);
  m = gtk_item_factory_get_widget(mainmenufactory,
                                  "<GdisorderMain>");
  set_tool_colors(m);
  return m;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
