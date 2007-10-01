/*
 * This file is part of DisOrder
 * Copyright (C) 2006,  2007 Richard Kettlewell
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
/** @file disobedience/queue.c
 * @brief Queue widgets
 *
 * This file provides both the queue widget and the recently-played widget.
 *
 * A queue layout is structured as follows:
 *
 * <pre>
 *  vbox
 *   titlescroll
 *    titlelayout
 *     titlecells[col]                 eventbox (made by wrap_queue_cell)
 *      titlecells[col]->child         label (from columns[])
 *   mainscroll
 *    mainlayout
 *     cells[row * N + c]              eventbox (made by wrap_queue_cell)
 *      cells[row * N + c]->child      label (from column constructors)
 * </pre>
 *
 * titlescroll never has any scrollbars.  Instead whenever mainscroll's
 * horizontal adjustment is changed, queue_scrolled adjusts titlescroll to
 * match, forcing the title and the queue to pan in sync but allowing the queue
 * to scroll independently.
 *
 * Whenever the queue changes everything below mainlayout is thrown away and
 * reconstructed from scratch.  Name lookups are cached, so this doesn't imply
 * lots of disorder protocol traffic.
 *
 * The last cell on each row is the padding cell, and this extends ridiculously
 * far to the right.  (Can we do better?)
 *
 * When drag and drop is active we create extra eventboxes to act as dropzones.
 * These only exist while the drag proceeds, as otherwise they steal events
 * from more deserving widgets.  (It might work to hide them when not in use
 * too but this way around the d+d code is a bit more self-contained.)
 */

#include "disobedience.h"

/** @brief Horizontal padding for queue cells */
#define HCELLPADDING 4

/** @brief Vertical padding for queue cells */
#define VCELLPADDING 2

/* Queue management -------------------------------------------------------- */

WT(label);
WT(event_box);
WT(menu);
WT(menu_item);
WT(layout);
WT(vbox);

struct queuelike;

static void add_drag_targets(struct queuelike *ql);
static void remove_drag_targets(struct queuelike *ql);
static void redisplay_queue(struct queuelike *ql);
static GtkWidget *column_when(const struct queuelike *ql,
                              const struct queue_entry *q,
                              const char *data);
static GtkWidget *column_who(const struct queuelike *ql,
                             const struct queue_entry *q,
                             const char *data);
static GtkWidget *column_namepart(const struct queuelike *ql,
                                  const struct queue_entry *q,
                                  const char *data);
static GtkWidget *column_length(const struct queuelike *ql,
                                const struct queue_entry *q,
                                const char *data);
static int draggable_row(const struct queue_entry *q);

static const struct tabtype tabtype_queue; /* forward */

static const GtkTargetEntry dragtargets[] = {
  { (char *)"disobedience-queue", GTK_TARGET_SAME_APP, 0 }
};
#define NDRAGTARGETS (int)(sizeof dragtargets / sizeof *dragtargets)

/** @brief Definition of a column */
struct column {
  const char *name;                     /* Column name */
  GtkWidget *(*widget)(const struct queuelike *ql,
                       const struct queue_entry *q,
                       const char *data); /* Make a label for this column */
  const char *data;                     /* Data to pass to widget() */
  gfloat xalign;                        /* Alignment of the label */
};

/** @brief Table of columns
 *
 * Need this in the middle of the types for NCOLUMNS
 */
static const struct column columns[] = {
  { "When",   column_when,     0,        1 },
  { "Who",    column_who,      0,        0 },
  { "Artist", column_namepart, "artist", 0 },
  { "Album",  column_namepart, "album",  0 },
  { "Title",  column_namepart, "title",  0 },
  { "Length", column_length,   0,        1 }
};

/** @brief Number of columns */
#define NCOLUMNS (int)(sizeof columns / sizeof *columns)

/** @brief Data passed to menu item activation handlers */
struct menuiteminfo {
  struct queuelike *ql;                 /**< @brief which queue we're dealing with */
  struct queue_entry *q;                /**< @brief hovered entry or 0 */
};

/** @brief An item in the queue's popup menu */
struct queue_menuitem {
  /** @brief Menu item name */
  const char *name;

  /** @brief Called to activate the menu item
   *
   * The user data is the queue entry that the pointer was over when the menu
   * popped up. */
  void (*activate)(GtkMenuItem *menuitem,
                   gpointer user_data);
  
  /** @brief Called to determine whether the menu item is usable.
   *
   * Returns @c TRUE if it should be sensitive and @c FALSE otherwise.  @p q
   * points to the queue entry the pointer is over.
   */
  int (*sensitive)(struct queuelike *ql,
                   struct queue_menuitem *m,
                   struct queue_entry *q);

  /** @brief Signal handler ID */
  gulong handlerid;

  /** @brief Widget for menu item */
  GtkWidget *w;
};

/** @brief A queue-like object
 *
 * There are (currently) two of these: @ref ql_queue and @ref ql_recent.
 */
struct queuelike {
  /** @brief Name of this queue */
  const char *name;

  /** @brief Called when an update completes */
  void (*notify)(void);

  /** @brief Called to  fix up the queue after update
   * @param q The list passed back from the server
   * @return Assigned to @c ql->q
   */
  struct queue_entry *(*fixup)(struct queue_entry *q);

  /* Widgets */
  GtkWidget *mainlayout;                /**< @brief main layout */
  GtkWidget *mainscroll;                /**< @brief scroller for main layout */
  GtkWidget *titlelayout;               /**< @brief title layout */
  GtkWidget *titlecells[NCOLUMNS + 1];  /**< @brief title cells */
  GtkWidget **cells;                    /**< @brief all the cells */
  GtkWidget *menu;                      /**< @brief popup menu */
  struct queue_menuitem *menuitems;     /**< @brief menu items */
  GtkWidget *dragmark;                  /**< @brief drag destination marker */
  GtkWidget **dropzones;                /**< @brief drag targets */

