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
/** @file disobedience/appearance.c
 * @brief Visual appearance of Disobedience
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

/** @brief Background colors for tools - menus, icons, etc. */
GdkColor tool_bg = { 0, 0xDC00, 0xDA00, 0xD500 };

/** @brief Background color for active tool */
GdkColor tool_active;

/** @brief Foreground colors for tools */
GdkColor tool_fg = { 0, 0x0000, 0x0000, 0x0000 };

/** @brief Foreground colors for inactive tools */
GdkColor inactive_tool_fg = { 0, 0x8000, 0x8000, 0x8000 };

/** @brief Background color for the various layouts */
GdkColor layout_bg = { 0, 0xFFFF, 0xFFFF, 0xFFFF };

/** @brief Title-row background color */
GdkColor title_bg = { 0, 0x0000, 0x0000, 0x0000 };

/** @brief Title-row foreground color */
GdkColor title_fg = { 0, 0xFFFF, 0xFFFF, 0xFFFF };

/** @brief Even-row background color */
GdkColor even_bg = { 0, 0xFFFF, 0xEC00, 0xEBFF };

/** @brief Odd-row background color */
GdkColor odd_bg = { 0, 0xFFFF, 0xFFFF, 0xFFFF };

/** @brief Active-row background color */
GdkColor active_bg = { 0, 0xE000, 0xFFFF, 0xE000 };

/** @brief Item foreground color */
GdkColor item_fg = { 0, 0x0000, 0x0000, 0x0000 };

/** @brief Selected background color */
GdkColor selected_bg = { 0, 0x4B00, 0x6900, 0x8300 };

/** @brief Selected foreground color */
GdkColor selected_fg = { 0, 0xFFFF, 0xFFFF, 0xFFFF };

/** @brief Search results */
GdkColor search_bg = { 0, 0xFFFF, 0xFFFF, 0x0000 };

/** @brief Drag target color */
GdkColor drag_target = { 0, 0x6666, 0x6666, 0x6666 };

struct colordesc {
  GdkColor *color;
  const char *name;
  const char *description;
};

#define COLOR(name, description) { &name, #name, description }

/** @brief Table of configurable colors
 *
 * Some of the descriptions could be improve!
 */
static const struct colordesc colors[] = {
  COLOR(tool_bg, "Tool background color"),
  COLOR(tool_fg, "Tool foreground color"),
  COLOR(layout_bg, "Layout background color"),
  COLOR(title_bg, "Title row background color"),
  COLOR(title_fg, "Title row foreground color"),
  COLOR(even_bg, "Even row background color"),
  COLOR(odd_bg, "Odd row background color"),
  COLOR(active_bg, "Playing row background color"),
  COLOR(item_fg, "Track foreground color"),
  COLOR(selected_bg, "Selected item background color"),
  COLOR(selected_fg, "Selected item foreground color"),
  COLOR(search_bg, "Search result background color"),
  COLOR(drag_target, "Drag target color"),
};

#define NCOLORS (sizeof colors / sizeof *colors)

void save_appearance(void) {
  char *dir, *path, *tmp;
  FILE *fp = 0;
  size_t n;

  byte_xasprintf(&dir, "%s/.disorder", getenv("HOME"));
  byte_xasprintf(&path, "%s/disobedience", dir);
  byte_xasprintf(&tmp, "%s.tmp", path);
  mkdir(dir, 02700);                    /* make sure directory exists */
  if(!(fp = fopen(tmp, "w"))) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
               tmp, strerror(errno));
    goto done;
  }
  for(n = 0; n < NCOLORS; ++n)
    if(fprintf(fp, "color %-20s 0x%04X 0x%04X 0x%04X\n", colors[n].name,
               colors[n].color->red,
               colors[n].color->green,
               colors[n].color->blue) < 0) {
      fpopup_msg(GTK_MESSAGE_ERROR, "error writing to %s: %s",
                 tmp, strerror(errno));
      goto done;
    }
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

