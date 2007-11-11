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
/** @file disobedience/control.c
 * @brief Volume control and buttons
 */

#include "disobedience.h"

/* Forward declarations ---------------------------------------------------- */

WT(adjustment);
WT(hscale);
WT(hbox);
WT(button);
WT(image);
WT(label);
WT(vbox);

struct icon;

static void update_pause(const struct icon *);
static void update_play(const struct icon *);
static void update_scratch(const struct icon *);
static void update_random_enable(const struct icon *);
static void update_random_disable(const struct icon *);
static void update_enable(const struct icon *);
static void update_disable(const struct icon *);
static void update_rtp(const struct icon *);
static void update_nortp(const struct icon *);
static void clicked_icon(GtkButton *, gpointer);
static void clicked_menu(GtkMenuItem *, gpointer userdata);
static void toggled_menu(GtkCheckMenuItem *, gpointer userdata);

static int enable_rtp(disorder_eclient *c,
                      disorder_eclient_no_response *completed,
                      void *v);
static int disable_rtp(disorder_eclient *c,
                       disorder_eclient_no_response *completed,
                       void *v);

static double left(double v, double b);
static double right(double v, double b);
static double volume(double l, double r);
static double balance(double l, double r);

static void volume_adjusted(GtkAdjustment *a, gpointer user_data);
static gchar *format_volume(GtkScale *scale, gdouble value);
static gchar *format_balance(GtkScale *scale, gdouble value);

/* Control bar ------------------------------------------------------------- */

/** @brief Guard against feedback loop in volume control */
static int suppress_set_volume;

/** @brief Definition of an icon
 *
 * The design here is rather mad: rather than changing the image displayed by
 * icons according to their state, we flip the visibility of pairs of icons.
 */
struct icon {
  /** @brief Filename for image */
  const char *icon;

  /** @brief Text for tooltip */
  const char *tip;

  /** @brief Associated menu item or NULL */
  const char *menuitem;

  /** @brief Called to update button when state may have changed */
  void (*update)(const struct icon *i);

  /** @brief @ref eclient.h function to call */
  int (*action)(disorder_eclient *c,
                disorder_eclient_no_response *completed,
                void *v);

  /** @brief Flag */
  unsigned flags;
  
  /** @brief Pointer to button */
  GtkWidget *button;

  /** @brief Pointer to menu item */
  GtkWidget *item;
};

/** @brief This is the active half of a pair */
#define ICON_ACTIVE 0x0001

/** @brief This is the inactive half of a pair */
#define ICON_INACTIVE 0x0002