  /* State */
  struct queue_entry *q;                /**< @brief head of queue */
  struct queue_entry *last_click;       /**< @brief last click */
  int nrows;                            /**< @brief number of rows */
  int mainrowheight;                    /**< @brief height of one row */
  hash *selection;                      /**< @brief currently selected items */
  int swallow_release;                  /**< @brief swallow button release from drag */
};

static struct queuelike ql_queue; /**< @brief The main queue */
static struct queuelike ql_recent; /*< @brief Recently-played tracks */
static struct queue_entry *actual_queue; /**< @brief actual queue */
static struct queue_entry *playing_track;     /**< @brief currenty playing */
static time_t last_playing = (time_t)-1; /**< @brief when last got playing */
static int namepart_lookups_outstanding;
static int  namepart_completions_deferred; /* # of completions not processed */
static const struct cache_type cachetype_string = { 3600 };
static const struct cache_type cachetype_integer = { 3600 };
static GtkWidget *playing_length_label;

/* Debugging --------------------------------------------------------------- */

#if 0
static void describe_widget(const char *name, GtkWidget *w, int indent) {
  int ww, wh, wx, wy;

  if(name)
    fprintf(stderr, "%*s[%s]: '%s'\n", indent, "",
            name, gtk_widget_get_name(w));
  gdk_window_get_position(w->window, &wx, &wy);
  gdk_drawable_get_size(GDK_DRAWABLE(w->window), &ww, &wh);
  fprintf(stderr, "%*s window %p: %dx%d at %dx%d\n",
          indent, "", w->window, ww, wh, wx, wy);
}

static void dump_layout(const struct queuelike *ql) {
  GtkWidget *w;
  char s[20];
  int row, col;
  const struct queue_entry *q;
  
  describe_widget("mainscroll", ql->mainscroll, 0);
  describe_widget("mainlayout", ql->mainlayout, 1);
  for(q = ql->q, row = 0; q; q = q->next, ++row)
    for(col = 0; col < NCOLUMNS + 1; ++col)
      if((w = ql->cells[row * (NCOLUMNS + 1) + col])) {
        sprintf(s, "%dx%d", row, col);
        describe_widget(s, w, 2);
        if(GTK_BIN(w)->child)
          describe_widget(0, w, 3);
      }
}
#endif

/* Track detail lookup ----------------------------------------------------- */

/** @brief Called when a namepart lookup has completed or failed */
static void namepart_completed_or_failed(void) {
  D(("namepart_completed_or_failed"));
  --namepart_lookups_outstanding;
  if(!namepart_lookups_outstanding || namepart_completions_deferred > 24) {
    redisplay_queue(&ql_queue);
    redisplay_queue(&ql_recent);
    namepart_completions_deferred = 0;
  }
}

/** @brief Called when A namepart lookup has completed */
static void namepart_completed(void *v, const char *value) {
  struct callbackdata *cbd = v;

  D(("namepart_completed"));
  cache_put(&cachetype_string, cbd->u.key, value);
  ++namepart_completions_deferred;
  namepart_completed_or_failed();
}

/** @brief Called when a length lookup has completed */
static void length_completed(void *v, long l) {
  struct callbackdata *cbd = v;
  long *value;

  D(("namepart_completed"));
  value = xmalloc(sizeof *value);
  *value = l;
  cache_put(&cachetype_integer, cbd->u.key, value);
  ++namepart_completions_deferred;
  namepart_completed_or_failed();
}

/** @brief Called when a length or namepart lookup has failed */
static void namepart_protocol_error(
  struct callbackdata attribute((unused)) *cbd,
  int attribute((unused)) code,
  const char *msg) {
  D(("namepart_protocol_error"));
  gtk_label_set_text(GTK_LABEL(report_label), msg);
  namepart_completed_or_failed();
}

/** @brief Arrange to fill in a namepart cache entry */
static void namepart_fill(const char *track,
                          const char *context,
                          const char *part,
                          const char *key) {
  struct callbackdata *cbd;

  ++namepart_lookups_outstanding;
  cbd = xmalloc(sizeof *cbd);
  cbd->onerror = namepart_protocol_error;
  cbd->u.key = key;
  disorder_eclient_namepart(client, namepart_completed,
                            track, context, part, cbd);
}

/** @brief Look up a namepart
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
static const char *namepart(const char *track,
                            const char *context,
                            const char *part) {
  char *key;
  const char *value;

  D(("namepart %s %s %s", track, context, part));
  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  value = cache_get(&cachetype_string, key);
  if(!value) {
    D(("deferring..."));
    /* stick a value in the cache so we don't issue another lookup if we
     * revisit */
    cache_put(&cachetype_string, key, value = "?");
    namepart_fill(track, context, part, key);
  }
  return value;
}

/** @brief Called from @ref disobedience/properties.c when we know a name part has changed */
void namepart_update(const char *track,
                     const char *context,
                     const char *part) {
  char *key;

  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  /* Only refetch if it's actually in the cache */
  if(cache_get(&cachetype_string, key))
    namepart_fill(track, context, part, key);
}

/** @brief Look up a track length
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
static long getlength(const char *track) {
  char *key;
  const long *value;
  struct callbackdata *cbd;
  static const long bogus = -1;

  D(("getlength %s", track));
  byte_xasprintf(&key, "length track=%s", track);
  value = cache_get(&cachetype_integer, key);
  if(!value) {
    D(("deferring..."));;
    cache_put(&cachetype_integer, key, value = &bogus);
    ++namepart_lookups_outstanding;
    cbd = xmalloc(sizeof *cbd);
    cbd->onerror = namepart_protocol_error;
    cbd->u.key = key;
    disorder_eclient_length(client, length_completed, track, cbd);
  }
  return *value;
}

/* Column constructors ----------------------------------------------------- */

/** @brief Format the 'when' column */
static GtkWidget *column_when(const struct queuelike attribute((unused)) *ql,
                              const struct queue_entry *q,
                              const char attribute((unused)) *data) {
  char when[64];
  struct tm tm;
  time_t t;

  D(("column_when"));
  switch(q->state) {
  case playing_isscratch:
  case playing_unplayed:
  case playing_random:
    t = q->expected;
    break;
  case playing_failed:
  case playing_no_player:
  case playing_ok:
  case playing_scratched:
  case playing_started:
  case playing_paused:
  case playing_quitting:
    t = q->played;
    break;
  default:
    t = 0;
    break;
  }
  if(t)
    strftime(when, sizeof when, "%H:%M", localtime_r(&t, &tm));
  else
    when[0] = 0;
  NW(label);
  return gtk_label_new(when);
}

