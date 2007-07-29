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

#include "disobedience.h"

#include <getopt.h>
#include <locale.h>
#include <pcre.h>

/* Apologies for the numerous de-consting casts, but GLib et al do not seem to
 * have heard of const. */

#include "style.h"                      /* generated style */

/* Functions --------------------------------------------------------------- */

static void log_connected(void *v);
static void log_completed(void *v, const char *track);
static void log_failed(void *v, const char *track, const char *status);
static void log_moved(void *v, const char *user);
static void log_playing(void *v, const char *track, const char *user);
static void log_queue(void *v, struct queue_entry *q);
static void log_recent_added(void *v, struct queue_entry *q);
static void log_recent_removed(void *v, const char *id);
static void log_removed(void *v, const char *id, const char *user);
static void log_scratched(void *v, const char *track, const char *user);
static void log_state(void *v, unsigned long state);
static void log_volume(void *v, int l, int r);

/* Variables --------------------------------------------------------------- */

GMainLoop *mainloop;                    /* event loop */
GtkWidget *toplevel;                    /* top level window */
GtkWidget *report_label;                /* label for progress indicator */
GtkWidget *tabs;                        /* main tabs */

disorder_eclient *client;               /* main client */

unsigned long last_state;               /* last reported state */
int playing;                            /* true if playing some track */
int volume_l, volume_r;                 /* volume */
double goesupto = 10;                   /* volume upper bound */
int choosealpha;                        /* break up choose by letter */

static const disorder_eclient_log_callbacks gdisorder_log_callbacks = {
  log_connected,
  log_completed,
  log_failed,
  log_moved,
  log_playing,
  log_queue,
  log_recent_added,
  log_recent_removed,
  log_removed,
  log_scratched,
  log_state,
  log_volume
};

/* Window creation --------------------------------------------------------- */

/* Note that all the client operations kicked off from here will only complete
 * after we've entered the event loop. */

static gboolean delete_event(GtkWidget attribute((unused)) *widget,
                             GdkEvent attribute((unused)) *event,
                             gpointer attribute((unused)) data) {
  D(("delete_event"));
  exit(0);                              /* die immediately */
}

/* Called when the current tab is switched. */
static void tab_switched(GtkNotebook attribute((unused)) *notebook,
                         GtkNotebookPage attribute((unused)) *page,
                         guint page_num,
                         gpointer attribute((unused)) user_data) {
  menu_update(page_num);
}

static GtkWidget *report_box(void) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  report_label = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(report_label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment(GTK_MISC(report_label), 0, 0);
  gtk_container_add(GTK_CONTAINER(vbox), gtk_hseparator_new());
  gtk_container_add(GTK_CONTAINER(vbox), report_label);
  return vbox;
}

static GtkWidget *notebook(void) {
  tabs = gtk_notebook_new();
  g_signal_connect(tabs, "switch-page", G_CALLBACK(tab_switched), 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), queue_widget(),
                           gtk_label_new("Queue"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), recent_widget(),
                           gtk_label_new("Recent"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), choose_widget(),
                           gtk_label_new("Choose"));
  return tabs;
}

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

/* State monitoring -------------------------------------------------------- */

static void all_update(void) {
  playing_update();
  queue_update();
  recent_update();
  control_update();
}

static void log_connected(void attribute((unused)) *v) {
  struct callbackdata *cbd;

  /* Don't know what we might have missed while disconnected so update
   * everything.  We get this at startup too and this is how we do the initial
   * state fetch. */
  all_update();
  /* Re-get the volume */
  cbd = xmalloc(sizeof *cbd);
  cbd->onerror = 0;
  disorder_eclient_volume(client, log_volume, -1, -1, cbd);
}

static void log_completed(void attribute((unused)) *v,
                          const char attribute((unused)) *track) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_failed(void attribute((unused)) *v,
                       const char attribute((unused)) *track,
                       const char attribute((unused)) *status) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_moved(void attribute((unused)) *v,
                      const char attribute((unused)) *user) {
   queue_update();
}

static void log_playing(void attribute((unused)) *v,
                        const char attribute((unused)) *track,
                        const char attribute((unused)) *user) {
  playing = 1;
  playing_update();
  control_update();
  /* we get a log_removed() anyway so we don't need to update_queue() from
   * here */
}

static void log_queue(void attribute((unused)) *v,
                      struct queue_entry attribute((unused)) *q) {
  queue_update();
}

static void log_recent_added(void attribute((unused)) *v,
                             struct queue_entry attribute((unused)) *q) {
  recent_update();
}

static void log_recent_removed(void attribute((unused)) *v,
                               const char attribute((unused)) *id) {
  /* nothing - log_recent_added() will trigger the relevant update */
}

static void log_removed(void attribute((unused)) *v,
                        const char attribute((unused)) *id,
                        const char attribute((unused)) *user) {
  queue_update();
}

static void log_scratched(void attribute((unused)) *v,
                          const char attribute((unused)) *track,
                          const char attribute((unused)) *user) {
  playing = 0;
  playing_update();
  control_update();
}

static void log_state(void attribute((unused)) *v,
                      unsigned long state) {
  last_state = state;
  control_update();
  /* If the track is paused or resume then the currently playing track is
   * refetched so that we can continue to correctly calculate the played so-far
   * field */
  playing_update();
}

static void log_volume(void attribute((unused)) *v,
                       int l, int r) {
  if(volume_l != l || volume_r != r) {
    volume_l = l;
    volume_r = r;
    control_update();
  }
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

/* Called once every 10 minutes */
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
  return TRUE;                          /* don't remove me */
}

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
  if(config_read()) fatal(0, "cannot read configuration");
  /* create the clients */
  if(!(client = gtkclient())
     || !(logclient = gtkclient()))
    return 1;                           /* already reported an error */
  disorder_eclient_log(logclient, &gdisorder_log_callbacks, 0);
  /* periodic operations (e.g. expiring the cache) */
#if MDEBUG
  g_timeout_add(5000/*milliseconds*/, periodic, 0);
#else
  g_timeout_add(600000/*milliseconds*/, periodic, 0);
#endif
  /* The point of this is to try and get a handle on mysterious
   * unresponsiveness.  It's not very useful in production use. */
  if(0)
    g_timeout_add(1000/*milliseconds*/, heartbeat, 0);
  make_toplevel_window();
  /* reset styles now everything has its name */
  gtk_rc_reset_styles(gtk_settings_get_for_screen(gdk_screen_get_default()));
  gtk_widget_show_all(toplevel);
  D(("enter main loop"));
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