/** @brief Table of all icons */
static struct icon icons[] = {
  {
    "pause.png",                        /* icon */
    "Pause playing track",              /* tip */
    "<GdisorderMain>/Control/Playing",  /* menuitem */
    update_pause,                       /* update */
    disorder_eclient_pause,             /* action */
    ICON_ACTIVE,                        /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "play.png",                         /* icon */
    "Resume playing track",             /* tip */
    "<GdisorderMain>/Control/Playing",  /* menuitem */
    update_play,                        /* update */
    disorder_eclient_resume,            /* action */
    ICON_INACTIVE,                      /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "cross.png",                        /* icon */
    "Cancel playing track",             /* tip */
    "<GdisorderMain>/Control/Scratch",  /* menuitem */
    update_scratch,                     /* update */
    disorder_eclient_scratch_playing,   /* action */
    0,                                  /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "random.png",                       /* icon */
    "Enable random play",               /* tip */
    "<GdisorderMain>/Control/Random play", /* menuitem */
    update_random_enable,               /* update */
    disorder_eclient_random_enable,     /* action */
    ICON_INACTIVE,                      /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "randomcross.png",                  /* icon */
    "Disable random play",              /* tip */
    "<GdisorderMain>/Control/Random play", /* menuitem */
    update_random_disable,              /* update */
    disorder_eclient_random_disable,    /* action */
    ICON_ACTIVE,                        /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "notes.png",                        /* icon */
    "Enable play",                      /* tip */
    0,                                  /* menuitem */
    update_enable,                      /* update */
    disorder_eclient_enable,            /* action */
    ICON_INACTIVE,                      /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "notescross.png",                   /* icon */
    "Disable play",                     /* tip */
    0,                                  /* menuitem */
    update_disable,                     /* update */
    disorder_eclient_disable,           /* action */
    ICON_ACTIVE,                        /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "speaker.png",                      /* icon */
    "Play network stream",              /* tip */
    "<GdisorderMain>/Control/Network player", /* menuitem */
    update_rtp,                         /* update */
    enable_rtp,                         /* action */
    ICON_INACTIVE,                      /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
  {
    "speakercross.png",                 /* icon */
    "Stop playing network stream",      /* tip */
    "<GdisorderMain>/Control/Network player", /* menuitem */
    update_nortp,                       /* update */
    disable_rtp,                        /* action */
    ICON_ACTIVE,                        /* flags */
    0,                                  /* button */
    0                                   /* item */
  },
};

/** @brief Count of icons */
#define NICONS (int)(sizeof icons / sizeof *icons)

static GtkAdjustment *volume_adj;
static GtkAdjustment *balance_adj;

/** @brief Called whenever last_state changes in any way */
void control_monitor(void attribute((unused)) *u) {
  int n;

  D(("control_monitor"));
  for(n = 0; n < NICONS; ++n)
    icons[n].update(&icons[n]);
}

/** @brief Create the control bar */
GtkWidget *control_widget(void) {
  GtkWidget *hbox = gtk_hbox_new(FALSE, 1), *vbox;
  GtkWidget *v, *b;
  int n;

  NW(hbox);
  D(("control_widget"));
  assert(mainmenufactory);              /* ordering must be right */
  for(n = 0; n < NICONS; ++n) {
    NW(button);
    icons[n].button = iconbutton(icons[n].icon, icons[n].tip);
    g_signal_connect(G_OBJECT(icons[n].button), "clicked",
                     G_CALLBACK(clicked_icon), &icons[n]);
    /* pop the icon in a vbox so it doesn't get vertically stretch if there are
     * taller things in the control bar */
    NW(vbox);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), icons[n].button, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    if(icons[n].menuitem) {
      icons[n].item = gtk_item_factory_get_widget(mainmenufactory,
                                                  icons[n].menuitem);
      switch(icons[n].flags & (ICON_ACTIVE|ICON_INACTIVE)) {
      case ICON_ACTIVE:
        g_signal_connect(G_OBJECT(icons[n].item), "toggled",
                         G_CALLBACK(toggled_menu), &icons[n]);
        break;
      case ICON_INACTIVE:
        /* Don't connect two instances of the signal! */
        break;
      default:
        g_signal_connect(G_OBJECT(icons[n].item), "activate",
                         G_CALLBACK(clicked_menu), &icons[n]);
        break;
      }
    }
  }
  /* create the adjustments for the volume control */
  NW(adjustment);
  volume_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, goesupto,
                                                 goesupto / 20, goesupto / 20,
                                                 0));
  NW(adjustment);
  balance_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -1, 1,
                                                  0.2, 0.2, 0));
  /* the volume control */
  NW(hscale);
  v = gtk_hscale_new(volume_adj);
  NW(hscale);
  b = gtk_hscale_new(balance_adj);
  gtk_widget_set_style(v, tool_style);
  gtk_widget_set_style(b, tool_style);
  gtk_scale_set_digits(GTK_SCALE(v), 10);
  gtk_scale_set_digits(GTK_SCALE(b), 10);
  gtk_widget_set_size_request(v, 192, -1);
  gtk_widget_set_size_request(b, 192, -1);
  gtk_tooltips_set_tip(tips, v, "Volume", "");
  gtk_tooltips_set_tip(tips, b, "Balance", "");
  gtk_box_pack_start(GTK_BOX(hbox), v, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), b, FALSE, TRUE, 0);
  /* space updates rather than hammering the server */
  gtk_range_set_update_policy(GTK_RANGE(v), GTK_UPDATE_DELAYED);
  gtk_range_set_update_policy(GTK_RANGE(b), GTK_UPDATE_DELAYED);
  /* notice when the adjustments are changed */
  g_signal_connect(G_OBJECT(volume_adj), "value-changed",
                   G_CALLBACK(volume_adjusted), 0);
  g_signal_connect(G_OBJECT(balance_adj), "value-changed",
                   G_CALLBACK(volume_adjusted), 0);
  /* format the volume/balance values ourselves */
  g_signal_connect(G_OBJECT(v), "format-value",
                   G_CALLBACK(format_volume), 0);
  g_signal_connect(G_OBJECT(b), "format-value",
                   G_CALLBACK(format_balance), 0);
  register_monitor(control_monitor, 0, -1UL);
  return hbox;
}

