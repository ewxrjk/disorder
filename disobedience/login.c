/*
 * This file is part of DisOrder
 * Copyright (C) 2007, 2008 Richard Kettlewell
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
/** @file disobedience/login.c
 * @brief Login box for Disobedience
 *
 * As of 2.1 we have only two buttons: Login and Cancel.
 *
 * If you hit Login then a login is attempted.  If it works the window
 * disappears and the settings are saved, otherwise they are NOT saved and the
 * window remains.
 *
 * It you hit Cancel then the window disappears without saving anything.
 *
 * TODO
 * - cancel/close should be consistent with properties
 */

#include "disobedience.h"
#include "split.h"
#include "filepart.h"
#include "client.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/** @brief One field in the login window */
struct login_window_item {
  /** @brief Description label */
  const char *description;

  /** @brief Return the current value */
  const char *(*get)(void);

  /** @brief Set a new value */
  void (*set)(struct config *c, const char *value);

  /** @brief Flags
   * 
   * - @ref LWI_HIDDEN - this is a password
   * - @ref LWI_REMOTE - this is for remote connections
   */
  unsigned flags;

};

/** @brief This is a password */
#define LWI_HIDDEN 0x0001

/** @brief This is for remote connections */
#define LWI_REMOTE 0x0002

/** @brief Current login window */
GtkWidget *login_window;

/** @brief Set connection defaults */
static void default_connect(void) {
  /* If a password is set assume we're good */
  if(config->password)
    return;
  /* If we already have a host and/or port that's good too */
  if(config->connect.af != -1)
    return;
  /* If there's a suitable socket that's probably what we wanted */
  const char *s = config_get_file("socket");
  struct stat st;
  if(s && *s && stat(s, &st) == 0 && S_ISSOCK(st.st_mode))
    return;
  /* TODO can we use some mdns thing to find a DisOrder server? */
}

static const char *get_hostname(void) {
  if(config->connect.af == -1 || !config->connect.address)
    return "";
  else
    return config->connect.address;
}

static const char *get_service(void) {
  if(config->connect.af == -1)
    return "";
  else {
    char *s;

    byte_xasprintf(&s, "%d", config->connect.port);
    return s;
  }
}

static const char *get_username(void) {
  return config->username;
}

static const char *get_password(void) {
  return config->password ? config->password : "";
}

static void set_hostname(struct config *c, const char *s) {
  if(c->connect.af == -1)
    c->connect.af = AF_UNSPEC;
  c->connect.address = xstrdup(s);
}

static void set_service(struct config *c, const char *s) {
  c->connect.port = atoi(s);
}

static void set_username(struct config *c, const char *s) {
  xfree(c->username);
  c->username = xstrdup(s);
}

static void set_password(struct config *c, const char *s) {
  xfree(c->password);
  c->password = xstrdup(s);
}

/** @brief Table used to generate the form */
static const struct login_window_item lwis[] = {
  { "Hostname", get_hostname, set_hostname, LWI_REMOTE },
  { "Service", get_service, set_service, LWI_REMOTE },
  { "User name", get_username, set_username, 0 },
  { "Password", get_password, set_password, LWI_HIDDEN },
};
#define NLWIS (sizeof lwis / sizeof *lwis)

static GtkWidget *lwi_remote;
static GtkWidget *lwi_entry[NLWIS];

static void login_update_config(struct config *c) {
  size_t n;
  const gboolean remote = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lwi_remote));

  if(remote)
    c->connect.af = AF_UNSPEC;
  else
    c->connect.af = -1;
  for(n = 0; n < NLWIS; ++n)
    if(remote || !(lwis[n].flags & LWI_REMOTE))
      lwis[n].set(c, xstrdup(gtk_entry_get_text(GTK_ENTRY(lwi_entry[n]))));
}

/** @brief Save current login details */
static void login_save_config(void) {
  char *tmp;
  FILE *fp;

  byte_xasprintf(&tmp, "%s.tmp", userconfigfile);
  /* Make sure the directory exists; don't care if it already exists. */
  mkdir(d_dirname(tmp), 02700);
  /* Write out the file */
  if(!(fp = fopen(tmp, "w"))) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
               tmp, strerror(errno));
    goto done;
  }
  int rc = fprintf(fp, "username %s\n"
                   "password %s\n",
                   quoteutf8(config->username),
                   quoteutf8(config->password));
  if(rc >= 0 && config->connect.af != -1) {
    char **vec;

    netaddress_format(&config->connect, NULL, &vec);
    rc = fprintf(fp, "connect %s %s %s\n", vec[0], vec[1], vec[2]);
  }
  if(rc < 0) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error writing to %s: %s",
               tmp, strerror(errno));
    fclose(fp);
    goto done;
  }
  if(fclose(fp) < 0) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error closing %s: %s",
               tmp, strerror(errno));
    goto done;
  }
  /* Rename into place */
  if(rename(tmp, userconfigfile) < 0) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error renaming %s: %s",
               tmp, strerror(errno));
    goto done;
  }
done:
  ;
}

/** @brief User pressed OK in login window */
static void login_ok(GtkButton attribute((unused)) *button,
                     gpointer attribute((unused)) userdata) {
  disorder_client *c;
  struct config *tmpconfig = xmalloc(sizeof *tmpconfig);
  
  tmpconfig->home = xstrdup(pkgstatedir);
  /* Copy the new config into @ref config */
  login_update_config(tmpconfig);
  /* Attempt a login with the new details */
  c = disorder_new(0);
  if(!disorder_connect_generic(tmpconfig, c,
                               tmpconfig->username, tmpconfig->password,
                               NULL/*cookie*/)) {
    /* Success; save the config and start using it */
    login_update_config(config);
    login_save_config();
    logged_in();
    /* Pop down login window */
    gtk_widget_destroy(login_window);
  } else {
    /* Failed to connect - report the error */
    popup_msg(GTK_MESSAGE_ERROR, disorder_last(c));
  }
  disorder_close(c);                    /* no use for this any more */
}

