/*
 * This file is part of Disobedience
 * Copyright (C) 2007, 2008 Richard Kettlewell
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

/** @brief Table of styles */
static const struct {
  const char *name;
  GtkStyle **style;
} styles[] = {
  { "layout", &layout_style },
  { "title", &title_style },
  { "even", &even_style },
  { "odd", &odd_style },
  { "active", &active_style },
  { "tool", &tool_style },
  { "search", &search_style },
  { "drag", &drag_style },
};

#define NSTYLES (sizeof styles / sizeof *styles)

/** @brief Table of state types */
static const char *const states[] = {
  "normal",
  "active",
  "prelight",
  "selected",
  "insensitive"
};

#define NSTATES (sizeof states / sizeof *states)

/** @brief Table of colors */
static const struct {
  const char *name;
  size_t offset;
} colors[] = {
  { "fg", offsetof(GtkStyle, fg) },
  { "bg", offsetof(GtkStyle, bg) },
};

#define NCOLORS (sizeof colors / sizeof *colors)

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

void save_settings(void) {
  char *dir, *path, *tmp;
  FILE *fp = 0;
  size_t n, m, c;

  byte_xasprintf(&dir, "%s/.disorder", getenv("HOME"));
  byte_xasprintf(&path, "%s/disobedience", dir);
  byte_xasprintf(&tmp, "%s.tmp", path);
  mkdir(dir, 02700);                    /* make sure directory exists */
  if(!(fp = fopen(tmp, "w"))) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
               tmp, strerror(errno));
    goto done;
  }
  if(fprintf(fp,
             "# automatically generated!\n\n") < 0)
    goto write_error;
  for(n = 0; n < NSTYLES; ++n)
    for(c = 0; c < NCOLORS; ++c)
      for(m = 0; m < NSTATES; ++m) {
        const GdkColor *color = (GdkColor *)((char *)styles[n].style + colors[c].offset) + m;
        if(fprintf(fp, "color %8s %12s %s 0x%04x 0x%04x 0x%04x\n",
                   styles[n].name, states[m], colors[c].name,
                   color->red,
                   color->green,
                   color->blue) < 0)
          goto write_error;
      }
  if(fprintf(fp, "browser %s\n", browser) < 0)
    goto write_error;
  if(fclose(fp) < 0) {
    fp = 0;
  write_error:
    fpopup_msg(GTK_MESSAGE_ERROR, "error writing to %s: %s",
               tmp, strerror(errno));
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
  size_t n, m, c;

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
        GdkColor *color;
        if(nvec != 7) {
          error(0, "%s: malformed '%s' command", path, vec[0]);
          continue;
        }
        for(n = 0; n < NSTYLES && strcmp(styles[n].name, vec[1]); ++n)
          ;
        if(n >= NSTYLES) {
          error(0, "%s: unknown style '%s'", path, vec[1]);
          continue;
        }
        for(m = 0; m < NSTATES && strcmp(states[m], vec[2]); ++m)
          ;
        if(m >= NSTATES) {
          error(0, "%s: unknown state '%s'", path, vec[2]);
          continue;
        }
        for(c = 0; c < NCOLORS && strcmp(colors[c].name, vec[3]); ++c)
          ;
        if(c >= NCOLORS) {
          error(0, "%s: unknown color '%s'", path, vec[3]);
          continue;
        }
        color = (GdkColor *)((char *)styles[n].style + colors[c].offset) + m;
        color->red = strtoul(vec[4], 0, 0);
        color->green = strtoul(vec[5], 0, 0);
        color->blue = strtoul(vec[6], 0, 0);
      } else if(!strcmp(vec[0], "browser")) {
        if(nvec != 2) {
          error(0, "%s: malformed '%s' command", path, vec[0]);
          continue;
        }
        browser = vec[1];
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

/** @brief Recursively set tool widget colors
 *
 * This is currently unused; the idea was to allow for configurability without
 * allowing GTK+ to override our use of color, but things seem generally better
 * without this particular call.
 */
void set_tool_colors(GtkWidget attribute((unused)) *w) {
}

/** @brief Pop up a settings editor widget */
void popup_settings(void) {
  static GtkWidget *settings_window;
  GtkWidget *table;
  unsigned row, col;

  if(settings_window) { 
    gtk_window_present(GTK_WINDOW(settings_window));
    return;
  }
  /* Create the window */
  settings_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_style(settings_window, tool_style);
  gtk_window_set_title(GTK_WINDOW(settings_window), "Disobedience Settings");
  /* Clear the pointer to the window widget when it is closed */
  g_signal_connect(settings_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &settings_window);

  /* The color settings live in a big table */
  table = gtk_table_new(2 * NSTYLES + 1/*rows */, NSTATES + 1/*cols*/,
                        TRUE/*homogeneous*/);
  /* Titles */
  for(row = 0; row < 2 * NSTYLES; ++row) {
    char *legend;

    byte_xasprintf(&legend, "%s %s", styles[row / 2].name,
                   row % 2 ? "background" : "foreground");
    gtk_table_attach(GTK_TABLE(table),
                     gtk_label_new(legend),
                     0, 1,
                     row + 1, row + 2,
                     GTK_FILL, GTK_FILL,
                     1, 1);
  }
  for(col = 0; col < NSTATES; ++col) {
    gtk_table_attach(GTK_TABLE(table),
                     gtk_label_new(states[col]),
                     col + 1, col + 2,
                     0, 1,
                     GTK_FILL, GTK_FILL,
                     1, 1);
  }
  /* The actual colors */
  for(row = 0; row < 2 * NSTYLES; ++row) {
    for(col = 0; col <  NSTATES; ++col) {
      GdkColor *const c = &(row % 2
                            ? (**styles[row / 2].style).bg
                            : (**styles[row / 2].style).fg)[col];
      gtk_table_attach(GTK_TABLE(table),
                       gtk_color_button_new_with_color(c),
                       col + 1, col + 2,
                       row + 1, row + 2,
                       GTK_FILL, GTK_FILL,
                       1, 1);
    }
  }
  gtk_container_add(GTK_CONTAINER(settings_window), frame_widget(table, NULL));
  gtk_widget_show_all(settings_window);
  /* TODO: save settings
     TODO: web browser
     TODO: impose settings when they are set
  */
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