void load_appearance(void) {
  char *path, *line;
  FILE *fp;
  size_t n;
  char **vec;
  int nvec;

  byte_xasprintf(&path, "%s/.disorder/disobedience", getenv("HOME"));
  if(!(fp = fopen(path, "r"))) {
    if(errno != ENOENT)
      fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
                 path, strerror(errno));
    return;
  }
  while(!inputline(path, fp, &line, '\n')) {
    if(!(vec = split(line, &nvec, SPLIT_COMMENTS|SPLIT_QUOTES, 0, 0))
       || !nvec)
      continue;
    if(!strcmp(vec[0], "color")) {
      if(nvec != 5) {
        error(0, "%s: malformed '%s' command", path, vec[0]);
        continue;
      }
      for(n = 0; n < NCOLORS && strcmp(colors[n].name, vec[1]); ++n)
        ;
      if(n >= NCOLORS) {
        error(0, "%s: unknown color '%s'", path, vec[1]);
        continue;
      }
      colors[n].color->red = strtoul(vec[2], 0, 0);
      colors[n].color->green = strtoul(vec[3], 0, 0);
      colors[n].color->blue = strtoul(vec[4], 0, 0);
    } else
      /* mention errors but otherwise ignore them */
      error(0, "%s: unknown command '%s'", path, vec[0]);
  }
  if(ferror(fp)) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error reading %s: %s",
               path, strerror(errno));
    fclose(fp);
  }
  tool_active = tool_bg;
  tool_active.red = clamp(105 * tool_active.red / 100);
  tool_active.green = clamp(105 * tool_active.green / 100);
  tool_active.blue = clamp(105 * tool_active.blue / 100);
}

/** @brief Callback used by set_tool_colors() */
static void set_tool_colors_callback(GtkWidget *w,
                                     gpointer attribute((unused)) data) {
  set_tool_colors(w);
}

/** @brief Recursively set tool widget colors */
void set_tool_colors(GtkWidget *w) {
  GtkWidget *child;

  gtk_widget_modify_bg(w, GTK_STATE_NORMAL, &tool_bg);
  gtk_widget_modify_bg(w, GTK_STATE_SELECTED, &selected_bg);
  gtk_widget_modify_bg(w, GTK_STATE_PRELIGHT, &selected_bg);
  gtk_widget_modify_bg(w, GTK_STATE_INSENSITIVE, &tool_bg);
  gtk_widget_modify_fg(w, GTK_STATE_NORMAL, &tool_fg);
  gtk_widget_modify_fg(w, GTK_STATE_SELECTED, &selected_fg);
  gtk_widget_modify_fg(w, GTK_STATE_PRELIGHT, &selected_fg);
  gtk_widget_modify_fg(w, GTK_STATE_INSENSITIVE, &inactive_tool_fg);
  if(GTK_IS_CONTAINER(w))
    gtk_container_foreach(GTK_CONTAINER(w), set_tool_colors_callback, 0);
  if(GTK_IS_MENU_ITEM(w)
     && (child = gtk_menu_item_get_submenu(GTK_MENU_ITEM(w))))
    set_tool_colors(child);
}

/** @brief Set the colors for a slider */
void set_slider_colors(GtkWidget *w) {
  if(!w)
    return;
  gtk_widget_modify_bg(w, GTK_STATE_NORMAL, &tool_bg);
  gtk_widget_modify_bg(w, GTK_STATE_ACTIVE, &tool_bg);
  gtk_widget_modify_bg(w, GTK_STATE_SELECTED, &tool_active);
  gtk_widget_modify_bg(w, GTK_STATE_PRELIGHT, &tool_active);
  gtk_widget_modify_fg(w, GTK_STATE_NORMAL, &tool_fg);
  gtk_widget_modify_fg(w, GTK_STATE_ACTIVE, &tool_fg);
  gtk_widget_modify_fg(w, GTK_STATE_SELECTED, &tool_fg);
  gtk_widget_modify_fg(w, GTK_STATE_PRELIGHT, &tool_fg);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