/** @brief Format the 'who' column */
static GtkWidget *column_who(const struct queuelike attribute((unused)) *ql,
                             const struct queue_entry *q,
                             const char attribute((unused)) *data) {
  D(("column_who"));
  NW(label);
  return gtk_label_new(q->submitter ? q->submitter : "");
}

/** @brief Format one of the track name columns */
static GtkWidget *column_namepart(const struct queuelike
                                               attribute((unused)) *ql,
                                  const struct queue_entry *q,
                                  const char *data) {
  D(("column_namepart"));
  NW(label);
  return gtk_label_new(namepart(q->track, "display", data));
}

/** @brief Compute the length field */
static const char *text_length(const struct queue_entry *q) {
  long l;
  time_t now;
  char *played = 0, *length = 0;

  /* Work out what to say for the length */
  l = getlength(q->track);
  if(l > 0)
    byte_xasprintf(&length, "%ld:%02ld", l / 60, l % 60);
  else
    byte_xasprintf(&length, "?:??");
  /* For the currently playing track we want to report how much of the track
   * has been played */
  if(q == playing_track) {
    /* log_state() arranges that we re-get the playing data whenever the
     * pause/resume state changes */
    if(last_state & DISORDER_TRACK_PAUSED)
      l = playing_track->sofar;
    else {
      time(&now);
      l = playing_track->sofar + (now - last_playing);
    }
    byte_xasprintf(&played, "%ld:%02ld/%s", l / 60, l % 60, length);
    return played;
  } else
    return length;
}

/** @brief Format the length column */
static GtkWidget *column_length(const struct queuelike attribute((unused)) *ql,
                                const struct queue_entry *q,
                                const char attribute((unused)) *data) {
  D(("column_length"));
  if(q == playing_track) {
    assert(!playing_length_label);
    NW(label);
    playing_length_label = gtk_label_new(text_length(q));
    /* Zot playing_length_label when it is destroyed */
    g_signal_connect(playing_length_label, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &playing_length_label);
    return playing_length_label;
  } else {
    NW(label);
    return gtk_label_new(text_length(q));
  }
  
}

/** @brief Apply a new queue contents, transferring the selection from the old value */
static void update_queue(struct queuelike *ql, struct queue_entry *newq) {
  struct queue_entry *q;

  D(("update_queue"));
  /* Propagate last_click across the change */
  if(ql->last_click) {
    for(q = newq; q; q = q->next) {
      if(!strcmp(q->id, ql->last_click->id)) 
        break;
      ql->last_click = q;
    }
  }
  /* Tell every queue entry which queue owns it */
  for(q = newq; q; q = q->next)
    q->ql = ql;
  /* Switch to the new queue */
  ql->q = newq;
  /* Clean up any selected items that have fallen off */
  for(q = ql->q; q; q = q->next)
    selection_live(ql->selection, q->id);
  selection_cleanup(ql->selection);
}

/** @brief Wrap up a widget for putting into the queue or title */
static GtkWidget *wrap_queue_cell(GtkWidget *label,
                                  const char *name,
                                  int *wp) {
  GtkRequisition req;
  GtkWidget *bg;

  D(("wrap_queue_cell"));
  /* Padding should be in the label so there are no gaps in the
   * background */
  gtk_misc_set_padding(GTK_MISC(label), HCELLPADDING, VCELLPADDING);
  /* Event box is just to hold a background color */
  NW(event_box);
  bg = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(bg), label);
  if(wp) {
    /* Update maximum width */
    gtk_widget_size_request(label, &req);
    if(req.width > *wp) *wp = req.width;
  }
  /* Set widget names */
  gtk_widget_set_name(bg, name);
  gtk_widget_set_name(label, name);
  return bg;
}

/** @brief Create the wrapped widget for a cell in the queue display */
static GtkWidget *get_queue_cell(struct queuelike *ql,
                                 const struct queue_entry *q,
                                 int row,
                                 int col,
                                 const char *name,
                                 int *wp) {
  GtkWidget *label;
  D(("get_queue_cell %d %d", row, col));
  label = columns[col].widget(ql, q, columns[col].data);
  gtk_misc_set_alignment(GTK_MISC(label), columns[col].xalign, 0);
  return wrap_queue_cell(label, name, wp);
}

/** @brief Add a padding cell to the end of a row */
static GtkWidget *get_padding_cell(const char *name) {
  D(("get_padding_cell"));
  NW(label);
  return wrap_queue_cell(gtk_label_new(""), name, 0);
}

/* User button press and menu ---------------------------------------------- */

/** @brief Update widget states in order to reflect the selection status */
static void set_widget_states(struct queuelike *ql) {
  struct queue_entry *q;
  int row, col;

  for(q = ql->q, row = 0; q; q = q->next, ++row) {
    for(col = 0; col < NCOLUMNS + 1; ++col)
      gtk_widget_set_state(ql->cells[row * (NCOLUMNS + 1) + col],
                           selection_selected(ql->selection, q->id) ?
                           GTK_STATE_SELECTED : GTK_STATE_NORMAL);
  }
  /* Might need to change sensitivity of 'Properties' in main menu */
  menu_update(-1);
}

/** @brief Ordering function for queue entries */
static int queue_before(const struct queue_entry *a,
                        const struct queue_entry *b) {
  while(a && a != b)
    a = a->next;
  return !!a;
}

