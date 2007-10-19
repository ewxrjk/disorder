/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file disobedience/login.c
 * @brief Login box for Disobedience
 */

#include "disobedience.h"
#include "split.h"
#include "filepart.h"
#include <sys/types.h>
#include <sys/stat.h>

/** @brief One field in the login window */
struct login_window_item {
  /** @brief Description label */
  const char *description;

  /** @brief Return the current value */
  const char *(*get)(void);

  /** @brief Set a new value */
  void (*set)(const char *value);

  /** @brief Flags
   * 
   * - @ref LWI_HIDDEN - this is a password
   */
  unsigned flags;

};

/** @brief This is a password */
#define LWI_HIDDEN 0x0001

/** @brief Current login window */
static GtkWidget *login_window;

/** @brief Set connection defaults */
static void default_connect(void) {
  if(!config->connect.n) {
    config->connect.n = 2;
    config->connect.s = xcalloc(2, sizeof (char *));
    config->connect.s[0] = xstrdup("localhost");
    config->connect.s[1] = xstrdup("9999"); /* whatever */
  }
}

static const char *get_hostname(void) { return config->connect.s[0]; }
static const char *get_service(void) { return config->connect.s[1]; }
static const char *get_username(void) { return config->username; }
static const char *get_password(void) { return config->password; }

static void set_hostname(const char *s) { config->connect.s[0] = (char *)s; }
static void set_service(const char *s) { config->connect.s[1] = (char *)s; }
static void set_username(const char *s) { config->username = s; }
static void set_password(const char *s) { config->password = s; }

/** @brief Table used to generate the form */
static const struct login_window_item lwis[] = {
  { "Hostname", get_hostname, set_hostname, 0 },
  { "Service", get_service, set_service, 0 },
  { "User name", get_username, set_username, 0 },
  { "Password", get_password, set_password, LWI_HIDDEN },
};
#define NLWIS (sizeof lwis / sizeof *lwis)

static GtkWidget *lwi_entry[NLWIS];

static void update_config(void) {
  size_t n;

  for(n = 0; n < NLWIS; ++n)
    lwis[n].set(gtk_entry_get_text(GTK_ENTRY(lwi_entry[n])));
}

static void login_ok(GtkButton attribute((unused)) *button,
                     gpointer attribute((unused)) userdata) {
  update_config();
  reset();
}

static void login_save(GtkButton attribute((unused)) *button,
                       gpointer attribute((unused)) userdata) {
  char *path = config_userconf(0, 0), *tmp;
  FILE *fp;

  update_config();
  byte_xasprintf(&tmp, "%s.tmp", path);
  /* Make sure the directory exists; don't care if it already exists. */
  mkdir(d_dirname(tmp), 02700);
  /* Write out the file */
  if(!(fp = fopen(tmp, "w"))) {
    fpopup_error("error opening %s: %s", tmp, strerror(errno));
    return;
  }
  if(fprintf(fp, "username %s\n"
             "password %s\n"
             "connect %s %s\n",
             quoteutf8(config->username),
             quoteutf8(config->password),
             quoteutf8(config->connect.s[0]),
             quoteutf8(config->connect.s[1])) < 0) {
    fpopup_error("error writing to %s: %s", tmp, strerror(errno));
    fclose(fp);
    return;
  }
  if(fclose(fp) < 0) {
    fpopup_error("error closing %s: %s", tmp, strerror(errno));
    return;
  }
  /* Rename into place */
  if(rename(tmp, path) < 0) {
    fpopup_error("error renaming %s: %s", tmp, strerror(errno));
    return;
  }
}

static void login_cancel(GtkButton attribute((unused)) *button,
                         gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(login_window);
}

/* Buttons that appear at the bottom of the window */
static const struct button buttons[] = {
  {
    GTK_STOCK_OK,
    login_ok,
    "Login with these settings",
  },
  {
    GTK_STOCK_SAVE,
    login_save,
    "Save these settings",
  },
  {
    GTK_STOCK_CANCEL,
    login_cancel,
    "Discard all changes and close window"
  },
};

#define NBUTTONS (int)(sizeof buttons / sizeof *buttons)

/** @brief Pop up a login box */
void login_box(void) {
  GtkWidget *table, *label, *entry,  *buttonbox, *vbox;
  size_t n;

  /* If there's one already then bring it to the front */
  if(login_window) {
    gtk_window_present(GTK_WINDOW(login_window));
    return;
  }
  default_connect();
  /* Create a new login window */
  login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(login_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &login_window);
  gtk_window_set_title(GTK_WINDOW(login_window), "Login Details");
  /* Construct the form */
  table = gtk_table_new(NLWIS + 1/*rows*/, 2/*columns*/, FALSE/*homogenous*/);
  for(n = 0; n < NLWIS; ++n) {
    label = gtk_label_new(lwis[n].description);
    gtk_misc_set_alignment(GTK_MISC(label), 1/*right*/, 0/*bottom*/);
    gtk_table_attach(GTK_TABLE(table), label,
		     0, 1,              /* left/right_attach */
		     n, n+1,            /* top/bottom_attach */
		     GTK_FILL, 0,       /* x/yoptions */
		     1, 1);             /* x/ypadding */
    entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry),
                             lwis[n].flags & LWI_HIDDEN ? FALSE : TRUE);
    gtk_entry_set_text(GTK_ENTRY(entry), lwis[n].get());
    gtk_table_attach(GTK_TABLE(table), entry,
		     1, 2,              /* left/right_attach */
		     n, n+1,            /* top/bottom_attach */
		     GTK_EXPAND|GTK_FILL, 0, /* x/yoptions */
		     1, 1);             /* x/ypadding */
    lwi_entry[n] = entry;
  }
  buttonbox = create_buttons(buttons, NBUTTONS);
  vbox = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), table, 
                     TRUE/*expand*/, TRUE/*fill*/, 1/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox), buttonbox,
                     FALSE/*expand*/, FALSE/*fill*/, 1/*padding*/);
  gtk_container_add(GTK_CONTAINER(login_window), vbox);
  gtk_widget_show_all(login_window);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
