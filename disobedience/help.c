/*
 * This file is part of DisOrder
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

#include "disobedience.h"
#include "table.h"
#include "html.h"
#include "manual.h"

VECTOR_TYPE(markstack, GtkTextMark *, xrealloc);

/** @brief Known tag type */
struct tag {
  /** @brief HTML tag name */
  const char *name;

  /** @brief Called to set up the tag */
  void (*init)(GtkTextTag *tag);
  
  /** @brief GTK+ tag object */
  GtkTextTag *tag;
};

static void init_bold(GtkTextTag *tag) {
  g_object_set(G_OBJECT(tag), "weight", PANGO_WEIGHT_BOLD, (char *)0);
}

static void init_italic(GtkTextTag *tag) {
  g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, (char *)0);
}

static void init_pre(GtkTextTag *tag) {
  g_object_set(G_OBJECT(tag), "family", "monospace", (char *)0);
}
/** @brief Table of known tags
 *
 * Keep in alphabetical order
 */
static  struct tag tags[] = {
  { "b", init_bold, 0 },
  { "i", init_italic, 0 },
  { "pre", init_pre, 0 }
};

/** @brief Number of known tags */
#define NTAGS (sizeof tags / sizeof *tags)

/** @brief State structure for insert_html() */
struct state {
  /** @brief The buffer to insert into */
  GtkTextBuffer *buffer;

  /** @brief True if we are inside <body> */
  int body;

  /** @brief True if inside <pre> */
  int pre;

  /** @brief True if a space is required before any non-space */
  int space;

  /** @brief Stack of marks corresponding to tags */
  struct markstack marks[1];

};

/** @brief Called for an open tag */
static void html_open(const char *tag,
		      hash attribute((unused)) *attrs,
		      void *u) {
  struct state *const s = u;
  GtkTextIter iter[1];

  if(!strcmp(tag, "body"))
    ++s->body;
  else if(!strcmp(tag, "pre"))
    ++s->pre;
  if(!s->body)
    return;
  /* push a mark for the start of the region */
  gtk_text_buffer_get_iter_at_mark(s->buffer, iter,
				   gtk_text_buffer_get_insert(s->buffer));
  markstack_append(s->marks,
		   gtk_text_buffer_create_mark(s->buffer,
					       0/* mark_name */,
					       iter,
					       TRUE/*left_gravity*/));
}

/** @brief Called for a close tag */
static void html_close(const char *tag,
		       void *u) {
  struct state *const s = u;
  GtkTextIter start[1], end[1];
  int n;

  if(!strcmp(tag, "body"))
    --s->body;
  else if(!strcmp(tag, "pre")) {
    --s->pre;
    s->space = 0;
  }
  if(!s->body)
    return;
  /* see if this is a known tag */
  if((n = TABLE_FIND(tags, struct tag, name, tag)) < 0)
    return;
  /* pop the mark at the start of the region */
  assert(s->marks->nvec > 0);
  gtk_text_buffer_get_iter_at_mark(s->buffer, start,
				   s->marks->vec[--s->marks->nvec]);
  gtk_text_buffer_get_iter_at_mark(s->buffer, end,
				   gtk_text_buffer_get_insert(s->buffer));
  /* apply a tag */
  gtk_text_buffer_apply_tag(s->buffer, tags[n].tag, start, end);
  /* don't need the start mark any more */
  gtk_text_buffer_delete_mark(s->buffer, s->marks->vec[s->marks->nvec]);
}

/** @brief Called for text */
static void html_text(const char *text,
		      void *u) {
  struct state *const s = u;

  /* ignore header */
  if(!s->body)
    return;
  if(!s->pre) {
    char *formatted = xmalloc(strlen(text) + 1), *t = formatted;
    /* normalize spacing */
    while(*text) {
      if(isspace((unsigned char)*text)) {
	s->space = 1;
	++text;
      } else {
	if(s->space) {
	  *t++ = ' ';
	  s->space = 0;
	}
	*t++ = *text++;
      }
    }
    *t = 0;
    text = formatted;
  }
  gtk_text_buffer_insert_at_cursor(s->buffer, text, strlen(text));
}

/** @brief Callbacks for insert_html() */
static const struct html_parser_callbacks insert_html_callbacks = {
  html_open,
  html_close,
  html_text
};

/** @brief Insert @p html into @p buffer at cursor */
static void insert_html(GtkTextBuffer *buffer,
			const char *html) {
  struct state s[1];
  size_t n;
  GtkTextTagTable *tagtable;

  memset(s, 0, sizeof *s);
  s->buffer = buffer;
  markstack_init(s->marks);
  /* initialize tags */
  if(!tags[0].tag)
    for(n = 0; n < NTAGS; ++n)
      tags[n].init(tags[n].tag = gtk_text_tag_new(0));
  /* add tags to this buffer */
  tagtable = gtk_text_buffer_get_tag_table(s->buffer);
  for(n = 0; n < NTAGS; ++n)
    gtk_text_tag_table_add(tagtable, tags[n].tag);
  /* convert the input */
  html_parse(&insert_html_callbacks, html, s);
}

static GtkTextBuffer *html_buffer(const char *html) {
  GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);

  insert_html(buffer, html);
  return buffer;
}

static GtkWidget *help_window;

void popup_help(void) {
  GtkWidget *view;
  
  if(help_window) {
    gtk_window_present(GTK_WINDOW(help_window));
    return;
  }
  help_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(help_window, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &help_window);
  gtk_window_set_title(GTK_WINDOW(help_window), "Disobedience Manual");
  view = gtk_text_view_new_with_buffer(html_buffer(manual));
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_container_add(GTK_CONTAINER(help_window),
		    scroll_widget(view,
				  "help"));
  gtk_window_set_default_size(GTK_WINDOW(help_window), 480, 512);
  gtk_widget_show_all(help_window);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