/** @brief Update the volume control when it changes */
void volume_update(void) {
  double l, r;

  D(("volume_update"));
  l = volume_l / 100.0;
  r = volume_r / 100.0;
  ++suppress_set_volume;
  gtk_adjustment_set_value(volume_adj, volume(l, r) * goesupto);
  gtk_adjustment_set_value(balance_adj, balance(l, r));
  --suppress_set_volume;
}

/** @brief Update the state of one of the control icons
 * @param icon Target icon
 * @param visible True if this version of the button should be visible
 * @param usable True if the button is currently usable
 *
 * Several of the icons, rather bizarrely, come in pairs: for instance exactly
 * one of the play and pause buttons is supposed to be visible at any given
 * moment.
 *
 * @p usable need not take into account server availability, that is done
 * automatically.
 */
static void update_icon(const struct icon *icon,
                        int visible, int usable) {
  /* If the connection is down nothing is ever usable */
  if(!(last_state & DISORDER_CONNECTED))
    usable = 0;
  (visible ? gtk_widget_show : gtk_widget_hide)(icon->button);
  /* Only both updating usability if the button is visible */
  if(visible)
    gtk_widget_set_sensitive(icon->button, usable);
  if(icon->item) {
    /* There's an associated menu item.  These are always visible, but may not
     * be usable. */
    if((icon->flags & (ICON_ACTIVE|ICON_INACTIVE)) == ICON_ACTIVE) {
      /* The active half of a pair */
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(icon->item), visible);
    }
    gtk_widget_set_sensitive(icon->item, usable);
  }
}

static void update_pause(const struct icon *icon) {
  const int visible = !(last_state & DISORDER_TRACK_PAUSED);
  const int usable = !!(last_state & DISORDER_PLAYING); /* TODO: might be a lie */
  update_icon(icon, visible, usable);
}

static void update_play(const struct icon *icon) {
  const int visible = !!(last_state & DISORDER_TRACK_PAUSED);
  const int usable = !!(last_state & DISORDER_PLAYING);
  update_icon(icon, visible, usable);
}

static void update_scratch(const struct icon *icon) {
  const int visible = 1;
  const int usable = !!(last_state & DISORDER_PLAYING);
  update_icon(icon, visible, usable);
}

static void update_random_enable(const struct icon *icon) {
  const int visible = !(last_state & DISORDER_RANDOM_ENABLED);
  const int usable = 1;
  update_icon(icon, visible, usable);
}

static void update_random_disable(const struct icon *icon) {
  const int visible = !!(last_state & DISORDER_RANDOM_ENABLED);
  const int usable = 1;
  update_icon(icon, visible, usable);
}

static void update_enable(const struct icon *icon) {
  const int visible = !(last_state & DISORDER_PLAYING_ENABLED);
  const int usable = 1;
  update_icon(icon, visible, usable);
}

static void update_disable(const struct icon *icon) {
  const int visible = !!(last_state & DISORDER_PLAYING_ENABLED);
  const int usable = 1;
  update_icon(icon, visible, usable);
}

static void update_rtp(const struct icon *icon) {
  const int visible = !rtp_is_running;
  const int usable = rtp_supported;
  update_icon(icon, visible, usable);
}

static void update_nortp(const struct icon *icon) {
  const int visible = rtp_is_running;
  const int usable = rtp_supported;
  update_icon(icon, visible, usable);
}

static void clicked_icon(GtkButton attribute((unused)) *button,
                         gpointer userdata) {
  const struct icon *icon = userdata;

  icon->action(client, 0, 0);
}

static void clicked_menu(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer userdata) {
  const struct icon *icon = userdata;

  icon->action(client, 0, 0);
}

