/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file disobedience/disobedience.c
 * @brief Main Disobedience program
 */

#include "disobedience.h"

#include <getopt.h>
#include <locale.h>
#include <pcre.h>

/* Apologies for the numerous de-consting casts, but GLib et al do not seem to
 * have heard of const. */

#include "style.h"                      /* generated style */

/* Variables --------------------------------------------------------------- */

/** @brief Event loop */
GMainLoop *mainloop;

/** @brief Top-level window */
GtkWidget *toplevel;

/** @brief Label for progress indicator */
GtkWidget *report_label;

/** @brief Main tab group */
GtkWidget *tabs;

/** @brief Main client */
disorder_eclient *client;

/** @brief Last reported state
 *
 * This is updated by log_state().
 */
unsigned long last_state;

/** @brief True if some track is playing
 *
 * This ought to be removed in favour of last_state & DISORDER_PLAYING
 */
int playing;

/** @brief Left channel volume */
int volume_l;

/** @brief Right channel volume */
int volume_r;

double goesupto = 10;                   /* volume upper bound */

/** @brief Break up choose tab by initial letter */
int choosealpha;

/** @brief True if a NOP is in flight */
static int nop_in_flight;

/** @brief Global tooltip group */
GtkTooltips *tips;

/* Window creation --------------------------------------------------------- */

/* Note that all the client operations kicked off from here will only complete
 * after we've entered the event loop. */

/** @brief Called when main window is deleted
 *
 * Terminates the program.
 */
static gboolean delete_event(GtkWidget attribute((unused)) *widget,
                             GdkEvent attribute((unused)) *event,
                             gpointer attribute((unused)) data) {
  D(("delete_event"));
  exit(0);                              /* die immediately */
}

/** @brief Called when the current tab is switched
 *
 * Updates the menu settings to correspond to the new page.
 */
static void tab_switched(GtkNotebook attribute((unused)) *notebook,
                         GtkNotebookPage attribute((unused)) *page,
                         guint page_num,
                         gpointer attribute((unused)) user_data) {
  menu_update(page_num);
}

/** @brief Create the report box */
static GtkWidget *report_box(void) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  report_label = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(report_label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment(GTK_MISC(report_label), 0, 0);
  gtk_container_add(GTK_CONTAINER(vbox), gtk_hseparator_new());
  gtk_container_add(GTK_CONTAINER(vbox), report_label);
  return vbox;
}

/** @brief Create and populate the main tab group */
static GtkWidget *notebook(void) {
  tabs = gtk_notebook_new();
  g_signal_connect(tabs, "switch-page", G_CALLBACK(tab_switched), 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), queue_widget(),
                           gtk_label_new("Queue"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), recent_widget(),
                           gtk_label_new("Recent"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), choose_widget(),
                           gtk_label_new("Choose"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), added_widget(),
                           gtk_label_new("Added"));
  return tabs;
}

/** @brief Create and populate the main window */
static void make_toplevel_window(void) {
  GtkWidget *const vbox = gtk_vbox_new(FALSE, 1);
  GtkWidget *const rb = report_box();

  D(("top_window"));
  toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(toplevel), 640, 480);
  /* terminate on close */
  g_signal_connect(G_OBJECT(toplevel), "delete_event",
                   G_CALLBACK(delete_event), NULL);
  /* lay out the window */
  gtk_window_set_title(GTK_WINDOW(toplevel), "Disobedience");
  gtk_container_add(GTK_CONTAINER(toplevel), vbox);
  /* lay out the vbox */
  gtk_box_pack_start(GTK_BOX(vbox),
                     menubar(toplevel),
                     FALSE,             /* expand */
                     FALSE,             /* fill */
                     0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     control_widget(),
                     FALSE,             /* expand */
                     FALSE,             /* fill */
                     0);
  gtk_container_add(GTK_CONTAINER(vbox), notebook());
  gtk_box_pack_end(GTK_BOX(vbox),
                   rb,
                   FALSE,             /* expand */
                   FALSE,             /* fill */
                   0);
  gtk_widget_set_name(toplevel, "disobedience");
}

#if MDEBUG
static int widget_count, container_count;

static void count_callback(GtkWidget *w,
                           gpointer attribute((unused)) data) {
  ++widget_count;
  if(GTK_IS_CONTAINER(w)) {
    ++container_count;
    gtk_container_foreach(GTK_CONTAINER(w), count_callback, 0);
  }
}

static void count_widgets(void) {
  widget_count = 0;
  container_count = 1;
  if(toplevel)
    gtk_container_foreach(GTK_CONTAINER(toplevel), count_callback, 0);
  fprintf(stderr, "widget count: %8d  container count: %8d\n",
          widget_count, container_count);
}
#endif

#if MTRACK
const char *mtag = "init";
static hash *mtrack_hash;

static int *mthfind(const char *tag) {
  static const int zero = 0;
  int *cp = hash_find(mtrack_hash, tag);
  if(!cp) {
    hash_add(mtrack_hash, tag, &zero, HASH_INSERT);
    cp = hash_find(mtrack_hash, tag);
  }
  return cp;
}

static void *trap_malloc(size_t n) {
  void *ptr = malloc(n + sizeof(char *));

  *(const char **)ptr = mtag;
  ++*mthfind(mtag);
  return (char *)ptr + sizeof(char *);
}

