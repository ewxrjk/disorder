/*
 * This file is part of DisOrder
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file disobedience/multidrag.c
 * @brief Drag multiple rows of a GtkTreeView
 *
 * Normally when you start a drag, GtkTreeView sets the selection to just row
 * you dragged from (because it can't cope with dragging more than one row at a
 * time).
 *
 * Disobedience needs more.
 *
 * Firstly it intercepts button-press-event and button-release event and for
 * clicks that might be the start of drags, suppresses changes to the
 * selection.  A consequence of this is that it needs to intercept
 * button-release-event too, to restore the effect of the click, if it turns
 * out not to be drag after all.
 *
 * The location of the initial click is stored in object data called @c
 * multidrag-where.
 *
 * Secondly it intercepts drag-begin and constructs an icon from the rows to be
 * dragged.
 *
 * Inspired by similar code in <a
 * href="http://code.google.com/p/quodlibet/">Quodlibet</a> (another software
 * jukebox, albeit as far as I can see a single-user one).
 */
#include <config.h>

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "multidrag.h"

static gboolean multidrag_selection_block(GtkTreeSelection attribute((unused)) *selection,
					  GtkTreeModel attribute((unused)) *model,
					  GtkTreePath attribute((unused)) *path,
					  gboolean attribute((unused)) path_currently_selected,
					  gpointer data) {
  return *(const gboolean *)data;
}

static void block_selection(GtkWidget *w, gboolean block,
			    int x, int y) {
  static const gboolean which[] = { FALSE, TRUE };
  GtkTreeSelection *s = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
  gtk_tree_selection_set_select_function(s,
					 multidrag_selection_block,
					 (gboolean *)&which[!!block],
					 NULL);
  // Remember the pointer location
  int *where = g_object_get_data(G_OBJECT(w), "multidrag-where");
  if(!where) {
    where = g_malloc(2 * sizeof (int));
    g_object_set_data(G_OBJECT(w), "multidrag-where", where);
  }
  where[0] = x;
  where[1] = y;
  // TODO release 'where' when object is destroyed
}

static gboolean multidrag_button_press_event(GtkWidget *w,
					     GdkEventButton *event,
					     gpointer attribute((unused)) user_data) {
  /* By default we assume that anything this button press does should
   * act as normal */
  block_selection(w, TRUE, -1, -1);
  /* We are only interested in left-button behavior */
  if(event->button != 1)
    return FALSE;
  /* We are only interested in unmodified clicks (not SHIFT etc) */
  if(event->state & GDK_MODIFIER_MASK)
    return FALSE;
  /* We are only interested if a well-defined path is clicked */
  GtkTreePath *path;
  if(!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(w),
				    event->x, event->y,
				    &path,
				    NULL,
				    NULL, NULL))
    return FALSE;
  //gtk_widget_grab_focus(w);    // TODO why??
  /* We are only interested if a selected row is clicked */
  GtkTreeSelection *s = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
  if(!gtk_tree_selection_path_is_selected(s, path))
    return FALSE;
  /* We block subsequent selection changes and remember where the
   * click was */
  block_selection(w, FALSE, event->x, event->y);
  return FALSE;			/* propagate */
}

static gboolean multidrag_button_release_event(GtkWidget *w,
					       GdkEventButton *event,
					       gpointer attribute((unused)) user_data) {
  int *where = g_object_get_data(G_OBJECT(w), "multidrag-where");

  /* Did button-press-event do anything?  We just check the outcome rather than
   * going through all the conditions it tests. */
  if(where && where[0] != -1) {
    // Remember where the down-click was
    const int x = where[0], y = where[1];
    // Re-allow selections
    block_selection(w, TRUE, -1, -1);
    if(x == event->x && y == event->y) {
      // If the up-click is at the same location as the down-click,
      // it's not a drag.
      GtkTreePath *path;
      GtkTreeViewColumn *col;
      if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(w),
				       event->x, event->y,
				       &path,
				       &col,
				       NULL, NULL)) {
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(w), path, col, FALSE);
      }
    }
  }
  return FALSE;			/* propagate */
}