/** @brief User pressed cancel in the login window */
static void login_cancel(GtkButton attribute((unused)) *button,
                         gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(login_window);
}

/** @brief User pressed cancel in the login window */
static void login_help(GtkButton attribute((unused)) *button,
                       gpointer attribute((unused)) userdata) {
  popup_help("intro.html#login");
}

/** @brief Keypress handler */
static gboolean login_keypress(GtkWidget attribute((unused)) *widget,
                               GdkEventKey *event,
                               gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_Return:
    login_ok(0, 0);
    return TRUE;
  case GDK_Escape:
    login_cancel(0, 0);
    return TRUE;
  default:
    return FALSE;
  }
}

/* Buttons that appear at the bottom of the window */
static struct button buttons[] = {
  {
    GTK_STOCK_HELP,
    login_help,
    "Go to manual",
    0,
    gtk_box_pack_start,
  },
  {
    GTK_STOCK_CLOSE,
    login_cancel,
    "Discard changes and close window",
    0,
    gtk_box_pack_end,
  },
  {
    "Login",
    login_ok,
    "(Re-)connect using these settings",
    0,
    gtk_box_pack_end,
  },
};

#define NBUTTONS (int)(sizeof buttons / sizeof *buttons)

/** @brief Called when the remote/local button is toggled (and initially)
 *
 * Sets the sensitivity of the host/port entries.
 */
static void lwi_remote_toggled(GtkToggleButton *togglebutton,
                               gpointer attribute((unused)) user_data) {
  const gboolean remote = gtk_toggle_button_get_active(togglebutton);
  
  for(unsigned n = 0; n < NLWIS; ++n)
    if(lwis[n].flags & LWI_REMOTE)
      gtk_widget_set_sensitive(lwi_entry[n], remote);
}

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
  gtk_widget_set_style(login_window, tool_style);
  g_signal_connect(login_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &login_window);
  gtk_window_set_title(GTK_WINDOW(login_window), "Login Details");
  /* Construct the form */
  table = gtk_table_new(1 + NLWIS/*rows*/, 2/*columns*/, FALSE/*homogenous*/);
  gtk_widget_set_style(table, tool_style);
  label = gtk_label_new("Remote");
  gtk_widget_set_style(label, tool_style);
  gtk_misc_set_alignment(GTK_MISC(label), 1/*right*/, 0/*bottom*/);
  gtk_table_attach(GTK_TABLE(table), label,
                   0, 1,                /* left/right_attach */
                   0, 1,                /* top/bottom_attach */
                   GTK_FILL, 0,         /* x/yoptions */
                   1, 1);               /* x/ypadding */
  lwi_remote = gtk_check_button_new();
  gtk_widget_set_style(lwi_remote, tool_style);
  gtk_table_attach(GTK_TABLE(table), lwi_remote,
                   1, 2,                   /* left/right_attach */
                   0, 1,                   /* top/bottom_attach */
                   GTK_EXPAND|GTK_FILL, 0, /* x/yoptions */
                   1, 1);                  /* x/ypadding */
  g_signal_connect(lwi_remote, "toggled", G_CALLBACK(lwi_remote_toggled), 0);
  for(n = 0; n < NLWIS; ++n) {
    label = gtk_label_new(lwis[n].description);
    gtk_widget_set_style(label, tool_style);
    gtk_misc_set_alignment(GTK_MISC(label), 1/*right*/, 0/*bottom*/);
    gtk_table_attach(GTK_TABLE(table), label,
		     0, 1,              /* left/right_attach */
		     n+1, n+2,          /* top/bottom_attach */
		     GTK_FILL, 0,       /* x/yoptions */
		     1, 1);             /* x/ypadding */
    entry = gtk_entry_new();
    gtk_widget_set_style(entry, tool_style);
    gtk_entry_set_visibility(GTK_ENTRY(entry),
                             lwis[n].flags & LWI_HIDDEN ? FALSE : TRUE);
    gtk_entry_set_text(GTK_ENTRY(entry), lwis[n].get());
    gtk_table_attach(GTK_TABLE(table), entry,
		     1, 2,              /* left/right_attach */
		     n+1, n+2,          /* top/bottom_attach */
		     GTK_EXPAND|GTK_FILL, 0, /* x/yoptions */
		     1, 1);             /* x/ypadding */
    lwi_entry[n] = entry;
  }
  /* Initial settings */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lwi_remote),
                               config->connect.af != -1);
  lwi_remote_toggled(GTK_TOGGLE_BUTTON(lwi_remote), 0);
  buttonbox = create_buttons(buttons, NBUTTONS);
  vbox = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox),
                     gtk_image_new_from_pixbuf(find_image("logo256.png")),
                     TRUE/*expand*/,
                     TRUE/*fill*/,
                     4/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox), table, 
                     TRUE/*expand*/, TRUE/*fill*/, 1/*padding*/);
  gtk_box_pack_start(GTK_BOX(vbox), buttonbox,
                     FALSE/*expand*/, FALSE/*fill*/, 1/*padding*/);
  gtk_container_add(GTK_CONTAINER(login_window), frame_widget(vbox, NULL));
  gtk_window_set_transient_for(GTK_WINDOW(login_window),
                               GTK_WINDOW(toplevel));
  /* Keyboard shortcuts */
  g_signal_connect(login_window, "key-press-event",
                   G_CALLBACK(login_keypress), 0);
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