/** @brief A button was pressed and released */
static gboolean queuelike_button_released(GtkWidget attribute((unused)) *widget,
                                          GdkEventButton *event,
                                          gpointer user_data) {
  struct queue_entry *q = user_data, *qq;
  struct queuelike *ql = q->ql;
  struct menuiteminfo *mii;
  int n;
  
  /* Might be a release left over from a drag */
  if(ql->swallow_release) {
    ql->swallow_release = 0;
    return FALSE;                       /* propagate */
  }

  if(event->type == GDK_BUTTON_PRESS
     && event->button == 3) {
    /* Right button click.
     * If the current item is not selected then switch the selection to just
     * this item */
    if(q && !selection_selected(ql->selection, q->id)) {
      selection_empty(ql->selection);
      selection_set(ql->selection, q->id, 1);
      ql->last_click = q;
      set_widget_states(ql);
    }
    /* Set the sensitivity of each menu item and (re-)establish the signal
     * handlers */
    for(n = 0; ql->menuitems[n].name; ++n) {
      if(ql->menuitems[n].handlerid)
        g_signal_handler_disconnect(ql->menuitems[n].w,
                                    ql->menuitems[n].handlerid);
      gtk_widget_set_sensitive(ql->menuitems[n].w,
                               ql->menuitems[n].sensitive(ql,
                                                          &ql->menuitems[n],
                                                          q));
      mii = xmalloc(sizeof *mii);
      mii->ql = ql;
      mii->q = q;
      ql->menuitems[n].handlerid = g_signal_connect
        (ql->menuitems[n].w, "activate",
         G_CALLBACK(ql->menuitems[n].activate), mii);
    }
    /* Update the menu according to context */
    gtk_widget_show_all(ql->menu);
    gtk_menu_popup(GTK_MENU(ql->menu), 0, 0, 0, 0,
                   event->button, event->time);
    return TRUE;                        /* hide the click from other widgets */
  }
  if(event->type == GDK_BUTTON_RELEASE
     && event->button == 1) {
    /* no modifiers: select this, unselect everything else, set last click
     * +ctrl: flip selection of this, set last click
     * +shift: select from last click to here, don't set last click
     * +ctrl+shift: select from last click to here, set last click
     */
    switch(event->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) {
    case 0:
      selection_empty(ql->selection);
      selection_set(ql->selection, q->id, 1);
      ql->last_click = q;
      break;
    case GDK_CONTROL_MASK:
      selection_flip(ql->selection, q->id);
      ql->last_click = q;
      break;
    case GDK_SHIFT_MASK:
    case GDK_SHIFT_MASK|GDK_CONTROL_MASK:
      if(ql->last_click) {
        if(!(event->state & GDK_CONTROL_MASK))
          selection_empty(ql->selection);
        selection_set(ql->selection, q->id, 1);
        qq = q;
        if(queue_before(ql->last_click, q))
          while(qq != ql->last_click) {
            qq = qq->prev;
            selection_set(ql->selection, qq->id, 1);
          }
        else
          while(qq != ql->last_click) {
            qq = qq->next;
            selection_set(ql->selection, qq->id, 1);
          }
        if(event->state & GDK_CONTROL_MASK)
          ql->last_click = q;
      }
      break;
    }
    set_widget_states(ql);
    gtk_widget_queue_draw(ql->mainlayout);
  }
  return FALSE;                         /* propagate */
}

/** @brief A button was pressed or released on the mainlayout
 *
 * For debugging only at the moment. */
static gboolean mainlayout_button(GtkWidget attribute((unused)) *widget,
                                  GdkEventButton attribute((unused)) *event,
                                  gpointer attribute((unused)) user_data) {
  return FALSE;                         /* propagate */
}

/** @brief Select all entries in a queue */
void queue_select_all(struct queuelike *ql) {
  struct queue_entry *qq;

  for(qq = ql->q; qq; qq = qq->next)
    selection_set(ql->selection, qq->id, 1);
  ql->last_click = 0;
  set_widget_states(ql);
}

/** @brief Pop up properties for selected tracks */
void queue_properties(struct queuelike *ql) {
  struct vector v;
  const struct queue_entry *qq;

  vector_init(&v);
  for(qq = ql->q; qq; qq = qq->next)
    if(selection_selected(ql->selection, qq->id))
      vector_append(&v, (char *)qq->track);
  if(v.nvec)
    properties(v.nvec, v.vec);
}

/* Drag and drop rearrangement --------------------------------------------- */

/** @brief Return nonzero if @p is a draggable row */
static int draggable_row(const struct queue_entry *q) {
  return q->ql == &ql_queue && q != playing_track;
}

/** @brief Called when a drag begings */
static void queue_drag_begin(GtkWidget attribute((unused)) *widget, 
                             GdkDragContext attribute((unused)) *dc,
                             gpointer data) {
  struct queue_entry *q = data;
  struct queuelike *ql = q->ql;

  /* Make sure the playing track is not selected, since it cannot be dragged */
  if(playing_track)
    selection_set(ql->selection, playing_track->id, 0);
  /* If the dragged item is not in the selection then change the selection to
   * just that */
  if(!selection_selected(ql->selection, q->id)) {
    selection_empty(ql->selection);
    selection_set(ql->selection, q->id, 1);
    set_widget_states(ql);
  }
  /* Ignore the eventual button release */
  ql->swallow_release = 1;
  /* Create dropzones */
  add_drag_targets(ql);
}

/** @brief Convert @p id back into a queue entry and a screen row number */
static struct queue_entry *findentry(struct queuelike *ql,
                                     const char *id,
                                     int *rowp) {
  int row;
  struct queue_entry *q;

  if(id) {
    for(q = ql->q, row = 0; q && strcmp(q->id, id); q = q->next, ++row)
      ;
  } else {
    q = 0;
    row = playing_track ? 0 : -1;
  }
  if(rowp) *rowp = row;
  return q;
}

/** @brief Called when data is dropped */
static gboolean queue_drag_drop(GtkWidget attribute((unused)) *widget,
                                GdkDragContext *drag_context,
                                gint attribute((unused)) x,
                                gint attribute((unused)) y,
                                guint when,
                                gpointer user_data) {
  struct queuelike *ql = &ql_queue;
  const char *id = user_data;
  struct vector vec;
  struct queue_entry *q;

  if(!id || (playing_track && !strcmp(id, playing_track->id)))
    id = "";
  vector_init(&vec);
  for(q = ql->q; q; q = q->next)
    if(q != playing_track && selection_selected(ql->selection, q->id))
      vector_append(&vec, (char *)q->id);
  disorder_eclient_moveafter(client, id, vec.nvec, (const char **)vec.vec,
                             0/*completed*/, 0/*v*/);
  gtk_drag_finish(drag_context, TRUE, TRUE, when);
  /* Destroy dropzones */
  remove_drag_targets(ql);
  return TRUE;
}

