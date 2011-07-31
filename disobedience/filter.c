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
/** @file disobedience/filter.c
 * @brief Track filtering
 */

#include "disobedience.h"

static GtkWidget *filtering_window;
static void filter_close(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata);

static struct filter_row {
  const char *label;
  const char *pref;
  GtkWidget *entry;
} filter_rows[] = {
  { "Required tags", "required-tags", NULL },
  { "Prohibited tags", "prohibited-tags", NULL },
};
#define NFILTER (sizeof filter_rows / sizeof *filter_rows)

/** @brief Buttons for filtering popup */
static struct button filter_buttons[] = {
  {
    .stock = GTK_STOCK_CLOSE,
    .clicked = filter_close,
    .tip = "Close window",
    .pack = gtk_box_pack_end,
  },
};
#define NFILTER_BUTTONS (sizeof filter_buttons / sizeof *filter_buttons)

static void filter_close(GtkButton attribute((unused)) *button,
			 gpointer attribute((unused)) userdata) {
  gtk_widget_destroy(filtering_window);
}

/** @brief Called with the latest setting for a row */
static void filter_get_completed(void *v, const char *err,
				 const char *value) {
  if(err)
    popup_protocol_error(0, err);
  else if(filtering_window) {
    struct filter_row *row = v;
    /* Identify unset and empty lists */
    if(!value)
      value = "";
    /* Skip trivial updates (we'll see one as a consequence of each
     * update we make...) */
    if(strcmp(gtk_entry_get_text(GTK_ENTRY(row->entry)), value))
      gtk_entry_set_text(GTK_ENTRY(row->entry), value);
  }
}

/** @brief Retrieve the latest setting for @p row */
static void filter_get(struct filter_row *row) {
  disorder_eclient_get_global(client, filter_get_completed, row->pref, row);
}

/** @brief Called when the user changes the contents of some entry */
static void filter_entry_changed(GtkEditable *editable, gpointer user_data) {
  struct filter_row *row = user_data;
  const char *new_value = gtk_entry_get_text(GTK_ENTRY(editable));
  if(*new_value)
    disorder_eclient_set_global(client, NULL, row->pref, new_value, row);
  else
    disorder_eclient_unset_global(client, NULL, row->pref, row);
}

/** @brief Display the filtering window */
void popup_filtering(void) {
  GtkWidget *label, *table, *hbox;
  /* Pop up the window if it already exists */
  if(filtering_window) {
    gtk_window_present(GTK_WINDOW(filtering_window));
    return;
  }
  /* Create the window */
  /* TODO loads of this is very similar to (copied from!) users.c - can we
   * de-dupe? */
  filtering_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(filtering_window, tool_style);
  gtk_window_set_title(GTK_WINDOW(filtering_window), "Filtering");
  g_signal_connect(filtering_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &filtering_window);
  table = gtk_table_new(NFILTER + 1/*rows*/, 2/*cols*/, FALSE/*homogeneous*/);
  gtk_widget_set_style(table, tool_style);\

  for(size_t n = 0; n < NFILTER; ++n) {
    label = gtk_label_new(filter_rows[n].label);
    gtk_widget_set_style(label, tool_style);
    gtk_misc_set_alignment(GTK_MISC(label), 1/*right*/, 0/*bottom*/);
    gtk_table_attach(GTK_TABLE(table), label,
		     0, 1,                /* left/right_attach */
		     n, n+1,		  /* top/bottom_attach */
		     GTK_FILL, 0,         /* x/yoptions */
		     1, 1);               /* x/ypadding */
    filter_rows[n].entry = gtk_entry_new();
    gtk_widget_set_style(filter_rows[n].entry, tool_style);
    gtk_table_attach(GTK_TABLE(table), filter_rows[n].entry,
		     1, 2,                /* left/right_attach */
		     n, n+1,		  /* top/bottom_attach */
		     GTK_FILL, 0,         /* x/yoptions */
		     1, 1);               /* x/ypadding */
    g_signal_connect(filter_rows[n].entry, "changed",
		     G_CALLBACK(filter_entry_changed), &filter_rows[n]);
    filter_get(&filter_rows[n]);
  }
  hbox = create_buttons_box(filter_buttons,
			    NFILTER_BUTTONS,
			    gtk_hbox_new(FALSE, 1));
  gtk_table_attach_defaults(GTK_TABLE(table), hbox,
			    0, 2,                /* left/right_attach */
			    NFILTER, NFILTER+1); /* top/bottom_attach */

  gtk_container_add(GTK_CONTAINER(filtering_window), frame_widget(table, NULL));
  gtk_widget_show_all(filtering_window);
}

/** @brief Called when any global pref changes */
static void filtering_global_pref_changed(const char *event,
					  void *eventdata,
					  void *callbackdata) {
  const char *pref = eventdata;
  if(!filtering_window)
    return;			/* not paying attention */
  for(size_t n = 0; n < NFILTER; ++n) {
    if(!strcmp(pref, filter_rows[n].pref))
      filter_get(&filter_rows[n]);
  }
}

/** @brief Initialize filtering infrastructure */
void filtering_init() {
  event_register("global-pref", filtering_global_pref_changed, NULL);
}
