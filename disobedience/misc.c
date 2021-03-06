/*
 * This file is part of DisOrder
 * Copyright (C) 2006-2008, 2010 Richard Kettlewell
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
/** @file disobedience/misc.c
 * @brief Miscellaneous GTK+ interfacing stuff
 */

#include "disobedience.h"
#include "table.h"

/** @brief Embedded image */
struct image {
  /** @brief Image name */
  const char *name;
  /** @brief Image data in GDK pixbuf inline format */
  const guint8 *data;
};

#include "../images/images.h"

/* Miscellaneous GTK+ stuff ------------------------------------------------ */

/* Functions */

/** @brief Put scrollbars around a widget
 * @param child Widget to surround
 * @return Scroll widget
 */
GtkWidget *scroll_widget(GtkWidget *child) {
  GtkWidget *scroller = gtk_scrolled_window_new(0, 0);
  GtkAdjustment *adj;

  gtk_widget_set_style(scroller, tool_style);
  D(("scroll_widget"));
  /* Why isn't _AUTOMATIC the default? */
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  if(GTK_IS_LAYOUT(child)
     || GTK_IS_TREE_VIEW(child)) {
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
    gtk_widget_set_style(gtk_bin_get_child(GTK_BIN(scroller)), tool_style);
  }
  gtk_widget_set_style(GTK_SCROLLED_WINDOW(scroller)->hscrollbar, tool_style);
  gtk_widget_set_style(GTK_SCROLLED_WINDOW(scroller)->vscrollbar, tool_style);
  return scroller;
}

/** @brief Put a frame round a widget
 * @param w Widget
 * @param label Label or NULL
 * @return Frame widget
 */
GtkWidget *frame_widget(GtkWidget *w, const char *label) {
  GtkWidget *const frame = gtk_frame_new(label);
  GtkWidget *const hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *const vbox = gtk_vbox_new(FALSE, 0);
  /* We want 4 pixels outside the frame boundary... */
  gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
  /* ...and 4 pixels inside */
  gtk_box_pack_start(GTK_BOX(hbox), w, TRUE/*expand*/, TRUE/*fill*/, 4);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE/*expand*/, TRUE/*fill*/, 4);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  return frame;
}

/** @brief Find an image
 * @param name Relative path to image
 * @return pixbuf containing image
 *
 * Images are cached so it's perfectly sensible to call this lots of times even
 * for the same image.
 *
 * Images are searched for in @c pkgdatadir/static.
 */
GdkPixbuf *find_image(const char *name) {
  static const struct cache_type image_cache_type = { INT_MAX };

  GdkPixbuf *pb;
  char *path;
  GError *err = 0;
  int n;

  if(!(pb = (GdkPixbuf *)cache_get(&image_cache_type, name))) {
    if((n = TABLE_FIND(images, name, name)) >= 0) {
      /* Use the built-in copy */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      if(!(pb = gdk_pixbuf_new_from_inline(-1, images[n].data, FALSE, &err))) {
#pragma GCC diagnostic pop
        disorder_error(0, "%s", err->message);
        return 0;
      }
    } else {
      /* See if there's a copy on disk */
      byte_xasprintf(&path, "%s/static/%s", pkgdatadir, name);
      if(!(pb = gdk_pixbuf_new_from_file(path, &err))) {
        disorder_error(0, "%s", err->message);
        return 0;
      }
    }
    cache_put(&image_cache_type, name,  pb);
  }
  return pb;
}

/** @brief Pop up a message */
void popup_msg(GtkMessageType mt, const char *msg) {
  popup_submsg(toplevel, mt, msg);
}

void popup_submsg(GtkWidget *parent, GtkMessageType mt, const char *msg) {
  GtkWidget *w;

  w = gtk_message_dialog_new(GTK_WINDOW(parent),
                             GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                             mt,
                             GTK_BUTTONS_CLOSE,
                             "%s", msg);
  gtk_widget_set_style(w, tool_style);
  gtk_dialog_run(GTK_DIALOG(w));
  gtk_widget_destroy(w);
}

/** @brief Pop up an error message */
void fpopup_msg(GtkMessageType mt, const char *fmt, ...) {
  va_list ap;
  char *msg;

  va_start(ap, fmt);
  byte_xvasprintf(&msg, fmt, ap);
  va_end(ap);
  popup_msg(mt, msg);
}

/** @brief Create a button with an icon in it
 * @param path (relative) path to image
 * @param tip Tooltip or NULL to not set one
 * @return Button
 */
GtkWidget *iconbutton(const char *path, const char *tip) {
  GtkWidget *button, *content;
  GdkPixbuf *pb;

  button = gtk_button_new();
  if((pb = find_image(path))) {
    content = gtk_image_new_from_pixbuf(pb);
  } else {
    content = gtk_label_new(path);
  }
  gtk_widget_set_style(button, tool_style);
  gtk_widget_set_style(content, tool_style);
  gtk_container_add(GTK_CONTAINER(button), content);
  if(tip)
    gtk_widget_set_tooltip_text(button, tip);
  return button;
}

/** @brief Create buttons and pack them into a box, which is returned */
GtkWidget *create_buttons_box(struct button *buttons,
                              size_t nbuttons,
                              GtkWidget *box) {
  size_t n;

  for(n = 0; n < nbuttons; ++n) {
    buttons[n].widget = gtk_button_new_from_stock(buttons[n].stock);
    gtk_widget_set_style(buttons[n].widget, tool_style);
    g_signal_connect(G_OBJECT(buttons[n].widget), "clicked",
                     G_CALLBACK(buttons[n].clicked), 0);
    void (*pack)(GtkBox *box,
                 GtkWidget *child,
                 gboolean expand,
                 gboolean fill,
                 guint padding);
    if(!(pack = buttons[n].pack))
      pack = gtk_box_pack_start;
    pack(GTK_BOX(box), buttons[n].widget, FALSE, FALSE, 1);
    gtk_widget_set_tooltip_text(buttons[n].widget, buttons[n].tip);
  }
  return box;
}

/** @brief Create buttons and pack them into an hbox */
GtkWidget *create_buttons(struct button *buttons,
                          size_t nbuttons) {
  return create_buttons_box(buttons, nbuttons,
                            gtk_hbox_new(FALSE, 1));
}



/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