/** @brief Called when we enter, or move within, a drop zone */
static gboolean queue_drag_motion(GtkWidget attribute((unused)) *widget,
                                  GdkDragContext *drag_context,
                                  gint attribute((unused)) x,
                                  gint attribute((unused)) y,
                                  guint when,
                                  gpointer user_data) {
  struct queuelike *ql = &ql_queue;
  const char *id = user_data;
  int row;
  struct queue_entry *q = findentry(ql, id, &row);

  if(!id || q) {
    if(!ql->dragmark) {
      NW(event_box);
      ql->dragmark = gtk_event_box_new();
      g_signal_connect(ql->dragmark, "destroy",
                       G_CALLBACK(gtk_widget_destroyed), &ql->dragmark);
      gtk_widget_set_size_request(ql->dragmark, 10240, row ? 4 : 2);
      gtk_widget_set_name(ql->dragmark, "queue-drag");
      gtk_layout_put(GTK_LAYOUT(ql->mainlayout), ql->dragmark, 0, 
                     (row + 1) * ql->mainrowheight - !!row);
    } else
      gtk_layout_move(GTK_LAYOUT(ql->mainlayout), ql->dragmark, 0, 
                      (row + 1) * ql->mainrowheight - !!row);
    gtk_widget_show(ql->dragmark);
    gdk_drag_status(drag_context, GDK_ACTION_MOVE, when);
    return TRUE;
  } else
    /* ID has gone AWOL */
    return FALSE;
}                              

/** @brief Called when we leave a drop zone */
static void queue_drag_leave(GtkWidget attribute((unused)) *widget,
                             GdkDragContext attribute((unused)) *drag_context,
                             guint attribute((unused)) when,
                             gpointer attribute((unused)) user_data) {
  struct queuelike *ql = &ql_queue;
  
  if(ql->dragmark)
    gtk_widget_hide(ql->dragmark);
}

/** @brief Add a drag target at position @p y
 *
 * @p id is the track to insert the moved tracks after, and might be 0 to
 * insert before the start. */
static void add_drag_target(struct queuelike *ql, int y, int row,
                            const char *id) {
  GtkWidget *eventbox;

  assert(ql->dropzones[row] == 0);
  NW(event_box);
  eventbox = gtk_event_box_new();
  /* Make the target zone invisible */
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(eventbox), FALSE);
  /* Make it large enough */
  gtk_widget_set_size_request(eventbox, 10240, 
                              y ? ql->mainrowheight : ql->mainrowheight / 2);
  /* Position it */
  gtk_layout_put(GTK_LAYOUT(ql->mainlayout), eventbox, 0,
                 y ? y - ql->mainrowheight / 2 : 0);
  /* Mark it as capable of receiving drops */
  gtk_drag_dest_set(eventbox,
                    0,
                    dragtargets, NDRAGTARGETS, GDK_ACTION_MOVE);
  g_signal_connect(eventbox, "drag-drop",
                   G_CALLBACK(queue_drag_drop), (char *)id);
  /* Monitor drag motion */
  g_signal_connect(eventbox, "drag-motion",
                   G_CALLBACK(queue_drag_motion), (char *)id);
  g_signal_connect(eventbox, "drag-leave",
                   G_CALLBACK(queue_drag_leave), (char *)id);
  /* The widget needs to be shown to receive drags */
  gtk_widget_show(eventbox);
  /* Remember the drag targets */
  ql->dropzones[row] = eventbox;
  g_signal_connect(eventbox, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &ql->dropzones[row]);
}

/** @brief Create dropzones for dragging into */
static void add_drag_targets(struct queuelike *ql) {
  int row, y;
  struct queue_entry *q;

  /* Create an array to store the widgets */
  ql->dropzones = xcalloc(ql->nrows, sizeof (GtkWidget *));
  y = 0;
  /* Add a drag target before the first row provided it's not the playing
   * track */
  if(!playing_track || ql->q != playing_track)
    add_drag_target(ql, 0, 0, 0);
  /* Put a drag target at the bottom of every row */
  for(q = ql->q, row = 0; q; q = q->next, ++row) {
    y += ql->mainrowheight;
    add_drag_target(ql, y, row, q->id);
  }
}

/** @brief Remove the dropzones */
static void remove_drag_targets(struct queuelike *ql) {
  int row;

  for(row = 0; row < ql->nrows; ++row) {
    if(ql->dropzones[row]) {
      DW(event_box);
      gtk_widget_destroy(ql->dropzones[row]);
    }
    assert(ql->dropzones[row] == 0);
  }
}

/* Layout ------------------------------------------------------------------ */

