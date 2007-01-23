/*
 * This file is part of DisOrder.
 * Copyright (C) 2006 Richard Kettlewell
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

#ifndef DISOBEDIENCE_H
#define DISOBEDIENCE_H

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Types ------------------------------------------------------------------- */

struct queuelike;
struct choosenode;

struct callbackdata {
  void (*onerror)(struct callbackdata *cbd,
                  int code,
		  const char *msg);     /* called on error */
  union {
    const char *key;                    /* gtkqueue.c op_part_lookup */
    struct choosenode *choosenode;      /* gtkchoose.c got_files/got_dirs */
    struct queuelike *ql;               /* gtkqueue.c queuelike_completed */
    struct prefdata *f;                 /* properties.c */
  } u;
};

struct tabtype {
  int (*properties_sensitive)(GtkWidget *tab);
  int (*selectall_sensitive)(GtkWidget *tab);
  void (*properties_activate)(GtkWidget *tab);
  void (*selectall_activate)(GtkWidget *tab);
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

/* Functions --------------------------------------------------------------- */

disorder_eclient *gtkclient(void);
/* Configure C for use in GTK+ programs */

void popup_protocol_error(int code,
                          const char *msg);
/* Report an error */

void properties(int ntracks, char **tracks);
/* Pop up a properties window for a list of tracks */

GtkWidget *scroll_widget(GtkWidget *child, const char *name);
/* Wrap a widget up for scrolling */

GdkPixbuf *find_image(const char *name);
/* Get the pixbuf for an image.  Returns a null pointer if it cannot be
 * found. */

void popup_error(const char *msg);
/* Pop up an error message */


/* Main menu */

GtkWidget *menubar(GtkWidget *w);
/* Create the menu bar */
     
void menu_update(int page);
/* Called whenever the main menu might need to change.  PAGE is the current
 * page if known or -1 otherwise. */


/* Controls */

GtkWidget *control_widget(void);
/* Make the controls widget */

void control_update(void);
/* Called whenever we think the control widget needs changing */


/* Queue/Recent */

GtkWidget *queue_widget(void);
GtkWidget *recent_widget(void);
/* Create widgets for displaying the queue and the recently played list */

void queue_update(void);
void recent_update(void);
/* Called whenever we think the queue or recent list might have chanegd */

void queue_select_all(struct queuelike *ql);
/* Select all on some queue */

void queue_properties(struct queuelike *ql);
/* Pop up properties of selected items in some queue */

void playing_update(void);
/* Called whenever we think the currently playing track might have changed */

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

#endif /* DISOBEDIENCE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
/* arch-tag:5DbN8e67AvkhPmNqSQyZFQ */
