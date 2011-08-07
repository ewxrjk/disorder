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
/** @file disobedience/properties.c
 * @brief Track properties editor
 */
#include "disobedience.h"

struct prefdata;

static void kickoff_namepart(struct prefdata *f);
static void completed_namepart(struct prefdata *f);
static const char *get_edited_namepart(struct prefdata *f);
static void set_edited_namepart(struct prefdata *f, const char *value);
static void set_namepart(struct prefdata *f, const char *value);
static void set_namepart_completed(void *v, const char *err);

static void kickoff_string(struct prefdata *f);
static void completed_string(struct prefdata *f);
static const char *get_edited_string(struct prefdata *f);
static void set_edited_string(struct prefdata *f, const char *value);
static void set_string(struct prefdata *f, const char *value);

static void kickoff_boolean(struct prefdata *f);
static void completed_boolean(struct prefdata *f);
static const char *get_edited_boolean(struct prefdata *f);
static void set_edited_boolean(struct prefdata *f, const char *value);
static void set_boolean(struct prefdata *f, const char *value);

static void prefdata_completed(void *v, const char *err, const char *value);

static void properties_ok(GtkButton *button, gpointer userdata);
static void properties_apply(GtkButton *button, gpointer userdata);
static void properties_cancel(GtkButton *button, gpointer userdata);
static void properties_help(GtkButton *button, gpointer userdata);

static void properties_logged_in(const char *event,
                                 void *eventdata,
                                 void *callbackdata);

/** @brief Data for a single preference */
struct prefdata {
  const char *track;
  int row;
  const struct pref *p;                 /**< @brief kind of preference */
  const char *value;                    /**< @brief value from server  */
  GtkWidget *widget;
};

/* The type of a preference is the collection of callbacks needed to get,
 * display and set it */
struct preftype {
  void (*kickoff)(struct prefdata *f);
  /* Kick off the request to fetch the pref from the server. */

  void (*completed)(struct prefdata *f);
  /* Called when the value comes back in; creates the widget. */

  const char *(*get_edited)(struct prefdata *f);
  /* Get the edited value from the widget. */

  /** @brief Update the edited value */
  void (*set_edited)(struct prefdata *f, const char *value);

  void (*set)(struct prefdata *f, const char *value);
  /* Set the new value and (if necessary) arrange for our display to update. */
};

/* A namepart pref */
static const struct preftype preftype_namepart = {
  kickoff_namepart,
  completed_namepart,
  get_edited_namepart,
  set_edited_namepart,
  set_namepart
};

/* A string pref */
static const struct preftype preftype_string = {
  kickoff_string,
  completed_string,
  get_edited_string,
  set_edited_string,
  set_string
};

/* A boolean pref */
static const struct preftype preftype_boolean = {
  kickoff_boolean,
  completed_boolean,
  get_edited_boolean,
  set_edited_boolean,
  set_boolean
};

/* @brief The known prefs for each track */
static const struct pref {
  const char *label;                    /**< @brief user-level description */
  const char *part;                     /**< @brief protocol-level tag */
  const char *default_value;            /**< @brief default value or NULL */
  const struct preftype *type;          /**< @brief underlying data type */
} prefs[] = {
  { "Artist", "artist", 0, &preftype_namepart },
  { "Album", "album", 0, &preftype_namepart },
  { "Title", "title", 0, &preftype_namepart },
  { "Tags", "tags", "", &preftype_string },
  { "Weight", "weight", "90000", &preftype_string },
  { "Random", "pick_at_random", "1", &preftype_boolean },
};

#define NPREFS (int)(sizeof prefs / sizeof *prefs)

/* Buttons that appear at the bottom of the window */
static struct button buttons[] = {
  {
    GTK_STOCK_HELP,
    properties_help,
    "Go to manual",
    0,
    gtk_box_pack_start,
  },
  {
    GTK_STOCK_OK,
    properties_ok,
    "Apply all changes and close window",
    0,
    gtk_box_pack_end,
  },
  {
    GTK_STOCK_CANCEL,
    properties_cancel,
    "Discard all changes and close window",
    0,
    gtk_box_pack_end
  },
  {
    GTK_STOCK_APPLY,
    properties_apply,
    "Apply all changes and keep window open",
    0,
    gtk_box_pack_end,
  },
};