/** @brief Redisplay a queue */
static void redisplay_queue(struct queuelike *ql) {
  struct queue_entry *q;
  int row, col;
  GList *c, *children;
  const char *name;
  GtkRequisition req;  
  GtkWidget *w;
  int maxwidths[NCOLUMNS], x, y, titlerowheight;
  int totalwidth = 10240;               /* TODO: can we be less blunt */

  D(("redisplay_queue"));
  /* Eliminate all the existing widgets and start from scratch */
  for(c = children = gtk_container_get_children(GTK_CONTAINER(ql->mainlayout));
      c;
      c = c->next) {
    /* Destroy both the label and the eventbox */
    if(GTK_BIN(c->data)->child) {
      DW(label);
      gtk_widget_destroy(GTK_BIN(c->data)->child);
    }
    DW(event_box);
    gtk_widget_destroy(GTK_WIDGET(c->data));
  }
  g_list_free(children);
  /* Adjust the row count */
  for(q = ql->q, ql->nrows = 0; q; q = q->next)
    ++ql->nrows;
  /* We need to create all the widgets before we can position them */
  ql->cells = xcalloc(ql->nrows * (NCOLUMNS + 1), sizeof *ql->cells);
  /* Minimum width is given by the column headings */
  for(col = 0; col < NCOLUMNS; ++col) {
    /* Reset size so we don't inherit last iteration's maximum size */
    gtk_widget_set_size_request(GTK_BIN(ql->titlecells[col])->child, -1, -1);
    gtk_widget_size_request(GTK_BIN(ql->titlecells[col])->child, &req);
    maxwidths[col] = req.width;
  }
  /* Find the vertical size of the title bar */
  gtk_widget_size_request(GTK_BIN(ql->titlecells[0])->child, &req);
  titlerowheight = req.height;
  y = 0;
  if(ql->nrows) {
    /* Construct the widgets */
    for(q = ql->q, row = 0; q; q = q->next, ++row) {
      /* Figure out the widget name for this row */
      if(q == playing_track) name = "row-playing";
      else name = row % 2 ? "row-even" : "row-odd";
      /* Make the widget for each column */
      for(col = 0; col <= NCOLUMNS; ++col) {
        /* Create and store the widget */
        if(col < NCOLUMNS)
          w = get_queue_cell(ql, q, row, col, name, &maxwidths[col]);
        else
          w = get_padding_cell(name);
        ql->cells[row * (NCOLUMNS + 1) + col] = w;
        /* Maybe mark it draggable */
        if(draggable_row(q)) {
          gtk_drag_source_set(w, GDK_BUTTON1_MASK,
                              dragtargets, NDRAGTARGETS, GDK_ACTION_MOVE);
          g_signal_connect(w, "drag-begin", G_CALLBACK(queue_drag_begin), q);
        }
        /* Catch button presses */
        g_signal_connect(w, "button-release-event",
                         G_CALLBACK(queuelike_button_released), q);
        g_signal_connect(w, "button-press-event",
                         G_CALLBACK(queuelike_button_released), q);
      }
    }
    /* ...and of each row in the main layout */
    gtk_widget_size_request(GTK_BIN(ql->cells[0])->child, &req);
    ql->mainrowheight = req.height;
    /* Now we know the maximum width of each column we can set the size of
     * everything and position it */
    for(row = 0, q = ql->q; row < ql->nrows; ++row, q = q->next) {
      x = 0;
      for(col = 0; col < NCOLUMNS; ++col) {
        w = ql->cells[row * (NCOLUMNS + 1) + col];
        gtk_widget_set_size_request(GTK_BIN(w)->child,
                                    maxwidths[col], -1);
        gtk_layout_put(GTK_LAYOUT(ql->mainlayout), w, x, y);
        x += maxwidths[col];
      }
      w = ql->cells[row * (NCOLUMNS + 1) + col];
      gtk_widget_set_size_request(GTK_BIN(w)->child,
                                  totalwidth - x, -1);
      gtk_layout_put(GTK_LAYOUT(ql->mainlayout), w, x, y);
      y += ql->mainrowheight;
    }
  }
  /* Titles */
  x = 0;
  for(col = 0; col < NCOLUMNS; ++col) {
    gtk_widget_set_size_request(GTK_BIN(ql->titlecells[col])->child,
                                maxwidths[col], -1);
    gtk_layout_move(GTK_LAYOUT(ql->titlelayout), ql->titlecells[col], x, 0);
    x += maxwidths[col];
  }
  gtk_widget_set_size_request(GTK_BIN(ql->titlecells[col])->child,
                              totalwidth - x, -1);
  gtk_layout_move(GTK_LAYOUT(ql->titlelayout), ql->titlecells[col], x, 0);
  /* Set the states */
  set_widget_states(ql);
  /* Make sure it's all visible */
  gtk_widget_show_all(ql->mainlayout);
  gtk_widget_show_all(ql->titlelayout);
  /* Layouts might shrink to arrange for the area they shrink out of to be
   * redrawn */
  gtk_widget_queue_draw(ql->mainlayout);
  gtk_widget_queue_draw(ql->titlelayout);
  /* Adjust the size of the layout */
  gtk_layout_set_size(GTK_LAYOUT(ql->mainlayout), x, y);
  gtk_layout_set_size(GTK_LAYOUT(ql->titlelayout), x, titlerowheight);
  gtk_widget_set_size_request(ql->titlelayout, -1, titlerowheight);
}

/** @brief Called with new queue/recent contents */ 
static void queuelike_completed(void *v, struct queue_entry *q) {
  struct callbackdata *cbd = v;
  struct queuelike *ql = cbd->u.ql;

  D(("queuelike_complete"));
  /* Install the new queue */
  update_queue(ql, ql->fixup(q));
  /* Update the display */
  redisplay_queue(ql);
  if(ql->notify)
    ql->notify();
  /* Update sensitivity of main menu items */
  menu_update(-1);
}

/** @brief Called with a new currently playing track */
static void playing_completed(void attribute((unused)) *v,
                              struct queue_entry *q) {
  struct callbackdata cbd;
  D(("playing_completed"));
  playing_track = q;
  /* Record when we got the playing track data so we know how old the 'sofar'
   * field is */
  time(&last_playing);
  cbd.u.ql = &ql_queue;
  queuelike_completed(&cbd, actual_queue);
}

/** @brief Called when the queue is scrolled */
static void queue_scrolled(GtkAdjustment *adjustment,
                           gpointer user_data) {
  GtkAdjustment *titleadj = user_data;

  D(("queue_scrolled"));
  gtk_adjustment_set_value(titleadj, adjustment->value);
}

