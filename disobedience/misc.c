/*
 * This file is part of DisOrder
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

#include "disobedience.h"

/* Miscellaneous GTK+ stuff ------------------------------------------------ */

WT(cached_image);

/* Functions */

GtkWidget *scroll_widget(GtkWidget *child,
                         const char *widgetname) {
  GtkWidget *scroller = gtk_scrolled_window_new(0, 0);
  GtkAdjustment *adj;

  D(("scroll_widget"));
  /* Why isn't _AUTOMATIC the default? */
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  if(GTK_IS_LAYOUT(child)) {
    /* Child widget has native scroll support */
    gtk_container_add(GTK_CONTAINER(scroller), child);
    /* Fix up the step increments if they are 0 (seems like an odd default?) */
    if(GTK_IS_LAYOUT(child)) {
      adj = gtk_layout_get_hadjustment(GTK_LAYOUT(child));
      if(!adj->step_increment) adj->step_increment = 16;
      adj = gtk_layout_get_vadjustment(GTK_LAYOUT(child));
      if(!adj->step_increment) adj->step_increment = 16;
    }
  } else {
    /* Child widget requires a viewport */
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroller),
                                          child);
  }
  /* Apply a name to the widget so it can be recolored */
  gtk_widget_set_name(GTK_BIN(scroller)->child, widgetname);
  gtk_widget_set_name(scroller, widgetname);
  return scroller;
}

GdkPixbuf *find_image(const char *name) {
  static const struct cache_type image_cache_type = { INT_MAX };

  GdkPixbuf *pb;
  char *path;
  GError *err = 0;

  if(!(pb = (GdkPixbuf *)cache_get(&image_cache_type, name))) {
    byte_xasprintf(&path, "%s/static/%s", pkgdatadir, name);
    if(!(pb = gdk_pixbuf_new_from_file(path, &err))) {
      error(0, "%s", err->message);
      return 0;
    }
    NW(cached_image);
    cache_put(&image_cache_type, name,  pb);
  }
  return pb;
}

void popup_error(const char *msg) {
  GtkWidget *w;

  w = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                             GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                             GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE,
                             "%s", msg);
  gtk_dialog_run(GTK_DIALOG(w));
  gtk_widget_destroy(w);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