#define NBUTTONS (int)(sizeof buttons / sizeof *buttons)

static int prefs_unfilled;              /* Prefs remaining to get */
static int prefs_total;                 /* Total prefs */
static struct prefdata *prefdatas;      /* Current prefdatas */
static GtkWidget *properties_window;
static GtkWidget *properties_table;
static struct progress_window *pw;
static event_handle properties_event;

static void propagate_clicked(GtkButton attribute((unused)) *button,
                              gpointer userdata) {
  struct prefdata *f = (struct prefdata *)userdata, *g;
  int p;
  const char *value = f->p->type->get_edited(f);
  
  for(p = 0; p < prefs_total; ++p) {
    g = &prefdatas[p];
    if(f->p == g->p && f != g)
      g->p->type->set_edited(g, value);
  }
}

/** @brief Keypress handler */
static gboolean properties_keypress(GtkWidget attribute((unused)) *widget,
                                    GdkEventKey *event,
                                    gpointer attribute((unused)) user_data) {
  if(event->state)
    return FALSE;
  switch(event->keyval) {
  case GDK_Return:
    properties_ok(0, 0);
    return TRUE;
  case GDK_Escape:
    properties_cancel(0, 0);
    return TRUE;
  default:
    return FALSE;
  }
}

void properties(int ntracks, const char **tracks,
                GtkWidget *parent) {
  int n, m;
  struct prefdata *f;
  GtkWidget *buttonbox, *vbox, *label, *entry, *propagate;
  
  /* If no tracks, do nothign */
  if(!ntracks)
    return;
  /* If there is a properties window open then just bring it to the
   * front.  It might not have the right values in... */
  if(properties_window) {
    if(!prefs_unfilled)
      gtk_window_present(GTK_WINDOW(properties_window));
    return;
  }
  assert(properties_table == 0);
  if(ntracks > INT_MAX / NPREFS) {
    popup_msg(GTK_MESSAGE_ERROR, "Too many tracks selected");
    return;
  }
  properties_event = event_register("logged-in", properties_logged_in, 0);
  /* Create a new properties window */
  properties_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(properties_window, tool_style);
  g_signal_connect(properties_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &properties_window);
  /* Keyboard shortcuts */
  g_signal_connect(properties_window, "key-press-event",
                   G_CALLBACK(properties_keypress), 0);
  /* Most of the action is the table of preferences */
  properties_table = gtk_table_new((NPREFS + 1) * ntracks, 2 + ntracks > 1,
                                   FALSE);
  gtk_widget_set_style(properties_table, tool_style);
  g_signal_connect(properties_table, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &properties_table);
  gtk_window_set_title(GTK_WINDOW(properties_window), "Track Properties");
  /* Create labels for each pref of each track and kick off requests to the
   * server to fill in the values */
  prefs_total = NPREFS * ntracks;
  prefdatas = xcalloc(prefs_total, sizeof *prefdatas);
  for(n = 0; n < ntracks; ++n) {
    /* The track itself */
    /* Caption */
    label = gtk_label_new("Track");
    gtk_widget_set_style(label, tool_style);
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0);
    gtk_table_attach(GTK_TABLE(properties_table),
                     label,
		     0, 1,
		     (NPREFS + 1) * n, (NPREFS + 1) * n + 1,
		     GTK_FILL, 0,
		     1, 1);
    /* The track name */
    entry = gtk_entry_new();
    gtk_widget_set_style(entry, tool_style);
    gtk_entry_set_text(GTK_ENTRY(entry), tracks[n]);
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
    gtk_table_attach(GTK_TABLE(properties_table),
                     entry,
		     1, 2,
		     (NPREFS + 1) * n, (NPREFS + 1) * n + 1,
		     GTK_EXPAND|GTK_FILL, 0,
		     1, 1);
    /* Each preference */
    for(m = 0; m < NPREFS; ++m) {
      /* Caption */
      label = gtk_label_new(prefs[m].label);
      gtk_widget_set_style(label, tool_style);
      gtk_misc_set_alignment(GTK_MISC(label), 1, 0);
      gtk_table_attach(GTK_TABLE(properties_table),
                       label,
		       0, 1,
		       (NPREFS + 1) * n + 1 + m, (NPREFS + 1) * n + 2 + m,
		       GTK_FILL/*xoptions*/, 0/*yoptions*/,
		       1, 1);
      /* Editing the preference is specific */
      f = &prefdatas[NPREFS * n + m];
      f->track = tracks[n];
      f->row = (NPREFS + 1) * n + 1 + m;
      f->p = &prefs[m];
      prefs[m].type->kickoff(f);
      if(ntracks > 1) {
        /* Propagation button */
        propagate = iconbutton("propagate.png", "Copy to other tracks");
        g_signal_connect(G_OBJECT(propagate), "clicked",
                         G_CALLBACK(propagate_clicked), f);
        gtk_table_attach(GTK_TABLE(properties_table),
                         propagate,
                         2/*left*/, 3/*right*/,
                         (NPREFS + 1) * n + 1 + m, (NPREFS + 1) * n + 2 + m,
                         GTK_FILL/*xoptions*/, 0/*yoptions*/,
                         1/*xpadding*/, 1/*ypadding*/);
      }
    }
  }
  prefs_unfilled = prefs_total;
  /* Buttons */
  buttonbox = create_buttons(buttons, NBUTTONS);
  /* Put it all together */
  vbox = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), 
                     scroll_widget(properties_table),
                     TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 1);
  gtk_container_add(GTK_CONTAINER(properties_window), frame_widget(vbox, NULL));
  /* The table only really wants to be vertically scrollable */
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(GTK_WIDGET(properties_table)->parent->parent),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  /* Zot any pre-existing progress window just in case */
  if(pw)
    progress_window_progress(pw, 0, 0);
  /* Pop up a progress bar while we're waiting */
  while(parent->parent)
    parent = parent->parent;
  pw = progress_window_new("Fetching Track Properties", parent);
}