/** @brief Create a queuelike thing (queue/recent) */
static GtkWidget *queuelike(struct queuelike *ql,
                            struct queue_entry *(*fixup)(struct queue_entry *),
                            void (*notify)(void),
                            struct queue_menuitem *menuitems,
                            const char *name) {
  GtkWidget *vbox, *mainscroll, *titlescroll, *label;
  GtkAdjustment *mainadj, *titleadj;
  int col, n;

  D(("queuelike"));
  ql->fixup = fixup;
  ql->notify = notify;
  ql->menuitems = menuitems;
  ql->name = name;
  ql->mainrowheight = !0;                /* else division by 0 */
  ql->selection = selection_new();
  /* Create the layouts */
  NW(layout);
  ql->mainlayout = gtk_layout_new(0, 0);
  NW(layout);
  ql->titlelayout = gtk_layout_new(0, 0);
  /* Scroll the layouts */
  ql->mainscroll = mainscroll = scroll_widget(ql->mainlayout, name);
  titlescroll = scroll_widget(ql->titlelayout, name);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(titlescroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  mainadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(mainscroll));
  titleadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(titlescroll));
  g_signal_connect(mainadj, "changed", G_CALLBACK(queue_scrolled), titleadj);
  g_signal_connect(mainadj, "value-changed", G_CALLBACK(queue_scrolled), titleadj);
  /* Fill the titles and put them anywhere */
  for(col = 0; col < NCOLUMNS; ++col) {
    NW(label);
    label = gtk_label_new(columns[col].name);
    gtk_misc_set_alignment(GTK_MISC(label), columns[col].xalign, 0);
    ql->titlecells[col] = wrap_queue_cell(label, "row-title", 0);
    gtk_layout_put(GTK_LAYOUT(ql->titlelayout), ql->titlecells[col], 0, 0);
  }
  ql->titlecells[col] = get_padding_cell("row-title");
  gtk_layout_put(GTK_LAYOUT(ql->titlelayout), ql->titlecells[col], 0, 0);
  /* Pack the lot together in a vbox */
  NW(vbox);
  vbox = gtk_vbox_new(0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), titlescroll, 0, 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), mainscroll, 1, 1, 0);
  /* Create the popup menu */
  NW(menu);
  ql->menu = gtk_menu_new();
  g_signal_connect(ql->menu, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &ql->menu);
  for(n = 0; menuitems[n].name; ++n) {
    NW(menu_item);
    menuitems[n].w = gtk_menu_item_new_with_label(menuitems[n].name);
    gtk_menu_attach(GTK_MENU(ql->menu), menuitems[n].w, 0, 1, n, n + 1);
  }
  g_object_set_data(G_OBJECT(vbox), "type", (void *)&tabtype_queue);
  g_object_set_data(G_OBJECT(vbox), "queue", ql);
  /* Catch button presses */
  g_signal_connect(ql->mainlayout, "button-release-event",
                   G_CALLBACK(mainlayout_button), ql);
#if 0
  g_signal_connect(ql->mainlayout, "button-press-event",
                   G_CALLBACK(mainlayout_button), ql);
#endif
  return vbox;
}

/* Popup menu items -------------------------------------------------------- */

/** @brief Count the number of items selected */
static int queue_count_selected(const struct queuelike *ql) {
  return hash_count(ql->selection);
}

/** @brief Count the number of items selected */
static int queue_count_entries(const struct queuelike *ql) {
  int nitems = 0;
  const struct queue_entry *q;

  for(q = ql->q; q; q = q->next)
    ++nitems;
  return nitems;
}

/** @brief Count the number of items selected, excluding the playing track if
 * there is one */
static int count_selected_nonplaying(const struct queuelike *ql) {
  int nselected = queue_count_selected(ql);

  if(ql->q == playing_track && selection_selected(ql->selection, ql->q->id))
    --nselected;
  return nselected;
}

/** @brief Determine whether the scratch option should be sensitive */
static int scratch_sensitive(struct queuelike attribute((unused)) *ql,
                             struct queue_menuitem attribute((unused)) *m,
                             struct queue_entry attribute((unused)) *q) {
  /* We can scratch if the playing track is selected */
  return (playing_track
          && (disorder_eclient_state(client) & DISORDER_CONNECTED)
          && selection_selected(ql->selection, playing_track->id));
}

/** @brief Scratch the playing track */
static void scratch_activate(GtkMenuItem attribute((unused)) *menuitem,
                             gpointer attribute((unused)) user_data) {
  if(playing_track)
    disorder_eclient_scratch(client, playing_track->id, 0, 0);
}

/** @brief Determine whether the remove option should be sensitive */
static int remove_sensitive(struct queuelike *ql,
                            struct queue_menuitem attribute((unused)) *m,
                            struct queue_entry *q) {
  /* We can remove if we're hovering over a particular track or any non-playing
   * tracks are selected */
  return ((disorder_eclient_state(client) & DISORDER_CONNECTED)
          && ((q
               && q != playing_track)
              || count_selected_nonplaying(ql)));
}

/** @brief Remove selected track(s) */
static void remove_activate(GtkMenuItem attribute((unused)) *menuitem,
                            gpointer user_data) {
  const struct menuiteminfo *mii = user_data;
  struct queue_entry *q = mii->q;
  struct queuelike *ql = mii->ql;

  if(count_selected_nonplaying(mii->ql)) {
    /* Remove selected tracks */
    for(q = ql->q; q; q = q->next)
      if(selection_selected(ql->selection, q->id) && q != playing_track)
        disorder_eclient_remove(client, q->id, 0, 0);
  } else if(q)
    /* Remove just the hovered track */
    disorder_eclient_remove(client, q->id, 0, 0);
}

/** @brief Determine whether the properties menu option should be sensitive */
static int properties_sensitive(struct queuelike *ql,
                                struct queue_menuitem attribute((unused)) *m,
                                struct queue_entry attribute((unused)) *q) {
  /* "Properties" is sensitive if at least something is selected */
  return (hash_count(ql->selection) > 0
          && (disorder_eclient_state(client) & DISORDER_CONNECTED));
}

/** @brief Pop up properties for the selected tracks */
static void properties_activate(GtkMenuItem attribute((unused)) *menuitem,
                                gpointer user_data) {
  const struct menuiteminfo *mii = user_data;
  
  queue_properties(mii->ql);
}

/** @brief Determine whether the select all menu option should be sensitive */
static int selectall_sensitive(struct queuelike *ql,
                               struct queue_menuitem attribute((unused)) *m,
                               struct queue_entry attribute((unused)) *q) {
  /* Sensitive if there is anything to select */
  return !!ql->q;
}

