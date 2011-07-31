/*
 * This file is part of DisOrder.
 * Copyright (C) 2011 Richard Kettlewell
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
/** @file disobedience/globals.c
 * @brief Track global preferences
 */

#include "disobedience.h"

static GtkWidget *globals_window;

struct globals_row;

/** @brief Handler for the presentation form of a global preference */
struct global_handler {
  /** @brief Initialize */
  void (*init)(struct globals_row *row);

  /** @brief Convert presentation form to string */
  const char *(*get)(struct globals_row *row);

  /** @brief Convert string to presentation form */
  void (*set)(struct globals_row *row, const char *value);
};

/** @brief Definition of a global preference */
struct globals_row {
  const char *label;
  const char *pref;
  GtkWidget *widget;
  const struct global_handler *handler;
  int initialized;
};

static void globals_close(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata);
static void globals_row_changed(struct globals_row *row);

/** @brief Called when the user changes the contents of a string entry */
static void global_string_entry_changed(GtkEditable attribute((unused)) *editable,
					gpointer user_data) {
  struct globals_row *row = user_data;
  globals_row_changed(row);
}

/** @brief Initialize a global presented as a string */
static void global_string_init(struct globals_row *row) {
  row->widget = gtk_entry_new();
  g_signal_connect(row->widget, "changed",
		   G_CALLBACK(global_string_entry_changed), row);
}

static const char *global_string_get(struct globals_row *row) {
  return gtk_entry_get_text(GTK_ENTRY(row->widget));
}

static void global_string_set(struct globals_row *row, const char *value) {
  /* Identify unset and empty lists */
  if(!value)
    value = "";
  /* Skip trivial updates (we'll see one as a consequence of each
   * update we make...) */
  if(strcmp(gtk_entry_get_text(GTK_ENTRY(row->widget)), value))
    gtk_entry_set_text(GTK_ENTRY(row->widget), value);
}

/** @brief String global preference */
static const struct global_handler global_string = {
  global_string_init,
  global_string_get,
  global_string_set,
};

/** @brief Called when the user changes the contents of a string entry */
static void global_boolean_toggled(GtkToggleButton attribute((unused)) *button,
				   gpointer user_data) {
  struct globals_row *row = user_data;
  globals_row_changed(row);
}

static void global_boolean_init(struct globals_row *row) {
  row->widget = gtk_check_button_new();
  g_signal_connect(row->widget, "toggled",
		   G_CALLBACK(global_boolean_toggled), row);
}

static const char *global_boolean_get(struct globals_row *row) {
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(row->widget)) ? "yes" : "no";
}

static void global_boolean_set(struct globals_row *row, const char *value) {
  gboolean new_state = !(value && strcmp(value, "yes"));
  if(new_state != gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(row->widget)))
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(row->widget), new_state);
}

/** @brief Boolean global preference */
static const struct global_handler global_boolean = {
  global_boolean_init,
  global_boolean_get,
  global_boolean_set,
};

/** @brief Table of global preferences */
static struct globals_row globals_rows[] = {
  { "Required tags", "required-tags", NULL, &global_string, 0 },
  { "Prohibited tags", "prohibited-tags", NULL, &global_string, 0 },
  { "Playing", "playing", NULL, &global_boolean, 0 },
  { "Random play", "random-play", NULL, &global_boolean, 0 },
};
#define NGLOBALS (sizeof globals_rows / sizeof *globals_rows)

/** @brief Buttons for globals popup */
static struct button globals_buttons[] = {
  {
    .stock = GTK_STOCK_CLOSE,
    .clicked = globals_close,
    .tip = "Close window",
    .pack = gtk_box_pack_end,
  },
};
#define NGLOBALS_BUTTONS (sizeof globals_buttons / sizeof *globals_buttons)

static void globals_close(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(globals_window);
}

/** @brief Called with the latest setting for a row */
static void globals_get_completed(void *v, const char *err,
				 const char *value) {
  if(err)
    popup_protocol_error(0, err);
  else if(globals_window) {
    struct globals_row *row = v;
    row->handler->set(row, value);
    row->initialized = 1;
  }
}

/** @brief Retrieve the latest setting for @p row */
static void globals_get(struct globals_row *row) {
  disorder_eclient_get_global(client, globals_get_completed, row->pref, row);
}

static void globals_row_changed(struct globals_row *row) {
  if(row->initialized) {
    const char *new_value = row->handler->get(row);
    if(new_value)
      disorder_eclient_set_global(client, NULL, row->pref, new_value, row);
    else
      disorder_eclient_unset_global(client, NULL, row->pref, row);
  }
}

/** @brief Display the globals window */
void popup_globals(void) {
  GtkWidget *label, *table, *hbox;
  /* Pop up the window if it already exists */
  if(globals_window) {
    gtk_window_present(GTK_WINDOW(globals_window));
    return;
  }
  /* Create the window */
  /* TODO loads of this is very similar to (copied from!) users.c - can we
   * de-dupe? */
  globals_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(globals_window, tool_style);
  gtk_window_set_title(GTK_WINDOW(globals_window), "Globals");
  g_signal_connect(globals_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &globals_window);
  table = gtk_table_new(NGLOBALS + 1/*rows*/, 2/*cols*/, FALSE/*homogeneous*/);
  gtk_widget_set_style(table, tool_style);\

  for(size_t n = 0; n < NGLOBALS; ++n) {
    globals_rows[n].initialized = 0;
    label = gtk_label_new(globals_rows[n].label);
    gtk_widget_set_style(label, tool_style);
    gtk_misc_set_alignment(GTK_MISC(label), 1/*right*/, 0/*bottom*/);
    gtk_table_attach(GTK_TABLE(table), label,
		     0, 1,                /* left/right_attach */
		     n, n+1,		  /* top/bottom_attach */
		     GTK_FILL, 0,         /* x/yoptions */
		     1, 1);               /* x/ypadding */
    globals_rows[n].handler->init(&globals_rows[n]);
    gtk_widget_set_style(globals_rows[n].widget, tool_style);
    gtk_table_attach(GTK_TABLE(table), globals_rows[n].widget,
		     1, 2,                /* left/right_attach */
		     n, n+1,		  /* top/bottom_attach */
		     GTK_FILL, 0,         /* x/yoptions */
		     1, 1);               /* x/ypadding */
    globals_get(&globals_rows[n]);
  }
  hbox = create_buttons_box(globals_buttons,
			    NGLOBALS_BUTTONS,
			    gtk_hbox_new(FALSE, 1));
  gtk_table_attach_defaults(GTK_TABLE(table), hbox,
			    0, 2,                /* left/right_attach */
			    NGLOBALS, NGLOBALS+1); /* top/bottom_attach */

  gtk_container_add(GTK_CONTAINER(globals_window), frame_widget(table, NULL));
  gtk_widget_show_all(globals_window);
}

/** @brief Called when any global pref changes */
static void globals_pref_changed(const char *event,
				 void *eventdata,
				 void *callbackdata) {
  const char *pref = eventdata;
  if(!globals_window)
    return;			/* not paying attention */
  for(size_t n = 0; n < NGLOBALS; ++n) {
    if(!strcmp(pref, globals_rows[n].pref))
      globals_get(&globals_rows[n]);
  }
}

/** @brief Initialize globals infrastructure */
void globals_init() {
  event_register("global-pref", globals_pref_changed, NULL);
}
