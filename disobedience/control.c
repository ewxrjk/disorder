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
/** @file disobedience/control.c
 * @brief Volume control and buttons
 */

#include "disobedience.h"

/* Forward declarations ---------------------------------------------------- */

struct icon;

static void clicked_icon(GtkToolButton *, gpointer);
static void toggled_icon(GtkToggleToolButton *button,
                         gpointer user_data);
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

static void icon_changed(const char *event,
                         void *evendata,
                         void *callbackdata);
static void volume_changed(const char *event,
                           void *eventdata,
                           void *callbackdata);
static void control_minimode(const char *event,
                             void *eventdata,
                             void *callbackdata);

/* Control bar ------------------------------------------------------------- */

/** @brief Guard against feedback */
int suppress_actions = 1;

/** @brief Toolbar widget */
static GtkWidget *toolbar;

/** @brief Definition of an icon
 *
 * We have two kinds of icon:
 * - action icons, which just do something but don't have a state as such
 * - toggle icons, which toggle between two states ("on" and "off").
 *
 * The scratch button is an action icon; currently all the others are toggle
 * icons.
 *
 * (All icons can be sensitive or insensitive, separately to the above.)
 */
struct icon {
  /** @brief TRUE to use GTK+ stock icons instead of filenames */
  gboolean stock;

  /** @brief TRUE for toggle buttons, FALSE for action buttons */
  gboolean toggle;
  
  /** @brief Filename for image or stock string */
  const char *icon;

  /** @brief Text for 'on' tooltip */
  const char *tip_on;

  /** @brief Text for 'off' tooltip */
  const char *tip_off;

  /** @brief Associated menu item or NULL */
  const char *menuitem;

  /** @brief Label text */
  const char *label;

  /** @brief Events that change this icon, separated by spaces */
  const char *events;

  /** @brief @ref eclient.h function to call to go from off to on
   *
   * For action buttons, this should be NULL.
   */
  int (*action_go_on)(disorder_eclient *c,
                      disorder_eclient_no_response *completed,
                      void *v);

  /** @brief @ref eclient.h function to call to go from on to off
   *
   * For action buttons, this action is used.
   */
  int (*action_go_off)(disorder_eclient *c,
                       disorder_eclient_no_response *completed,
                       void *v);

  /** @brief Get button state
   * @return 1 for on, 0 for off
   */
  int (*on)(void);

  /** @brief Get button sensitivity
   * @return 1 for sensitive, 0 for insensitive
   *
   * Can be NULL for always sensitive.
   */
  int (*sensitive)(void);

  /** @brief True if the menu item has inverse sense to the button */
  gboolean menu_invert;
  
  /** @brief Pointer to button */
  GtkWidget *button;

  /** @brief Pointer to menu item */
  GtkWidget *item;

  GtkWidget *image;
};

static int pause_resume_on(void) {
  return !!(last_state & DISORDER_TRACK_PAUSED);
}

static int pause_resume_sensitive(void) {
  return playing_track
    && !!(last_state & DISORDER_PLAYING)
    && (last_rights & RIGHT_PAUSE);
}

static int scratch_sensitive(void) {
  return !!(last_state & DISORDER_PLAYING)
    && right_scratchable(last_rights, config->username, playing_track);
}

static int random_sensitive(void) {
  return !!(last_rights & RIGHT_GLOBAL_PREFS);
}

static int random_enabled(void) {
  return !!(last_state & DISORDER_RANDOM_ENABLED);
}

static int playing_sensitive(void) {
  return !!(last_rights & RIGHT_GLOBAL_PREFS);
}

static int playing_enabled(void) {
  return !!(last_state & DISORDER_PLAYING_ENABLED);
}

static int rtp_enabled(void) {
  return rtp_is_running;
}

static int rtp_sensitive(void) {
  return rtp_supported;
}