/* Everything is filled in now */
static void prefdata_alldone(void) {
  if(pw) {
    progress_window_progress(pw, 0, 0);
    pw = 0;
  }
  /* Default size may be too small */
  gtk_window_set_default_size(GTK_WINDOW(properties_window), 480, 512);
  /* TODO: relate default size to required size more closely */
  gtk_widget_show_all(properties_window);
}

/* Namepart preferences ---------------------------------------------------- */

static void kickoff_namepart(struct prefdata *f) {
  /* We ask for the display name part.  This is a bit bizarre if what we really
   * wanted was the underlying preference, but in fact it should always match
   * and will supply a sane default without having to know how to parse tracks
   * names (which implies knowing collection roots). */
  disorder_eclient_part(client, prefdata_completed,
                        f->track, "display", f->p->part, f);
}

static void completed_namepart(struct prefdata *f) {
  if(!f->value) {
    /* No setting */
    f->value = "";
  }
  f->widget = gtk_entry_new();
}

static const char *get_edited_namepart(struct prefdata *f) {
  return gtk_entry_get_text(GTK_ENTRY(f->widget));
}

static void set_edited_namepart(struct prefdata *f, const char *value) {
  gtk_entry_set_text(GTK_ENTRY(f->widget), value);
}

static void set_namepart(struct prefdata *f, const char *value) {
  char *s;

  byte_xasprintf(&s, "trackname_display_%s", f->p->part);
  /* We don't know what the default is so can never unset.  This is a bug
   * relative to the original design, which is supposed to only ever allow for
   * non-trivial namepart preferences.  I suppose the server could spot a
   * default being set and translate it into an unset. */
  disorder_eclient_set(client, set_namepart_completed, f->track, s, value,
                       f);
}

/* Called when we've set a namepart */
static void set_namepart_completed(void *v, const char *err) {
  if(err)
    popup_protocol_error(0, err);
  else {
    struct prefdata *f = v;
    
    namepart_update(f->track, "display", f->p->part);
  }
}

