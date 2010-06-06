/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file disobedience/progress.c
 * @brief Progress bar support
 */

#include "disobedience.h"

/** @brief State for progress windows */
struct progress_window {
  /** @brief The window */
  GtkWidget *window;
  /** @brief The bar */
  GtkWidget *bar;
};

/** @brief Create a progress window */
struct progress_window *progress_window_new(const char *title) {
  struct progress_window *pw = xmalloc(sizeof *pw);

  pw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_transient_for(GTK_WINDOW(pw->window),
                               GTK_WINDOW(toplevel));
  g_signal_connect(pw->window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &pw->window);
  gtk_window_set_default_size(GTK_WINDOW(pw->window), 360, -1);
  gtk_window_set_title(GTK_WINDOW(pw->window), title);
  pw->bar = gtk_progress_bar_new();
  gtk_container_add(GTK_CONTAINER(pw->window), pw->bar);
  gtk_widget_show_all(pw->window);
  return pw;
}

/** @brief Report current progress
 * The window is automatically destroyed if @p progress >= @p limit.
 * To cancel a window just call with both set to 0.
 */
void progress_window_progress(struct progress_window *pw,
			      int progress,
			      int limit) {
  if(!pw)
    return;
  /* Maybe the user closed the window */
  if(!pw->window)
    return;
  /* Clamp insane or inconvenient values */
  if(limit <= 0)
    progress = limit = 1;
  if(progress < 0)
    progress = 0;
  /* Maybe we're done */
  if(progress >= limit) {
    gtk_widget_destroy(pw->window);
    pw->window = pw->bar = 0;
    return;
  }
  /* Display current progress */
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pw->bar),
                                (double)progress / limit);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
