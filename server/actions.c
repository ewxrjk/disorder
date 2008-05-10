/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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

#include <config.h>
#include "types.h"

/** @brief Login cookie */
char *login_cookie;

/** @brief Table of actions */
static const struct action {
  /** @brief Action name */
  const char *name;
  /** @brief Action handler */
  void (*handler)(void);
} actions[] = {
  { "confirm", act_confirm },
  { "disable", act_disable },
  { "edituser", act_edituser },
  { "enable", act_enable },
  { "login", act_login },
  { "logout", act_logout },
  { "manage", act_manage },
  { "move", act_move },
  { "pause", act_pause },
  { "play", act_play },
  { "playing", act_playing },
  { "prefs", act_prefs },
  { "random-disable", act_random_disable },
  { "random-enable", act_random_enable },
  { "register", act_register },
  { "reminder", act_reminder },
  { "remove", act_remove },
  { "resume", act_resume },
  { "scratch", act_scratch },
  { "volume", act_volume },
};

/** @brief Expand a template
 * @param name Base name of template, or NULL to consult CGI args
 */
void disorder_cgi_expand(const char *name) {
  const char *p;
  
  /* For unknown actions check that they aren't evil */
  for(p = name; *p && isalnum((unsigned char)*p); ++p)
    ;
  if(*p)
    fatal(0, "invalid action name '%s'", action);
  byte_xasprintf((char **)&p, "%s.tmpl", action);
  if(mx_expand_file(p, sink_stdio(stdout), 0) == -1
     || fflush(stdout) < 0)
    fatal(errno, "error writing to stdout");
}

/** @brief Execute a web action
 * @param action Action to perform, or NULL to consult CGI args
 *
 * If no recognized action is specified then 'playing' is assumed.
 */
void disorder_cgi_action(const char *action) {
  int n;
  char *s;

  /* Consult CGI args if caller had no view */
  if(!action)
    action = cgi_get("action");
  /* Pick a default if nobody cares at all */
  if(!action) {
    /* We allow URLs which are just c=... in order to keep confirmation URLs,
     * which are user-facing, as short as possible.  Actually we could lose the
     * c= for this... */
    if(cgi_get("c"))
      action = "confirm";
    else
      action = "playing";
  }
  if((n = TABLE_FIND(actions, struct action, name, action)) >= 0)
    /* Its a known action */
    actions[n].handler();
  else
    /* Just expand the template */
    disorder_cgi_expand(action);
}

/** @brief Generate an error page */
void disorder_cgi_error(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  byte_xvasprintf(&error_string, msg, ap);
  va_end(ap);
  disorder_cgi_expand("error");
}

/** @brief Log in as the current user or guest if none */
void disorder_cgi_login(dcgi_state *ds, struct sink *output) {
  /* Junk old data */
  disorder_macros_reset();
  /* Reconnect */
  if(disorder_connect_cookie(client, login_cookie)) {
    disorder_cgi_error("Cannot connect to server");
    exit(0);
  }
  /* If there was a cookie but it went bad, we forget it */
  if(login_cookie && !strcmp(disorder_user(>client), "guest"))
    login_cookie = 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