/** @brief Table of all icons */
static struct icon icons[] = {
  {
    .toggle = TRUE,
    .stock = TRUE,
    .icon = GTK_STOCK_MEDIA_PAUSE,
    .label = "Pause",
    .tip_on = "Resume playing track",
    .tip_off = "Pause playing track",
    .menuitem = "<GdisorderMain>/Control/Playing",
    .on = pause_resume_on,
    .sensitive = pause_resume_sensitive,
    .action_go_on = disorder_eclient_pause,
    .action_go_off = disorder_eclient_resume,
    .events = "pause-changed playing-changed rights-changed playing-track-changed",
    .menu_invert = TRUE,
  },
  {
    .stock = TRUE,
    .icon = GTK_STOCK_STOP,
    .label = "Scratch",
    .tip_on = "Cancel playing track",
    .menuitem = "<GdisorderMain>/Control/Scratch",
    .sensitive = scratch_sensitive,
    .action_go_off = disorder_eclient_scratch_playing,
    .events = "playing-track-changed rights-changed",
  },
  {
    .toggle = TRUE,
    .stock = FALSE,
    .icon = "cards24.png",
    .label = "Random",
    .tip_on = "Disable random play",
    .tip_off = "Enable random play",
    .menuitem = "<GdisorderMain>/Control/Random play",
    .on = random_enabled,
    .sensitive = random_sensitive,
    .action_go_on = disorder_eclient_random_enable,
    .action_go_off = disorder_eclient_random_disable,
    .events = "random-changed rights-changed",
  },
  {
    .toggle = TRUE,
    .stock = TRUE,
    .icon = GTK_STOCK_MEDIA_PLAY,
    .label = "Play",
    .tip_on = "Disable play",
    .tip_off = "Enable play",
    .on = playing_enabled,
    .sensitive = playing_sensitive,
    .action_go_on = disorder_eclient_enable,
    .action_go_off = disorder_eclient_disable,
    .events = "enabled-changed rights-changed",
  },
  {
    .toggle = TRUE,
    .stock = TRUE,
    .icon = GTK_STOCK_CONNECT,
    .label = "RTP",
    .tip_on = "Stop playing network stream",
    .tip_off = "Play network stream",
    .menuitem = "<GdisorderMain>/Control/Network player",
    .on = rtp_enabled,
    .sensitive = rtp_sensitive,
    .action_go_on = enable_rtp,
    .action_go_off = disable_rtp,
    .events = "rtp-changed",
  },
};

/** @brief Count of icons */
#define NICONS (int)(sizeof icons / sizeof *icons)

static GtkAdjustment *volume_adj;
static GtkAdjustment *balance_adj;
static GtkWidget *volume_widget;
static GtkWidget *balance_widget;

