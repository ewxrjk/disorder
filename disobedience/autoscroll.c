/* Derived from gtktreeview.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 * Portions copyright (C) 2009 Richard Kettlewell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/** @file disobedience/autoscroll.c
 * @brief Automatic scrolling of a GtkTreeView
 *
 * GTK+ doesn't expose the automatic scrolling support if you don't use its
 * high-level treeview drag+drop features.
 *
 * Adapted from GTK+ upstream as of commit
 * 7fda8e6378d90bc8cf670ffe9dea682911e5e241.
 */

#include <config.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "autoscroll.h"

/** @brief Object data key used to track the autoscroll timeout */
#define AUTOSCROLL_KEY "autoscroll.greenend.org.uk"

/** @brief Controls size of edge region that provokes scrolling
 *
 * Actually this is half the size of the scroll region.  In isolation this may
 * seem bizarre, but GTK+ uses the value internally for other purposes.
 */
#define SCROLL_EDGE_SIZE 15

/** @brief Called from time to time to check whether auto-scrolling is needed
 * @param data The GtkTreeView
 * @return TRUE, to keep on truckin'
 */
static gboolean autoscroll_timeout(gpointer data) {
  GtkTreeView *tree_view = data;
  GdkRectangle visible_rect;
  gint wx, wy, tx, ty;
  gint offset;
  gfloat value;

  /* First we must find the pointer Y position in tree coordinates.  GTK+
   * natively knows what the bin window is and can get the pointer in bin
   * coords and convert to tree coords.  But there is no published way for us
   * to find the bin window, so we must start in widget coords. */
  gtk_widget_get_pointer(GTK_WIDGET(tree_view), &wx, &wy);
  //fprintf(stderr, "widget coords: %d, %d\n", wx, wy);
  gtk_tree_view_convert_widget_to_tree_coords(tree_view, wx, wy, &tx, &ty);
  //fprintf(stderr, "tree coords: %d, %d\n", tx, ty);

  gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

  /* see if we are near the edge. */
  offset = ty - (visible_rect.y + 2 * SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = ty - (visible_rect.y + visible_rect.height - 2 * SCROLL_EDGE_SIZE);
      if (offset < 0)
	return TRUE;
    }

  GtkAdjustment *vadjustment = gtk_tree_view_get_vadjustment(tree_view);

  value = CLAMP (vadjustment->value + offset, 0.0,
		 vadjustment->upper - vadjustment->page_size);
  gtk_adjustment_set_value (vadjustment, value);

  return TRUE;
}

/** @brief Enable autoscrolling
 * @param tree_view Tree view to enable autoscrolling
 *
 * It's harmless to call this if autoscrolling is already enabled.
 *
 * It's up to you to cancel the callback when no longer required (including
 * object destruction).
 */
void autoscroll_add(GtkTreeView *tree_view) {
  guint *scrolldata = g_object_get_data(G_OBJECT(tree_view), AUTOSCROLL_KEY);
  if(!scrolldata) {
    /* Set up the callback */
    scrolldata = g_new(guint, 1);
    *scrolldata = gdk_threads_add_timeout(150, autoscroll_timeout, tree_view);
    g_object_set_data(G_OBJECT(tree_view), AUTOSCROLL_KEY, scrolldata);
    //fprintf(stderr, "autoscroll enabled\n");
  }
}

/** @brief Disable autoscrolling
 * @param tree_view Tree view to enable autoscrolling
 *
 * It's harmless to call this if autoscrolling is not enabled.
 */
void autoscroll_remove(GtkTreeView *tree_view) {
  guint *scrolldata = g_object_get_data(G_OBJECT(tree_view), AUTOSCROLL_KEY);
  if(scrolldata) {
    g_object_set_data(G_OBJECT(tree_view), AUTOSCROLL_KEY, NULL);
    g_source_remove(*scrolldata);
    g_free(scrolldata);
    //fprintf(stderr, "autoscroll disabled\n");
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