/** @brief Select all tracks */
static void selectall_activate(GtkMenuItem attribute((unused)) *menuitem,
                               gpointer user_data) {
  const struct menuiteminfo *mii = user_data;
  queue_select_all(mii->ql);
}

/* The queue --------------------------------------------------------------- */

/** @brief Fix up the queue by sticking the currently playing track on the front */
static struct queue_entry *fixup_queue(struct queue_entry *q) {
  D(("fixup_queue"));
  actual_queue = q;
  if(playing_track) {
    if(actual_queue)
      actual_queue->prev = playing_track;
    playing_track->next = actual_queue;
    return playing_track;
  } else
    return actual_queue;
}

/** @brief Adjust track played label
 *
 *  Called regularly to adjust the so-far played label (redrawing the whole
 * queue once a second makes disobedience occupy >10% of the CPU on my Athlon
 * which is ureasonable expensive) */
static gboolean adjust_sofar(gpointer attribute((unused)) data) {
  if(playing_length_label && playing_track)
    gtk_label_set_text(GTK_LABEL(playing_length_label),
                       text_length(playing_track));
  return TRUE;
}

/** @brief Popup menu for the queue
 *
 * Properties first so that finger trouble is less dangerous. */
static struct queue_menuitem queue_menu[] = {
  { "Track properties", properties_activate, properties_sensitive, 0, 0 },
  { "Select all tracks", selectall_activate, selectall_sensitive, 0, 0 },
  { "Scratch track", scratch_activate, scratch_sensitive, 0, 0 },
  { "Remove track from queue", remove_activate, remove_sensitive, 0, 0 },
  { 0, 0, 0, 0, 0 }
};

/** @brief Called whenever @ref DISORDER_PLAYING or @ref DISORDER_TRACK_PAUSED changes
 *
 * We monitor pause/resume as well as whether the track is playing in order to
 * keep the time played so far up to date correctly.  See playing_completed().
 */
static void playing_update(void attribute((unused)) *v) {
  D(("playing_update"));
  gtk_label_set_text(GTK_LABEL(report_label), "updating playing track");
  disorder_eclient_playing(client, playing_completed, 0);
}

/** @brief Create the queue widget */
GtkWidget *queue_widget(void) {
  D(("queue_widget"));
  /* Arrange periodic update of the so-far played field */
  g_timeout_add(1000/*ms*/, adjust_sofar, 0);
  /* Arrange a callback whenever the playing state changes */ 
  register_monitor(playing_update, 0,  DISORDER_PLAYING|DISORDER_TRACK_PAUSED);
  /* We pass choose_update() as our notify function since the choose screen
   * marks tracks that are playing/in the queue. */
  return queuelike(&ql_queue, fixup_queue, choose_update, queue_menu,
                   "queue");
}

/** @brief Arrange an update of the queue widget
 *
 * Called when a track is added to the queue, removed from the queue (by user
 * cmmand or because it is to be played) or moved within the queue
 */
void queue_update(void) {
  struct callbackdata *cbd;

  D(("queue_update"));
  cbd = xmalloc(sizeof *cbd);
  cbd->onerror = 0;
  cbd->u.ql = &ql_queue;
  gtk_label_set_text(GTK_LABEL(report_label), "updating queue");
  disorder_eclient_queue(client, queuelike_completed, cbd);
}

/* Recently played tracks -------------------------------------------------- */

/** @brief Fix up the recently played list
 *
 * It's in the wrong order!  TODO fix this globally */
static struct queue_entry *fixup_recent(struct queue_entry *q) {
  struct queue_entry *qr = 0,  *qn;

  D(("fixup_recent"));
  while(q) {
    qn = q->next;
    /* Swap next/prev pointers */
    q->next = q->prev;
    q->prev = qn;
    /* Remember last node for new head */
    qr = q;
    /* Next node */
    q = qn;
  }
  return qr;
}

/** @brief Pop-up menu for recently played list */
static struct queue_menuitem recent_menu[] = {
  { "Track properties", properties_activate, properties_sensitive,0, 0 },
  { "Select all tracks", selectall_activate, selectall_sensitive, 0, 0 },
  { 0, 0, 0, 0, 0 }
};

/** @brief Create the recently-played list */
GtkWidget *recent_widget(void) {
  D(("recent_widget"));
  return queuelike(&ql_recent, fixup_recent, 0, recent_menu, "recent");
}

/** @brief Update the recently played list
 *
 * Called whenever a track is added to it or removed from it.
 */
void recent_update(void) {
  struct callbackdata *cbd;

  D(("recent_update"));
  cbd = xmalloc(sizeof *cbd);
  cbd->onerror = 0;
  cbd->u.ql = &ql_recent;
  gtk_label_set_text(GTK_LABEL(report_label), "updating recently played list");
  disorder_eclient_recent(client, queuelike_completed, cbd);
}

/* Main menu plumbing ------------------------------------------------------ */

static int queue_properties_sensitive(GtkWidget *w) {
  return (!!queue_count_selected(g_object_get_data(G_OBJECT(w), "queue"))
          && (disorder_eclient_state(client) & DISORDER_CONNECTED));
}

static int queue_selectall_sensitive(GtkWidget *w) {
  return !!queue_count_entries(g_object_get_data(G_OBJECT(w), "queue"));
}

static void queue_properties_activate(GtkWidget *w) {
  queue_properties(g_object_get_data(G_OBJECT(w), "queue"));
}

static void queue_selectall_activate(GtkWidget *w) {
  queue_select_all(g_object_get_data(G_OBJECT(w), "queue"));
}

static const struct tabtype tabtype_queue = {
  queue_properties_sensitive,
  queue_selectall_sensitive,
  queue_properties_activate,
  queue_selectall_activate,
};

/* Other entry points ------------------------------------------------------ */

/** @brief Return nonzero if @p track is in the queue */
int queued(const char *track) {
  struct queue_entry *q;

  D(("queued %s", track));
  for(q = ql_queue.q; q; q = q->next)
    if(!strcmp(q->track, track))
      return 1;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
