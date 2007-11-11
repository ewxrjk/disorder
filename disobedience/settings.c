/*
 * This file is part of Disobedience
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file disobedience/settings.c
 * @brief Disobedience settings
 *
 * Originally I attempted to use a built-in rc file to configure
 * Disobedience's colors.  This is quite convenient but fails in the
 * face of themes, as the theme settings override the application
 * ones.
 *
 * This file therefore collects all the colors of the Disobedience UI
 * and (in time) will have a configuration dialog too.
 */

#include "disobedience.h"
#include "inputline.h"
#include "split.h"
#include <sys/stat.h>

/** @brief HTML displayer */
const char *browser = BROWSER;

void save_settings(void) {
  char *dir, *path, *tmp;
  FILE *fp = 0;

  byte_xasprintf(&dir, "%s/.disorder", getenv("HOME"));
  byte_xasprintf(&path, "%s/disobedience", dir);
  byte_xasprintf(&tmp, "%s.tmp", path);
  mkdir(dir, 02700);                    /* make sure directory exists */
  if(!(fp = fopen(tmp, "w"))) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
               tmp, strerror(errno));
    goto done;
  }
  /* TODO */
  if(fclose(fp) < 0) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error writing to %s: %s",
               tmp, strerror(errno));
    fp = 0;
    goto done;
  }
  fp = 0;
  if(rename(tmp, path) < 0)
    fpopup_msg(GTK_MESSAGE_ERROR, "error renaming %s to %s: %s",
               tmp, path, strerror(errno));
done:
  if(fp)
    fclose(fp);
}

static inline unsigned clamp(unsigned n) {
  return n > 0xFFFF ? 0xFFFF : n;
}

void load_settings(void) {
  char *path, *line;
  FILE *fp;
  char **vec;
  int nvec;

  byte_xasprintf(&path, "%s/.disorder/disobedience", getenv("HOME"));
  if(!(fp = fopen(path, "r"))) {
    if(errno != ENOENT)
      fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
                 path, strerror(errno));
  } else {
    while(!inputline(path, fp, &line, '\n')) {
      if(!(vec = split(line, &nvec, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0))
         || !nvec)
        continue;
      if(!strcmp(vec[0], "color")) {
        if(nvec != 5) {
          error(0, "%s: malformed '%s' command", path, vec[0]);
          continue;
        }
        /* TODO */
      } else
        /* mention errors but otherwise ignore them */
        error(0, "%s: unknown command '%s'", path, vec[0]);
    }
    if(ferror(fp)) {
      fpopup_msg(GTK_MESSAGE_ERROR, "error reading %s: %s",
                 path, strerror(errno));
      fclose(fp);
    }
  }
}

/** @brief Callback used by set_tool_colors() */
static void set_tool_colors_callback(GtkWidget *w,
                                     gpointer attribute((unused)) data) {
  set_tool_colors(w);
}

/** @brief Recursively set tool widget colors */
void set_tool_colors(GtkWidget *w) {
  GtkWidget *child;

  gtk_widget_set_style(w, tool_style);
  if(GTK_IS_CONTAINER(w))
    gtk_container_foreach(GTK_CONTAINER(w), set_tool_colors_callback, 0);
  if(GTK_IS_MENU_ITEM(w)
     && (child = gtk_menu_item_get_submenu(GTK_MENU_ITEM(w))))
    set_tool_colors(child);
}

/** @brief Default style for layouts */
GtkStyle *layout_style;

/** @brief Title-row style for layouts */
GtkStyle *title_style;

/** @brief Even-row style for layouts */
GtkStyle *even_style;

/** @brief Odd-row style for layouts */
GtkStyle *odd_style;

/** @brief Active-row style for layouts */
GtkStyle *active_style;

/** @brief Style for tools */
GtkStyle *tool_style;

/** @brief Style for search results */
GtkStyle *search_style;

/** @brief Style for drag targets */
GtkStyle *drag_style;

/** @brief Initialize styles */
void init_styles(void) {
  layout_style = gtk_style_new();
  title_style = gtk_style_new();
  even_style = gtk_style_new();
  odd_style = gtk_style_new();
  active_style = gtk_style_new();
  search_style = gtk_style_new();
  tool_style = gtk_style_new();
  drag_style = gtk_style_new();

  /* Style defaults */
    
  /* Layouts are basically black on white */
  layout_style->bg[GTK_STATE_NORMAL] = layout_style->white;
  layout_style->fg[GTK_STATE_NORMAL] = layout_style->black;
    
  /* Title row is inverted */
  title_style->bg[GTK_STATE_NORMAL] = layout_style->fg[GTK_STATE_NORMAL];
  title_style->fg[GTK_STATE_NORMAL] = layout_style->bg[GTK_STATE_NORMAL];

  /* Active row is pastel green */
  active_style->bg[GTK_STATE_NORMAL].red = 0xE000;
  active_style->bg[GTK_STATE_NORMAL].green = 0xFFFF;
  active_style->bg[GTK_STATE_NORMAL].blue = 0xE000;
  active_style->fg[GTK_STATE_NORMAL] = layout_style->fg[GTK_STATE_NORMAL];

  /* Even rows are pastel red */
  even_style->bg[GTK_STATE_NORMAL].red = 0xFFFF;
  even_style->bg[GTK_STATE_NORMAL].green = 0xEC00;
  even_style->bg[GTK_STATE_NORMAL].blue = 0xEC00;
  even_style->fg[GTK_STATE_NORMAL] = layout_style->fg[GTK_STATE_NORMAL];

  /* Odd rows match the underlying layout */
  odd_style->bg[GTK_STATE_NORMAL] = layout_style->bg[GTK_STATE_NORMAL];
  odd_style->fg[GTK_STATE_NORMAL] = layout_style->fg[GTK_STATE_NORMAL];

  /* Search results have a yellow background */
  search_style->fg[GTK_STATE_NORMAL] = layout_style->fg[GTK_STATE_NORMAL];
  search_style->bg[GTK_STATE_NORMAL].red = 0xFFFF;
  search_style->bg[GTK_STATE_NORMAL].green = 0xFFFF;
  search_style->bg[GTK_STATE_NORMAL].blue = 0x0000;

  /* Drag targets are grey */
  drag_style->bg[GTK_STATE_NORMAL].red = 0x6666;
  drag_style->bg[GTK_STATE_NORMAL].green = 0x6666;
  drag_style->bg[GTK_STATE_NORMAL].blue = 0x6666;
  
  /* Tools we leave at GTK+ defaults */
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
