/*
 * This file is part of DisOrder.
 * Copyright (C) 2006-2009 Richard Kettlewell
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
/** @file disobedience/disobedience.c
 * @brief Main Disobedience program
 */

#include "disobedience.h"
#include "version.h"

#include <getopt.h>
#include <locale.h>
#include <pcre.h>
#include <gcrypt.h>

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

/** @brief Mini-mode widget for playing track */
GtkWidget *playing_mini;

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

/** @brief Audio backend */
const struct uaudio *backend;

double goesupto = 10;                   /* volume upper bound */

/** @brief True if a NOP is in flight */
static int nop_in_flight;

/** @brief True if an rtp-address command is in flight */
static int rtp_address_in_flight;

/** @brief True if a rights lookup is in flight */
static int rights_lookup_in_flight;

/** @brief Current rights bitmap */
rights_type last_rights;

/** @brief True if RTP play is available
 *
 * This is a bit of a bodge...
 */
int rtp_supported;

/** @brief True if RTP play is enabled */
int rtp_is_running;

/** @brief Server version */
const char *server_version;

/** @brief Parsed server version */
long server_version_bytes;

static GtkWidget *queue;

static GtkWidget *notebook_box;

static void check_rtp_address(const char *event,
                              void *eventdata,
                              void *callbackdata);

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
static void tab_switched(GtkNotebook *notebook,
                         GtkNotebookPage attribute((unused)) *page,
                         guint page_num,
                         gpointer attribute((unused)) user_data) {
  GtkWidget *const tab = gtk_notebook_get_nth_page(notebook, page_num);
  const struct tabtype *const t = g_object_get_data(G_OBJECT(tab), "type");
  assert(t != 0);
  if(t->selected)
    t->selected();
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
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), queue = queue_widget(),
                           gtk_label_new("Queue"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), recent_widget(),
                           gtk_label_new("Recent"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), choose_widget(),
                           gtk_label_new("Choose"));
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), added_widget(),
                           gtk_label_new("Added"));
  return tabs;
}

/* Tracking of window sizes */
static int toplevel_width = 640, toplevel_height = 480;
static int mini_width = 480, mini_height = 140;
static struct timeval last_mode_switch;

static void main_minimode(const char attribute((unused)) *event,
                          void attribute((unused)) *evendata,
                          void attribute((unused)) *callbackdata) {
  if(full_mode) {
    gtk_window_resize(GTK_WINDOW(toplevel), toplevel_width, toplevel_height);
    gtk_widget_show(tabs);
    gtk_widget_hide(playing_mini);
    /* Show the queue (bit confusing otherwise!) */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs), 0);
  } else {
    gtk_window_resize(GTK_WINDOW(toplevel), mini_width, mini_height);
    gtk_widget_hide(tabs);
    gtk_widget_show(playing_mini);
  }
  xgettimeofday(&last_mode_switch, NULL);
}

/* Called when the window size is allocate */
static void toplevel_size_allocate(GtkWidget attribute((unused)) *w,
                                   GtkAllocation *a,
                                   gpointer attribute((unused)) user_data) {
  struct timeval now;
  xgettimeofday(&now, NULL);
  if(tvdouble(tvsub(now, last_mode_switch)) < 0.5) {
    /* Suppress size-allocate signals that are within half a second of a mode
     * switch: they are quite likely to be the result of re-arranging widgets
     * within the old size, not the application of the new size.  Yes, this is
     * a disgusting hack! */
    return;                             /* OMG too soon! */
  }
  if(full_mode) {
    toplevel_width = a->width;
    toplevel_height = a->height;
  } else {
    mini_width = a->width;
    mini_height = a->height;
  }
}

/* Periodically check the toplevel's size
 * (the hack in toplevel_size_allocate() means we could in principle
 * miss a user-initiated resize)
 */
static void check_toplevel_size(const char attribute((unused)) *event,
                                void attribute((unused)) *evendata,
                                void attribute((unused)) *callbackdata) {
  GtkAllocation a;
  gtk_window_get_size(GTK_WINDOW(toplevel), &a.width, &a.height);
  toplevel_size_allocate(NULL, &a, NULL);
}