/** @brief Create the control bar */
GtkWidget *control_widget(void) {
  GtkWidget *hbox = gtk_hbox_new(FALSE, 1);
  int n;

  D(("control_widget"));
  assert(mainmenufactory);              /* ordering must be right */
  toolbar = gtk_toolbar_new();
  /* Don't permit overflow arrow as otherwise the toolbar isn't greedy enough
   * in asking for space.  The ideal is probably to make the volume and balance
   * sliders hang down from the toolbar so it unavoidably gets the whole width
   * of the window to play with. */
  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), 
                        full_mode ? GTK_TOOLBAR_BOTH : GTK_TOOLBAR_ICONS);
  for(n = 0; n < NICONS; ++n) {
    struct icon *const icon = &icons[n];
    icon->button = (icon->toggle
                    ? GTK_WIDGET(gtk_toggle_tool_button_new())
                    : GTK_WIDGET(gtk_tool_button_new(NULL, NULL)));
    gtk_widget_set_style(icons[n].button, tool_style);
    if(icons[n].stock) {
      /* We'll use the stock icons for this one */
      icon->image = gtk_image_new_from_stock(icons[n].icon,
                                             GTK_ICON_SIZE_LARGE_TOOLBAR);
    } else {
      /* Create the 'on' image */
      icon->image = gtk_image_new_from_pixbuf(find_image(icons[n].icon));
    }
    assert(icon->image);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(icon->button),
                                    icon->image);
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(icon->button),
                                    icon->label);
    if(icon->toggle)
      g_signal_connect(G_OBJECT(icon->button), "toggled",
                       G_CALLBACK(toggled_icon), icon);
    else
      g_signal_connect(G_OBJECT(icon->button), "clicked",
                       G_CALLBACK(clicked_icon), icon);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                       GTK_TOOL_ITEM(icon->button),
                       -1);
    if(icons[n].menuitem) {
      /* Find the menu item */
      icons[n].item = gtk_item_factory_get_widget(mainmenufactory,
                                                  icons[n].menuitem);
      if(icon->toggle)
        g_signal_connect(G_OBJECT(icons[n].item), "toggled",
                         G_CALLBACK(toggled_menu), &icons[n]);
      else
        g_signal_connect(G_OBJECT(icons[n].item), "activate",
                         G_CALLBACK(clicked_menu), &icons[n]);
    }
    /* Make sure the icon is updated when relevant things changed */
    char **events = split(icons[n].events, 0, 0, 0, 0);
    while(*events)
      event_register(*events++, icon_changed, &icons[n]);
    event_register("connected-changed", icon_changed, &icons[n]);
  }
  /* create the adjustments for the volume control */
  volume_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, goesupto,
                                                 goesupto / 20, goesupto / 20,
                                                 0));
  balance_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -1, 1,
                                                  0.2, 0.2, 0));
  /* the volume control */
  volume_widget = gtk_hscale_new(volume_adj);
  balance_widget = gtk_hscale_new(balance_adj);
  gtk_widget_set_style(volume_widget, tool_style);
  gtk_widget_set_style(balance_widget, tool_style);
  gtk_scale_set_digits(GTK_SCALE(volume_widget), 10);
  gtk_scale_set_digits(GTK_SCALE(balance_widget), 10);
  gtk_widget_set_size_request(volume_widget, 128, -1);
  gtk_widget_set_size_request(balance_widget, 128, -1);
  gtk_widget_set_tooltip_text(volume_widget, "Volume");
  gtk_widget_set_tooltip_text(balance_widget, "Balance");
  gtk_box_pack_start(GTK_BOX(hbox), toolbar,
                     FALSE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(hbox), volume_widget,
                     FALSE/*expand*/, TRUE/*fill*/, 0);
  gtk_box_pack_start(GTK_BOX(hbox), balance_widget,
                     FALSE/*expand*/, TRUE/*fill*/, 0);
  /* space updates rather than hammering the server */
  gtk_range_set_update_policy(GTK_RANGE(volume_widget), GTK_UPDATE_DELAYED);
  gtk_range_set_update_policy(GTK_RANGE(balance_widget), GTK_UPDATE_DELAYED);
  /* notice when the adjustments are changed */
  g_signal_connect(G_OBJECT(volume_adj), "value-changed",
                   G_CALLBACK(volume_adjusted), 0);
  g_signal_connect(G_OBJECT(balance_adj), "value-changed",
                   G_CALLBACK(volume_adjusted), 0);
  /* format the volume/balance values ourselves */
  g_signal_connect(G_OBJECT(volume_widget), "format-value",
                   G_CALLBACK(format_volume), 0);
  g_signal_connect(G_OBJECT(balance_widget), "format-value",
                   G_CALLBACK(format_balance), 0);
  event_register("volume-changed", volume_changed, 0);
  event_register("rtp-changed", volume_changed, 0);
  event_register("mini-mode-changed", control_minimode, 0);
  return hbox;
}

/** @brief Return TRUE if volume setting is supported */
static int volume_supported(void) {
  /* TODO: if the server doesn't know how to set the volume [but isn't using
   * network play] then we should have volume_supported = FALSE */
  return 1;
}

/** @brief Update the volume control when it changes */
static void volume_changed(const char attribute((unused)) *event,
                           void attribute((unused)) *eventdata,
                           void attribute((unused)) *callbackdata) {
  double l, r;

  D(("volume_changed"));
  ++suppress_actions;
  /* Only display volume/balance controls if they will work */
  if(volume_supported()) {
    gtk_widget_show(volume_widget);
    if(full_mode)
      gtk_widget_show(balance_widget);
    l = volume_l / 100.0;
    r = volume_r / 100.0;
    gtk_adjustment_set_value(volume_adj, volume(l, r) * goesupto);
    gtk_adjustment_set_value(balance_adj, balance(l, r));
  } else {
    gtk_widget_hide(volume_widget);
    gtk_widget_hide(balance_widget);
  }
  --suppress_actions;
}

/** @brief Update the state of one of the control icons
 */