/** @brief State for multidrag_begin() and its callbacks */
struct multidrag_begin_state {
  GtkTreeView *view;
  int rows;
  int index;
  GdkPixmap **pixmaps;
};

/** @brief Callback to construct a row pixmap */
static void multidrag_make_row_pixmaps(GtkTreeModel attribute((unused)) *model,
				       GtkTreePath *path,
				       GtkTreeIter attribute((unused)) *iter,
				       gpointer data) {
  struct multidrag_begin_state *qdbs = data;

  qdbs->pixmaps[qdbs->index++]
    = gtk_tree_view_create_row_drag_icon(qdbs->view, path);
}

/** @brief Called when a drag operation starts
 * @param w Source widget (the tree view)
 * @param dc Drag context
 * @param user_data Not used
 */
static void multidrag_drag_begin(GtkWidget *w,
				 GdkDragContext attribute((unused)) *dc,
				 gpointer attribute((unused)) user_data) {
  struct multidrag_begin_state qdbs[1];
  GdkPixmap *icon;
  GtkTreeSelection *sel;

  //fprintf(stderr, "drag-begin\n");
  memset(qdbs, 0, sizeof *qdbs);
  qdbs->view = GTK_TREE_VIEW(w);
  sel = gtk_tree_view_get_selection(qdbs->view);
  /* Find out how many rows there are */
  if(!(qdbs->rows = gtk_tree_selection_count_selected_rows(sel)))
    return;                             /* doesn't make sense */
  /* Generate a pixmap for each row */
  qdbs->pixmaps = g_new(GdkPixmap *, qdbs->rows);
  gtk_tree_selection_selected_foreach(sel,
                                      multidrag_make_row_pixmaps,
                                      qdbs);
  /* Determine the size of the final icon */
  int height = 0, width = 0;
  for(int n = 0; n < qdbs->rows; ++n) {
    int pxw, pxh;
    gdk_drawable_get_size(qdbs->pixmaps[n], &pxw, &pxh);
    if(pxw > width)
      width = pxw;
    height += pxh;
  }
  if(!width || !height)
    return;                             /* doesn't make sense */
  /* Construct the icon */
  icon = gdk_pixmap_new(qdbs->pixmaps[0], width, height, -1);
  GdkGC *gc = gdk_gc_new(icon);
  gdk_gc_set_colormap(gc, gtk_widget_get_colormap(w));
  int y = 0;
  for(int n = 0; n < qdbs->rows; ++n) {
    int pxw, pxh;
    gdk_drawable_get_size(qdbs->pixmaps[n], &pxw, &pxh);
    gdk_draw_drawable(icon,
                      gc,
                      qdbs->pixmaps[n],
                      0, 0,             /* source coords */
                      0, y,             /* dest coords */
                      pxw, pxh);        /* size */
    y += pxh;
    gdk_drawable_unref(qdbs->pixmaps[n]);
    qdbs->pixmaps[n] = NULL;
  }
  g_free(qdbs->pixmaps);
  qdbs->pixmaps = NULL;
  // TODO scale down a bit, the resulting icons are currently a bit on the
  // large side.
  gtk_drag_source_set_icon(w,
                           gtk_widget_get_colormap(w),
                           icon,
                           NULL);
}

/** @brief Allow multi-row drag for @p w
 * @param w A GtkTreeView widget
 *
 * Suppresses the restriction of selections when a drag is started.
 */
void make_treeview_multidrag(GtkWidget *w) {
  g_signal_connect(w, "button-press-event",
		   G_CALLBACK(multidrag_button_press_event), NULL);
  g_signal_connect(w, "button-release-event",
		   G_CALLBACK(multidrag_button_release_event), NULL);
  g_signal_connect(w, "drag-begin",
                   G_CALLBACK(multidrag_drag_begin), NULL);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
