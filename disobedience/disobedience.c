/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007, 2008 Richard Kettlewell
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
#include "mixer.h"
#include "version.h"

#include <getopt.h>
#include <locale.h>
#include <pcre.h>

/* Apologies for the numerous de-consting casts, but GLib et al do not seem to
 * have heard of const. */

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

/** @brief Log client */
disorder_eclient *logclient;

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

/** @brief True if a NOP is in flight */
static int nop_in_flight;

/** @brief True if an rtp-address command is in flight */
static int rtp_address_in_flight;

/** @brief Global tooltip group */
GtkTooltips *tips;

/** @brief True if RTP play is available
 *
 * This is a bit of a bodge...
 */
int rtp_supported;

/** @brief True if RTP play is enabled */
int rtp_is_running;

/** @brief Linked list of functions to call when we reset login parameters */
static struct reset_callback_node {
  struct reset_callback_node *next;
  reset_callback *callback;
} *resets;

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
  /* The current tab is _NORMAL, the rest are _ACTIVE, which is bizarre but
   * produces not too dreadful appearance */
  gtk_widget_set_style(tabs, tool_style);
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
  gtk_widget_set_style(toplevel, tool_style);
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

/** @brief Called occasionally */
static gboolean periodic_slow(gpointer attribute((unused)) data) {
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

/** @brief Called frequently */
static gboolean periodic_fast(gpointer attribute((unused)) data) {
#if 0                                   /* debugging hack */
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
#endif
  if(rtp_supported && mixer_supported(DEFAULT_BACKEND)) {
    int nl, nr;
    if(!mixer_control(DEFAULT_BACKEND, &nl, &nr, 0)
       && (nl != volume_l || nr != volume_r)) {
      volume_l = nl;
      volume_r = nr;
      event_raise("volume-changed", 0);
    }
  }
  return TRUE;
}

/** @brief Called when a NOP completes */
static void nop_completed(void attribute((unused)) *v,
                          const char attribute((unused)) *error) {
  /* TODO report the error somewhere */
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
  if(rtp_supported) {
    const int rtp_was_running = rtp_is_running;
    rtp_is_running = rtp_running();
    if(rtp_was_running != rtp_is_running)
      event_raise("rtp-changed", 0);
  }
  return TRUE;                          /* keep call me please */
}

/** @brief Called when a rtp-address command succeeds */
static void got_rtp_address(void attribute((unused)) *v,
                            const char *error,
                            int attribute((unused)) nvec,
                            char attribute((unused)) **vec) {
  const int rtp_was_running = rtp_is_running;

  ++suppress_actions;
  rtp_address_in_flight = 0;
  if(error) {
    /* An error just means that we're not using network play */
    rtp_supported = 0;
    rtp_is_running = 0;
  } else {
    rtp_supported = 1;
    rtp_is_running = rtp_running();
  }
  if(rtp_is_running != rtp_was_running)
    event_raise("rtp-changed", 0);
  --suppress_actions;
}

/** @brief Called to check whether RTP play is available */
static void check_rtp_address(void) {
  if(!rtp_address_in_flight)
    disorder_eclient_rtp_address(client, got_rtp_address, NULL);
}

/* main -------------------------------------------------------------------- */

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "tufnel", no_argument, 0, 't' },
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

/* reset state */
void reset(void) {
  struct reset_callback_node *r;

  /* reset the clients */
  disorder_eclient_close(client);
  disorder_eclient_close(logclient);
  rtp_supported = 0;
  for(r = resets; r; r = r->next)
    r->callback();
  /* Might be a new server so re-check */
  check_rtp_address();
}

/** @brief Register a reset callback */
void register_reset(reset_callback *callback) {
  struct reset_callback_node *const r = xmalloc(sizeof *r);

  r->next = resets;
  r->callback = callback;
  resets = r;
}

int main(int argc, char **argv) {
  int n;
  gboolean gtkok;

  mem_init();
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
#if MTRACK
  mtrack_hash = hash_new(sizeof (int));
  g_mem_set_vtable((GMemVTable *)&glib_memvtable);
#endif
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  gtkok = gtk_init_check(&argc, &argv);
  while((n = getopt_long(argc, argv, "hVc:dtHC", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disobedience");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 't': goesupto = 11; break;
    default: fatal(0, "invalid option");
    }
  }
  if(!gtkok)
    fatal(0, "failed to initialize GTK+");
  signal(SIGPIPE, SIG_IGN);
  init_styles();
  load_settings();
  /* create the event loop */
  D(("create main loop"));
  mainloop = g_main_loop_new(0, 0);
  if(config_read(0)) fatal(0, "cannot read configuration");
  /* create the clients */
  if(!(client = gtkclient())
     || !(logclient = gtkclient()))
    return 1;                           /* already reported an error */
  /* periodic operations (e.g. expiring the cache, checking local volume) */
  g_timeout_add(600000/*milliseconds*/, periodic_slow, 0);
  g_timeout_add(1000/*milliseconds*/, periodic_fast, 0);
  /* global tooltips */
  tips = gtk_tooltips_new();
  make_toplevel_window();
  /* reset styles now everything has its name */
  gtk_rc_reset_styles(gtk_settings_get_for_screen(gdk_screen_get_default()));
  gtk_widget_show_all(toplevel);
  /* issue a NOP every so often */
  g_timeout_add_full(G_PRIORITY_LOW,
                     2000/*interval, ms*/,
                     maybe_send_nop,
                     0/*data*/,
                     0/*notify*/);
  register_reset(properties_reset);
  /* Start monitoring the log */
  disorder_eclient_log(logclient, &log_callbacks, 0);
  /* See if RTP play supported */
  check_rtp_address();
  suppress_actions = 0;
  /* If no password is set yet pop up a login box */
  if(!config->password)
    login_box();
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