static void trap_free(void *ptr) {
  const char *tag;
  if(!ptr)
    return;
  ptr = (char *)ptr - sizeof(char *);
  tag = *(const char **)ptr;
  --*mthfind(tag);
  free(ptr);
}

static void *trap_realloc(void *ptr, size_t n) {
  if(!ptr)
    return trap_malloc(n);
  if(!n) {
    trap_free(ptr);
    return 0;
  }
  ptr = (char *)ptr - sizeof(char *);
  ptr = realloc(ptr, n + sizeof(char *));
  *(const char **)ptr = mtag;
  return (char *)ptr + sizeof(char *);
}

static int report_tags_callback(const char *key, void *value,
                                void attribute((unused)) *u) {
  fprintf(stderr, "%16s: %d\n", key, *(int *)value);
  return 0;
}

static void report_tags(void) {
  hash_foreach(mtrack_hash, report_tags_callback, 0);
  fprintf(stderr, "\n");
}

static const GMemVTable glib_memvtable = {
  trap_malloc,
  trap_realloc,
  trap_free,
  0,
  0,
  0
};
#endif

/** @brief Called once every 10 minutes */
static gboolean periodic(gpointer attribute((unused)) data) {
  D(("periodic"));
  /* Expire cached data */
  cache_expire();
  /* Update everything to be sure that the connection to the server hasn't
   * mysteriously gone stale on us. */
  all_update();
#if MDEBUG
  count_widgets();
  fprintf(stderr, "cache size: %zu\n", cache_count());
#endif
#if MTRACK
  report_tags();
#endif
  return TRUE;                          /* don't remove me */
}

/** @brief Called from time to time
 *
 * Used for debugging purposes
 */
static gboolean heartbeat(gpointer attribute((unused)) data) {
  static struct timeval last;
  struct timeval now;
  double delta;

  xgettimeofday(&now, 0);
  if(last.tv_sec) {
    delta = (now.tv_sec + now.tv_sec / 1.0E6) 
      - (last.tv_sec + last.tv_sec / 1.0E6);
    if(delta >= 1.0625)
      fprintf(stderr, "%f: %fs between 1s heartbeats\n", 
              now.tv_sec + now.tv_sec / 1.0E6,
              delta);
  }
  last = now;
  return TRUE;
}

/** @brief Called when a NOP completes */
static void nop_completed(void attribute((unused)) *v) {
  nop_in_flight = 0;
}

/** @brief Called from time to time to arrange for a NOP to be sent
 *
 * At most one NOP remains in flight at any moment.  If the client is not
 * currently connected then no NOP is sent.
 */
static gboolean maybe_send_nop(gpointer attribute((unused)) data) {
  if(!nop_in_flight && (disorder_eclient_state(client) & DISORDER_CONNECTED)) {
    nop_in_flight = 1;
    disorder_eclient_nop(client, nop_completed, 0);
  }
  return TRUE;                          /* keep call me please */
}

/* main -------------------------------------------------------------------- */

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "tufnel", no_argument, 0, 't' },
  { "choosealpha", no_argument, 0, 'C' },
  { "debug", no_argument, 0, 'd' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Disobedience - GUI client for DisOrder\n"
          "\n"
          "Usage:\n"
	  "  disobedience [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "\n"
          "Also GTK+ options will work.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  disorder_eclient *logclient;

  mem_init();
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
#if MTRACK
  mtrack_hash = hash_new(sizeof (int));
  g_mem_set_vtable((GMemVTable *)&glib_memvtable);
#endif
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  gtk_init(&argc, &argv);
  gtk_rc_parse_string(style);
  while((n = getopt_long(argc, argv, "hVc:dtH", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 't': goesupto = 11; break;
    case 'C': choosealpha = 1; break;   /* not well tested any more */
    default: fatal(0, "invalid option");
    }
  }
  signal(SIGPIPE, SIG_IGN);
  /* create the event loop */
  D(("create main loop"));
  mainloop = g_main_loop_new(0, 0);
  if(config_read(0)) fatal(0, "cannot read configuration");
  /* create the clients */
  if(!(client = gtkclient())
     || !(logclient = gtkclient()))
    return 1;                           /* already reported an error */
  /* periodic operations (e.g. expiring the cache) */
#if MDEBUG || MTRACK
  g_timeout_add(5000/*milliseconds*/, periodic, 0);
#else
  g_timeout_add(600000/*milliseconds*/, periodic, 0);
#endif
  /* The point of this is to try and get a handle on mysterious
   * unresponsiveness.  It's not very useful in production use. */
  if(0)
    g_timeout_add(1000/*milliseconds*/, heartbeat, 0);
  /* global tooltips */
  tips = gtk_tooltips_new();
  make_toplevel_window();
  /* reset styles now everything has its name */
  gtk_rc_reset_styles(gtk_settings_get_for_screen(gdk_screen_get_default()));
  gtk_widget_show_all(toplevel);
  /* issue a NOP every so often */
  g_timeout_add_full(G_PRIORITY_LOW,
                     1000/*interval, ms*/,
                     maybe_send_nop,
                     0/*data*/,
                     0/*notify*/);
  /* Start monitoring the log */
  disorder_eclient_log(logclient, &log_callbacks, 0);
  D(("enter main loop"));
  MTAG("misc");
  g_main_loop_run(mainloop);
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
