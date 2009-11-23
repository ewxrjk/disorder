/*
 * This file is part of DisOrder.
 * Copyright (C) 2006-2008 Richard Kettlewell
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
/** @file disobedience/disobedience.h
 * @brief Header file for Disobedience, the DisOrder GTK+ client
 */

#ifndef DISOBEDIENCE_H
#define DISOBEDIENCE_H

#define PLAYLISTS 1

#include "common.h"

#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "mem.h"
#include "log.h"
#include "eclient.h"
#include "printf.h"
#include "cache.h"
#include "queue.h"
#include "printf.h"
#include "vector.h"
#include "trackname.h"
#include "syscalls.h"
#include "defs.h"
#include "configuration.h"
#include "hash.h"
#include "selection.h"
#include "kvp.h"
#include "eventdist.h"
#include "split.h"
#include "timeval.h"
#include "uaudio.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

/* Types ------------------------------------------------------------------- */

struct queuelike;
struct choosenode;
struct progress_window;

/** @brief Per-tab callbacks
 *
 * Some of the options in the main menu depend on which tab is displayed, so we
 * have some callbacks to set them appropriately.
 */
struct tabtype {
  int (*properties_sensitive)(void *extra);
  int (*selectall_sensitive)(void *extra);
  int (*selectnone_sensitive)(void *extra);
  void (*properties_activate)(GtkMenuItem *menuitem,
                              gpointer user_data);
  void (*selectall_activate)(GtkMenuItem *menuitem,
                             gpointer user_data);
  void (*selectnone_activate)(GtkMenuItem *menuitem,
                              gpointer user_data);
  void (*selected)(void);
  void *extra;
};

/** @brief Button definitions */
struct button {
  const gchar *stock;
  void (*clicked)(GtkButton *button, gpointer userdata);
  const char *tip;
  GtkWidget *widget;
};

/* Variables --------------------------------------------------------------- */

extern GMainLoop *mainloop;
extern GtkWidget *toplevel;             /* top level window */
extern GtkWidget *report_label;         /* label for progress indicator */
extern GtkWidget *tabs;                 /* main tabs */
extern disorder_eclient *client;        /* main client */

extern unsigned long last_state;        /* last reported state */
extern rights_type last_rights;         /* last reported rights bitmap */
extern int playing;                     /* true if playing some track */
extern int volume_l, volume_r;          /* current volume */
extern double goesupto;                 /* volume upper bound */
extern int choosealpha;                 /* break up choose by letter */
extern int rtp_supported;
extern int rtp_is_running;
extern GtkItemFactory *mainmenufactory;
extern const struct uaudio *backend;

extern const disorder_eclient_log_callbacks log_callbacks;

/* Functions --------------------------------------------------------------- */

disorder_eclient *gtkclient(void);
/* Configure C for use in GTK+ programs */

void popup_protocol_error(int code,
                          const char *msg);
/* Report an error */

void properties(int ntracks, const char **tracks);
/* Pop up a properties window for a list of tracks */

GtkWidget *scroll_widget(GtkWidget *child);
/* Wrap a widget up for scrolling */

GtkWidget *frame_widget(GtkWidget *w, const char *title);

GdkPixbuf *find_image(const char *name);
/* Get the pixbuf for an image.  Returns a null pointer if it cannot be
 * found. */

void popup_msg(GtkMessageType mt, const char *msg);
void popup_submsg(GtkWidget *parent, GtkMessageType mt, const char *msg);

void fpopup_msg(GtkMessageType mt, const char *fmt, ...);

struct progress_window *progress_window_new(const char *title);
/* Pop up a progress window */

void progress_window_progress(struct progress_window *pw,
			      int progress,
			      int limit);
/* Report current progress */

GtkWidget *iconbutton(const char *path, const char *tip);

GtkWidget *create_buttons(struct button *buttons,
                          size_t nbuttons);
GtkWidget *create_buttons_box(struct button *buttons,
                              size_t nbuttons,
                              GtkWidget *box);

void logged_in(void);

void all_update(void);
/* Update everything */

/* Main menu */

GtkWidget *menubar(GtkWidget *w);
/* Create the menu bar */

void users_set_sensitive(int sensitive);

/* Controls */

GtkWidget *control_widget(void);
/* Make the controls widget */

extern int suppress_actions;

/* Queue/Recent/Added */

GtkWidget *queue_widget(void);
GtkWidget *recent_widget(void);
GtkWidget *added_widget(void);
/* Create widgets for displaying the queue, the recently played list and the
 * newly added tracks list */

void queue_select_all(struct queuelike *ql);
void queue_select_none(struct queuelike *ql);
/* Select all/none on some queue */

void queue_properties(struct queuelike *ql);
/* Pop up properties of selected items in some queue */

int queued(const char *track);
/* Return nonzero iff TRACK is queued or playing */

extern struct queue_entry *playing_track;

/* Lookups */
const char *namepart(const char *track,
                     const char *context,
                     const char *part);
long namepart_length(const char *track);
char *namepart_resolve(const char *track);

void namepart_update(const char *track,
                     const char *context,
                     const char *part);
/* Called when a namepart might have changed */

/* Choose */

GtkWidget *choose_widget(void);
/* Create a widget for choosing tracks */

void choose_update(void);
/* Called when we think the choose tree might need updating */

void play_completed(void *v,
                    const char *err);

/* Login details */

void login_box(void);

GtkWidget *login_window;

/* User management */

void manage_users(void);

/* Help */

void popup_help(void);

/* RTP */

int rtp_running(void);
void start_rtp(void);
void stop_rtp(void);

/* Settings */

void init_styles(void);
extern GtkStyle *layout_style;
extern GtkStyle *title_style;
extern GtkStyle *even_style;
extern GtkStyle *odd_style;
extern GtkStyle *active_style;
extern GtkStyle *tool_style;
extern GtkStyle *search_style;
extern GtkStyle *drag_style;

extern const char *browser;

void save_settings(void);
void load_settings(void);
void set_tool_colors(GtkWidget *w);
void popup_settings(void);

/* Playlists */

#if PLAYLISTS
void playlists_init(void);
void playlist_window_create(gpointer callback_data,
                            guint callback_action,
                            GtkWidget  *menu_item);
extern char **playlists;
extern int nplaylists;
extern GtkWidget *menu_playlists_widget;
extern GtkWidget *playlists_menu;
extern GtkWidget *menu_editplaylists_widget;
#endif

#endif /* DISOBEDIENCE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