static void icon_changed(const char attribute((unused)) *event,
                         void attribute((unused)) *evendata,
                         void *callbackdata) {
  //fprintf(stderr, "icon_changed (%s)\n", event);
  const struct icon *const icon = callbackdata;
  int on = icon->on ? icon->on() : 1;
  int sensitive = icon->sensitive ? icon->sensitive() : 1;
  //fprintf(stderr, "sensitive->%d\n", sensitive);

  ++suppress_actions;
  /* If the connection is down nothing is ever usable */
  if(!(last_state & DISORDER_CONNECTED))
    sensitive = 0;
  if(icon->toggle)
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(icon->button),
                                      on);
  /* If you disable play or random play NOT via the icon (for instance, via the
   * edit menu or via a completely separate command line invocation) then the
   * icon shows up as insensitive.  Hover the mouse over it and the correct
   * state is immediately displayed.  sensitive and GTK_WIDGET_SENSITIVE show
   * it to be in the correct state, so I think this is may be a GTK+ bug. */
  if(icon->tip_on)
    gtk_widget_set_tooltip_text(icon->button,
                                on ? icon->tip_on : icon->tip_off);
  gtk_widget_set_sensitive(icon->button, sensitive);
  /* Icons with an associated menu item */
  if(icon->item) {
    if(icon->toggle)
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(icon->item),
                                     !!icon->menu_invert ^ !!on);
    gtk_widget_set_sensitive(icon->item, sensitive);
  }
  --suppress_actions;
}

static void icon_action_completed(void attribute((unused)) *v,
                                  const char *err) {
  if(err)
    popup_protocol_error(0, err);
}

static void clicked_icon(GtkToolButton attribute((unused)) *button,
                         gpointer userdata) {
  const struct icon *icon = userdata;

  if(suppress_actions)
    return;
  icon->action_go_off(client, icon_action_completed, 0);
}

static void toggled_icon(GtkToggleToolButton attribute((unused)) *button,
                         gpointer user_data) {
  const struct icon *icon = user_data;

  if(suppress_actions)
    return;
  if(icon->on())
    icon->action_go_off(client, icon_action_completed, 0);
  else
    icon->action_go_on(client, icon_action_completed, 0);
}

static void clicked_menu(GtkMenuItem attribute((unused)) *menuitem,
                         gpointer userdata) {
  clicked_icon(NULL, userdata);
}

static void toggled_menu(GtkCheckMenuItem attribute((unused)) *menuitem,
                         gpointer userdata) {
  toggled_icon(NULL, userdata);
}

/** @brief Called when a volume command completes */
static void volume_completed(void attribute((unused)) *v,
                             const char *err) {
  if(err)
    popup_protocol_error(0, err);
  /* We don't set the UI's notion of the volume here, it is set from the log
   * regardless of the reason it changed */
}

/** @brief Called when the volume has been adjusted */
static void volume_adjusted(GtkAdjustment attribute((unused)) *a,
                            gpointer attribute((unused)) user_data) {
  double v = gtk_adjustment_get_value(volume_adj) / goesupto;
  double b = gtk_adjustment_get_value(balance_adj);

  if(suppress_actions)
    /* This is the result of an update from the server, not a change from the
     * user.  Don't feedback! */
    return;
  D(("volume_adjusted"));
  /* force to 'stereotypical' values */
  v = nearbyint(100 * v) / 100;
  b = nearbyint(5 * b) / 5;
  /* Set the volume.  We don't want a reply, we'll get the actual new volume
   * from the log. */
  if(rtp_supported) {
    int l = nearbyint(left(v, b) * 100), r = nearbyint(right(v, b) * 100);
    rtp_setvol(&l, &r);
  } else
    disorder_eclient_set_volume(client, volume_completed,
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

static void control_minimode(const char attribute((unused)) *event,
                             void attribute((unused)) *evendata,
                             void attribute((unused)) *callbackdata) {
  if(full_mode && volume_supported()) {
    gtk_widget_show(balance_widget);
    gtk_scale_set_value_pos(GTK_SCALE(volume_widget), GTK_POS_TOP);
  } else {
    gtk_widget_hide(balance_widget);
    gtk_scale_set_value_pos(GTK_SCALE(volume_widget), GTK_POS_RIGHT);
  }
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), 
                        full_mode ? GTK_TOOLBAR_BOTH : GTK_TOOLBAR_ICONS);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