static void toggled_menu(GtkCheckMenuItem *menuitem,
                         gpointer userdata) {
  const struct icon *icon = userdata;
  size_t n;

  /* This is a bit fiddlier than the others, we need to find the action for the
   * new state.  If the new state is active then we want the ICON_INACTIVE
   * version and vica versa. */
  for(n = 0; n < NICONS; ++n)
    if(icons[n].item == icon->item
       && !!(icons[n].flags & ICON_INACTIVE) == !!menuitem->active)
      break;
  if(n < NICONS)
    icons[n].action(client, 0, 0);
}

/** @brief Called when the volume has been adjusted */
static void volume_adjusted(GtkAdjustment attribute((unused)) *a,
                            gpointer attribute((unused)) user_data) {
  double v = gtk_adjustment_get_value(volume_adj) / goesupto;
  double b = gtk_adjustment_get_value(balance_adj);

  if(suppress_set_volume)
    /* This is the result of an update from the server, not a change from the
     * user.  Don't feedback! */
    return;
  D(("volume_adjusted"));
  /* force to 'stereotypical' values */
  v = nearbyint(100 * v) / 100;
  b = nearbyint(5 * b) / 5;
  /* Set the volume.  We don't want a reply, we'll get the actual new volume
   * from the log. */
  disorder_eclient_volume(client, 0,
                          nearbyint(left(v, b) * 100),
                          nearbyint(right(v, b) * 100),
                          0);
}

/** @brief Formats the volume value */
static gchar *format_volume(GtkScale attribute((unused)) *scale,
                            gdouble value) {
  char s[32];

  snprintf(s, sizeof s, "%.1f", (double)value);
  return g_strdup(s);
}

/** @brief Formats the balance value */
static gchar *format_balance(GtkScale attribute((unused)) *scale,
                             gdouble value) {
  char s[32];

  if(fabs(value) < 0.1)
    return g_strdup("0");
  snprintf(s, sizeof s, "%+.1f", (double)value);
  return g_strdup(s);
}

/* Volume mapping.  We consider left, right, volume to be in [0,1]
 * and balance to be in [-1,1].
 * 
 * First, we just have volume = max(left, right).
 *
 * Balance we consider to linearly represent the amount by which the quieter
 * channel differs from the louder.  In detail:
 *
 *  if right > left then balance > 0:
 *   balance = 0 => left = right  (as an endpoint, not an instance)
 *   balance = 1 => left = 0
 *   fitting to linear, left = right * (1 - balance)
 *                so balance = 1 - left / right
 *   (right > left => right > 0 so no division by 0.)
 * 
 *  if left > right then balance < 0:
 *   balance = 0 => right = left  (same caveat as above)
 *   balance = -1 => right = 0
 *   again fitting to linear, right = left * (1 + balance)
 *                       so balance = right / left - 1
 *   (left > right => left > 0 so no division by 0.)
 *
 *  if left = right then we just have balance = 0.
 *
 * Thanks to Clive and Andrew.
 */

/** @brief Return the greater of @p x and @p y */
static double max(double x, double y) {
  return x > y ? x : y;
}

/** @brief Compute the left channel volume */
static double left(double v, double b) {
  if(b > 0)                             /* volume = right */
    return v * (1 - b);
  else                                  /* volume = left */
    return v;
}

/** @brief Compute the right channel volume */
static double right(double v, double b) {
  if(b > 0)                             /* volume = right */
    return v;
  else                                  /* volume = left */
    return v * (1 + b);
}

/** @brief Compute the overall volume */
static double volume(double l, double r) {
  return max(l, r);
}

/** @brief Compute the balance */
static double balance(double l, double r) {
  if(l > r)
    return r / l - 1;
  else if(r > l)
    return 1 - l / r;
  else                                  /* left = right */
    return 0;
}

/** @brief Called to enable RTP play
 *
 * Rather odd signature is to fit in with the other icons which all call @ref
 * lib/eclient.h functions.
 */
static int enable_rtp(disorder_eclient attribute((unused)) *c,
                      disorder_eclient_no_response attribute((unused)) *completed,
                      void attribute((unused)) *v) {
  start_rtp();
  return 0;
}

/** @brief Called to disable RTP play
 *
 * Rather odd signature is to fit in with the other icons which all call @ref
 * lib/eclient.h functions.
 */
static int disable_rtp(disorder_eclient attribute((unused)) *c,
                       disorder_eclient_no_response attribute((unused)) *completed,
                       void attribute((unused)) *v) {
  stop_rtp();
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
