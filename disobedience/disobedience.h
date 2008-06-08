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
/** @file disobedience/disobedience.h
 * @brief Header file for Disobedience, the DisOrder GTK+ client
 */

#ifndef DISOBEDIENCE_H
#define DISOBEDIENCE_H

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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Types ------------------------------------------------------------------- */

struct queuelike;
struct choosenode;
struct progress_window;

/** @brief Callback data structure
 *
 * This program is extremely heavily callback-driven.  Rather than have
 * numerous different callback structures we have a single one which can be
 * interpreted adequately both by success and error handlers.
 */
struct callbackdata {
  void (*onerror)(struct callbackdata *cbd,
                  int code,
		  const char *msg);     /* called on error */
  union {
    const char *key;                    /* gtkqueue.c op_part_lookup */
    struct choosenode *choosenode;      /* gtkchoose.c got_files/got_dirs */
    struct queuelike *ql;               /* gtkqueue.c queuelike_completed */
    struct prefdata *f;                 /* properties.c */
    const char *user;                   /* users.c */
    struct {
      const char *user, *email;         /* users.c */
    } edituser;
  } u;
};

/** @brief Per-tab callbacks
 *
 * Some of the options in the main menu depend on which tab is displayed, so we
 * have some callbacks to set them appropriately.
 */
struct tabtype {
  int (*properties_sensitive)(GtkWidget *tab);
  int (*selectall_sensitive)(GtkWidget *tab);
  int (*selectnone_sensitive)(GtkWidget *tab);
  void (*properties_activate)(GtkWidget *tab);
  void (*selectall_activate)(GtkWidget *tab);
  void (*selectnone_activate)(GtkWidget *tab);
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
extern int playing;                     /* true if playing some track */
extern int volume_l, volume_r;          /* current volume */
extern double goesupto;                 /* volume upper bound */
extern int choosealpha;                 /* break up choose by letter */
extern GtkTooltips *tips;
extern int rtp_supported;
extern int rtp_is_running;
extern GtkItemFactory *mainmenufactory;

extern const disorder_eclient_log_callbacks log_callbacks;

typedef void monitor_callback(void *u);

/* Functions --------------------------------------------------------------- */

disorder_eclient *gtkclient(void);
/* Configure C for use in GTK+ programs */

void popup_protocol_error(int code,
                          const char *msg);
/* Report an error */

void properties(int ntracks, const char **tracks);
/* Pop up a properties window for a list of tracks */

void properties_reset(void);

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

void register_monitor(monitor_callback *callback,
                      void *u,
                      unsigned long mask);
/* Register a state monitor */

/** @brief Type signature for a reset callback */
typedef void reset_callback(void);

void register_reset(reset_callback *callback);
/* Register a reset callback */

void reset(void);

void all_update(void);
/* Update everything */

/* Main menu */

GtkWidget *menubar(GtkWidget *w);
/* Create the menu bar */
     
void menu_update(int page);
/* Called whenever the main menu might need to change.  PAGE is the current
 * page if known or -1 otherwise. */

void users_set_sensitive(int sensitive);

/* Controls */

GtkWidget *control_widget(void);
/* Make the controls widget */

void volume_update(void);
/* Called whenever we think the volume control has changed */

void control_monitor(void *u);

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
                    const char *error);

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

/* Widget leakage debugging rubbish ---------------------------------------- */

#if MDEBUG
#define NW(what) do {                                   \
  if(++current##what % 100 > max##what) {               \
    fprintf(stderr, "%s:%d: %d %s\n",                   \
            __FILE__, __LINE__, current##what, #what);  \
    max##what = current##what;                          \
  }                                                     \
} while(0)
#define WT(what) static int current##what, max##what
#define DW(what) (--current##what)
#else
#define NW(what) do { } while(0)
#define DW(what) do { } while(0)
#define WT(what) struct neverused
#endif

#if MTRACK
extern const char *mtag;
#define MTAG(x) do { mtag = x; } while(0)
#define MTAG_PUSH(x) do { const char *save_mtag = mtag; mtag = x; (void)0
#define MTAG_POP() mtag = save_mtag; } while(0)
#else
#define MTAG(x) do { } while(0)
#define MTAG_PUSH(x) do {} while(0)
#define MTAG_POP() do {} while(0)
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