static void hack_window_title(const char attribute((unused)) *event,
                              void attribute((unused)) *eventdata,
                              void attribute((unused)) *callbackdata) {
  char *p;
  const char *note;
  static const char *last_note = 0;

  if(!(last_state & DISORDER_CONNECTED))
    note = "(disconnected)";
  else if(last_state & DISORDER_TRACK_PAUSED)
    note = "(paused)";
  else if(playing_track) {
    byte_asprintf(&p, "'%s' by %s, from '%s'",
                  namepart(playing_track->track, "display", "title"),
                  namepart(playing_track->track, "display", "artist"),
                  namepart(playing_track->track, "display", "album"));
    note = p;
  } else if(!(last_state & DISORDER_PLAYING_ENABLED))
    note = "(playing disabled)";
  else if(!(last_state & DISORDER_RANDOM_ENABLED))
    note = "(random play disabled)";
  else
    note = "(nothing to play for unknown reason)";

  if(last_note && !strcmp(note, last_note))
    return;
  last_note = xstrdup(note);
  byte_asprintf(&p, "Disobedience: %s", note);
  gtk_window_set_title(GTK_WINDOW(toplevel), p);
}

/** @brief Create and populate the main window */
static void make_toplevel_window(void) {
  GtkWidget *const vbox = gtk_vbox_new(FALSE/*homogeneous*/, 1/*spacing*/);
  GtkWidget *const rb = report_box();

  D(("top_window"));
  toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  /* default size is too small */
  gtk_window_set_default_size(GTK_WINDOW(toplevel),
                              toplevel_width, toplevel_height);
  /* terminate on close */
  g_signal_connect(G_OBJECT(toplevel), "delete_event",
                   G_CALLBACK(delete_event), NULL);
  /* track size */
  g_signal_connect(G_OBJECT(toplevel), "size-allocate",
                   G_CALLBACK(toplevel_size_allocate), NULL);
  /* lay out the window */
  hack_window_title(0, 0, 0);
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
  playing_mini = playing_widget();
  gtk_box_pack_start(GTK_BOX(vbox),
                     playing_mini,
                     FALSE,
                     FALSE,
                     0);
  notebook_box = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(notebook_box), notebook());
  gtk_container_add(GTK_CONTAINER(vbox), notebook_box);
  gtk_box_pack_end(GTK_BOX(vbox),
                   rb,
                   FALSE,             /* expand */
                   FALSE,             /* fill */
                   0);
  gtk_widget_set_style(toplevel, tool_style);
  event_register("mini-mode-changed", main_minimode, 0);
  event_register("periodic-fast", check_toplevel_size, 0);
  event_register("playing-track-changed", hack_window_title, 0);
  event_register("enabled-changed", hack_window_title, 0);
  event_register("random-changed", hack_window_title, 0);
  event_register("pause-changed", hack_window_title, 0);
  event_register("playing-changed", hack_window_title, 0);
  event_register("connected-changed", hack_window_title, 0);
  event_register("lookups-completed", hack_window_title, 0);
}

static void userinfo_rights_completed(void attribute((unused)) *v,
                                      const char *err,
                                      const char *value) {
  rights_type r;

  if(err) {
    popup_protocol_error(0, err);
    r = 0;
  } else {
    if(parse_rights(value, &r, 0))
      r = 0;
  }
  /* If rights have changed, signal everything that cares */
  if(r != last_rights) {
    last_rights = r;
    ++suppress_actions;
    event_raise("rights-changed", 0);
    --suppress_actions;
  }
  rights_lookup_in_flight = 0;
}

static void check_rights(void) {
  if(!rights_lookup_in_flight) {
    rights_lookup_in_flight = 1;
    disorder_eclient_userinfo(client,
                              userinfo_rights_completed,
                              config->username, "rights",
                              0);
  }
}

/** @brief Called occasionally */
static gboolean periodic_slow(gpointer attribute((unused)) data) {
  D(("periodic_slow"));
  /* Expire cached data */
  cache_expire();
  /* Update everything to be sure that the connection to the server hasn't
   * mysteriously gone stale on us. */
  all_update();
  event_raise("periodic-slow", 0);
  /* Recheck RTP status too */
  check_rtp_address(0, 0, 0);
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
  if(rtp_supported && backend && backend->get_volume) {
    int nl, nr;
    backend->get_volume(&nl, &nr);
    if(nl != volume_l || nr != volume_r) {
      volume_l = nl;
      volume_r = nr;
      event_raise("volume-changed", 0);
    }
  }
  /* Periodically check what our rights are */
  int recheck_rights = 1;
  if(server_version_bytes >= 0x04010000)
    /* Server versions after 4.1 will send updates */
    recheck_rights = 0;
  if((server_version_bytes & 0xFF) == 0x01)
    /* Development servers might do regardless of their version number */
    recheck_rights = 0;
  if(recheck_rights)
    check_rights();
  event_raise("periodic-fast", 0);
  return TRUE;
}