/* String preferences ------------------------------------------------------ */

static void kickoff_string(struct prefdata *f) {
  disorder_eclient_get(client, prefdata_completed, f->track, f->p->part, f);
}

static void completed_string(struct prefdata *f) {
  if(!f->value)
    /* No setting, use the default value instead */
    f->value = f->p->default_value;
  f->widget = gtk_entry_new();
}

static const char *get_edited_string(struct prefdata *f) {
  return gtk_entry_get_text(GTK_ENTRY(f->widget));
}

static void set_edited_string(struct prefdata *f, const char *value) {
  gtk_entry_set_text(GTK_ENTRY(f->widget), value);
}

static void set_string_completed(void attribute((unused)) *v,
                                 const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

static void set_string(struct prefdata *f, const char *value) {
  disorder_eclient_set(client, set_string_completed, f->track, f->p->part,
                       value, 0/*v*/);
}

/* Boolean preferences ----------------------------------------------------- */

static void kickoff_boolean(struct prefdata *f) {
  disorder_eclient_get(client, prefdata_completed, f->track, f->p->part, f);
}

static void completed_boolean(struct prefdata *f) {
  f->widget = gtk_check_button_new();
  gtk_widget_set_style(f->widget, tool_style);
  if(!f->value)
    /* Not set, use the default */
    f->value = f->p->default_value;
}

static const char *get_edited_boolean(struct prefdata *f) {
  return (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(f->widget))
          ? "1" : "0");
}

static void set_edited_boolean(struct prefdata *f, const char *value) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(f->widget),
                               strcmp(value, "0"));
}

#define set_boolean_completed set_string_completed

static void set_boolean(struct prefdata *f, const char *value) {
  char *s;

  byte_xasprintf(&s, "trackname_display_%s", f->p->part);
  disorder_eclient_set(client, set_boolean_completed,
                       f->track, f->p->part, value, 0/*v*/);
}

/* Querying preferences ---------------------------------------------------- */

static void prefdata_completed(void *v, const char *err, const char *value) {
  struct prefdata *const f = v;

  if(err)
    popup_protocol_error(0, err);
  f->value = value;
  f->p->type->completed(f);
  f->p->type->set_edited(f, f->value);
  assert(f->value != 0);                /* Had better set a default */
  gtk_table_attach(GTK_TABLE(properties_table), f->widget,
                   1, 2,
                   f->row, f->row + 1,
                   GTK_EXPAND|GTK_FILL/*xoptions*/, 0/*yoptions*/,
                   1, 1);
  --prefs_unfilled;
  if(prefs_total)
    progress_window_progress(pw, prefs_total - prefs_unfilled, prefs_total);
  if(!prefs_unfilled)
    prefdata_alldone();
}

/* Button callbacks -------------------------------------------------------- */

static void properties_ok(GtkButton *button, 
                          gpointer userdata) {
  properties_apply(button, userdata);
  properties_cancel(button, userdata);
}

static void properties_apply(GtkButton attribute((unused)) *button, 
                             gpointer attribute((unused)) userdata) {
  int n;
  const char *edited;
  struct prefdata *f;

  /* For each possible property we see if we've changed it and if so tell the
   * server */
  for(n = 0; n < prefs_total; ++n) {
    f = &prefdatas[n];
    edited = f->p->type->get_edited(f);
    if(strcmp(edited, f->value)) {
      /* The value has changed */
      f->p->type->set(f, edited);
      f->value = xstrdup(edited);
    }
  }
}

static void properties_cancel(GtkButton attribute((unused)) *button,
                              gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(properties_window);
  event_cancel(properties_event);
  properties_event = 0;
}

static void properties_help(GtkButton attribute((unused)) *button,
                            gpointer attribute((unused)) userdata) {
  popup_help("properties.html");
}

/** @brief Called when we've just logged in
 *
 * Destroys the current properties window.
 */
static void properties_logged_in(const char attribute((unused)) *event,
                                 void attribute((unused)) *eventdata,
                                 void attribute((unused)) *callbackdata) {
  if(properties_window)
    gtk_widget_destroy(properties_window);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