/** @brief Called when a NOP completes */
static void nop_completed(void attribute((unused)) *v,
                          const char attribute((unused)) *err) {
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
                            const char *err,
                            int attribute((unused)) nvec,
                            char attribute((unused)) **vec) {
  const int rtp_was_supported = rtp_supported;
  const int rtp_was_running = rtp_is_running;

  ++suppress_actions;
  rtp_address_in_flight = 0;
  if(err) {
    /* An error just means that we're not using network play */
    rtp_supported = 0;
    rtp_is_running = 0;
  } else {
    rtp_supported = 1;
    rtp_is_running = rtp_running();
  }
  /*fprintf(stderr, "rtp supported->%d, running->%d\n",
          rtp_supported, rtp_is_running);*/
  if(rtp_supported != rtp_was_supported
     || rtp_is_running != rtp_was_running)
    event_raise("rtp-changed", 0);
  --suppress_actions;
}

/** @brief Called to check whether RTP play is available */
static void check_rtp_address(const char attribute((unused)) *event,
                              void attribute((unused)) *eventdata,
                              void attribute((unused)) *callbackdata) {
  if(!rtp_address_in_flight) {
    //fprintf(stderr, "checking rtp\n");
    disorder_eclient_rtp_address(client, got_rtp_address, NULL);
  }
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

static void version_completed(void attribute((unused)) *v,
                              const char attribute((unused)) *err,
                              const char *ver) {
  long major, minor, patch, dev;

  if(!ver) {
    server_version = 0;
    server_version_bytes = 0;
    return;
  }
  server_version = ver;
  server_version_bytes = 0;
  major = strtol(ver, (char **)&ver, 10);
  if(*ver != '.')
    return;
  ++ver;
  minor = strtol(ver, (char **)&ver, 10);
  if(*ver == '.') {
    ++ver;
    patch = strtol(ver, (char **)&ver, 10);
  } else
    patch = 0;
  if(*ver) {
    if(*ver == '+') {
      dev = 1;
      ++ver;
    }
    if(*ver)
      dev = 2;
  } else
    dev = 0;
  server_version_bytes = (major << 24) + (minor << 16) + (patch << 8) + dev;
}

void logged_in(void) {
  /* reset the clients */
  disorder_eclient_close(client);
  disorder_eclient_close(logclient);
  rtp_supported = 0;
  event_raise("logged-in", 0);
  /* Force the periodic checks */
  periodic_slow(0);
  periodic_fast(0);
  /* Recheck server version */
  disorder_eclient_version(client, version_completed, 0);
  disorder_eclient_enable_connect(client);
  disorder_eclient_enable_connect(logclient);
}

int main(int argc, char **argv) {
  int n;
  gboolean gtkok;

  mem_init();
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "error calling setlocale");
  gtkok = gtk_init_check(&argc, &argv);
  while((n = getopt_long(argc, argv, "hVc:dtHC", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disobedience");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 't': goesupto = 11; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(!gtkok)
    disorder_fatal(0, "failed to initialize GTK+");
  /* gcrypt initialization */
  if(!gcry_check_version(NULL))
    disorder_fatal(0, "gcry_check_version failed");
  gcry_control(GCRYCTL_INIT_SECMEM, 0);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
  signal(SIGPIPE, SIG_IGN);
  init_styles();
  load_settings();
  /* create the event loop */
  D(("create main loop"));
  mainloop = g_main_loop_new(0, 0);
  if(config_read(0, NULL)) disorder_fatal(0, "cannot read configuration");
  /* we'll need mixer support */
  backend = uaudio_default(uaudio_apis, UAUDIO_API_CLIENT);
  if(backend->configure)
    backend->configure();
  if(backend->open_mixer)
    backend->open_mixer();
  /* create the clients */
  if(!(client = gtkclient())
     || !(logclient = gtkclient()))
    return 1;                           /* already reported an error */
  /* periodic operations (e.g. expiring the cache, checking local volume) */
  g_timeout_add(600000/*milliseconds*/, periodic_slow, 0);
  g_timeout_add(1000/*milliseconds*/, periodic_fast, 0);
  make_toplevel_window();
  /* reset styles now everything has its name */
  gtk_rc_reset_styles(gtk_settings_get_for_screen(gdk_screen_get_default()));
  gtk_widget_show_all(toplevel);
  gtk_widget_hide(playing_mini);
  /* issue a NOP every so often */
  g_timeout_add_full(G_PRIORITY_LOW,
                     2000/*interval, ms*/,
                     maybe_send_nop,
                     0/*data*/,
                     0/*notify*/);
  /* Start monitoring the log */
  disorder_eclient_log(logclient, &log_callbacks, 0);
  /* Initiate all the checks */
  periodic_fast(0);
  disorder_eclient_version(client, version_completed, 0);
  event_register("log-connected", check_rtp_address, 0);
  suppress_actions = 0;
  playlists_init();
  globals_init();
  /* If no password is set yet pop up a login box */
  if(!config->password)
    login_box();
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
