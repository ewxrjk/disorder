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
/** @file server/actions.c
 * @brief DisOrder web actions
 *
 * Actions are anything that the web interface does beyond passive template
 * expansion and inspection of state recieved from the server.  This means
 * playing tracks, editing prefs etc but also setting extra headers e.g. to
 * auto-refresh the playing list.
 */

#include "disorder-cgi.h"

/** @brief Redirect to some other action or URL */
static void redirect(const char *url) {
  /* By default use the 'back' argument */
  if(!url)
    url = cgi_get("back");
  if(url && *url) {
    if(strncmp(url, "http", 4))
      /* If the target is not a full URL assume it's the action */
      url = cgi_makeurl(config->url, "action", url, (char *)0);
  } else {
    /* If back= is not set just go back to the front page */
    url = config->url;
  }
  if(printf("Location: %s\n"
            "%s\n"
            "\n", url, dcgi_cookie_header()) < 0)
    fatal(errno, "error writing to stdout");
}

/*! playing
 *
 * Expands \fIplaying.tmpl\fR as if there was no special 'playing' action, but
 * adds a Refresh: field to the HTTP header.  The maximum refresh interval is
 * defined by \fBrefresh\fR (see \fBdisorder_config\fR(5)) but may be less if
 * the end of the track is near.
 */
/*! manage
 *
 * Expands \fIplaying.tmpl\fR (NB not \fImanage.tmpl\fR) as if there was no
 * special 'playing' action, and adds a Refresh: field to the HTTP header.  The
 * maximum refresh interval is defined by \Bfrefresh\fR (see
 * \fBdisorder_config\fR(5)) but may be less if the end of the track is near.
 */
static void act_playing(void) {
  long refresh = config->refresh;
  long length;
  time_t now, fin;
  char *url;
  const char *action;

  dcgi_lookup(DCGI_PLAYING|DCGI_QUEUE|DCGI_ENABLED|DCGI_RANDOM_ENABLED);
  if(dcgi_playing
     && dcgi_playing->state == playing_started /* i.e. not paused */
     && !disorder_length(dcgi_client, dcgi_playing->track, &length)
     && length
     && dcgi_playing->sofar >= 0) {
    /* Try to put the next refresh at the start of the next track. */
    time(&now);
    fin = now + length - dcgi_playing->sofar + config->gap;
    if(now + refresh > fin)
      refresh = fin - now;
  }
  if(dcgi_queue && dcgi_queue->state == playing_isscratch) {
    /* next track is a scratch, don't leave more than the inter-track gap */
    if(refresh > config->gap)
      refresh = config->gap;
  }
  if(!dcgi_playing
     && ((dcgi_queue
          && dcgi_queue->state != playing_random)
         || dcgi_random_enabled)
     && dcgi_enabled) {
    /* no track playing but playing is enabled and there is something coming
     * up, must be in a gap */
    if(refresh > config->gap)
      refresh = config->gap;
  }
  if((action = cgi_get("action")))
    url = cgi_makeurl(config->url, "action", action, (char *)0);
  else
    url = config->url;
  if(printf("Refresh: %ld;url=%s\n",
            refresh, url) < 0)
    fatal(errno, "error writing to stdout");
  dcgi_expand("playing", 1);
}

/*! disable
 *
 * Disables play.
 */
static void act_disable(void) {
  if(dcgi_client)
    disorder_disable(dcgi_client);
  redirect(0);
}

/*! enable
 *
 * Enables play.
 */
static void act_enable(void) {
  if(dcgi_client)
    disorder_enable(dcgi_client);
  redirect(0);
}

/*! random-disable
 *
 * Disables random play.
 */
static void act_random_disable(void) {
  if(dcgi_client)
    disorder_random_disable(dcgi_client);
  redirect(0);
}

/*! random-enable
 *
 * Enables random play.
 */
static void act_random_enable(void) {
  if(dcgi_client)
    disorder_random_enable(dcgi_client);
  redirect(0);
}

/*! pause
 *
 * Pauses the current track (if there is one and it's not paused already).
 */
static void act_pause(void) {
  if(dcgi_client)
    disorder_pause(dcgi_client);
  redirect(0);
}

/*! resume
 *
 * Resumes the current track (if there is one and it's paused).
 */
static void act_resume(void) {
  if(dcgi_client)
    disorder_resume(dcgi_client);
  redirect(0);
}

/*! remove
 *
 * Removes the track given by the \fBid\fR argument.  If this is the currently
 * playing track then it is scratched.
 */
static void act_remove(void) {
  const char *id;
  struct queue_entry *q;

  if(dcgi_client) {
    if(!(id = cgi_get("id")))
      error(0, "missing 'id' argument");
    else if(!(q = dcgi_findtrack(id)))
      error(0, "unknown queue id %s", id);
    else switch(q->state) {
    case playing_isscratch:
    case playing_failed:
    case playing_no_player:
    case playing_ok:
    case playing_quitting:
    case playing_scratched:
      error(0, "does not make sense to scratch %s", id);
      break;
    case playing_paused:                /* started but paused */
    case playing_started:               /* started to play */
      disorder_scratch(dcgi_client, id);
      break;
    case playing_random:                /* unplayed randomly chosen track */
    case playing_unplayed:              /* haven't played this track yet */
      disorder_remove(dcgi_client, id);
      break;
    }
  }
  redirect(0);
}

/*! move
 *
 * Moves the track given by the \fBid\fR argument the distance given by the
 * \fBdelta\fR argument.  If this is positive the track is moved earlier in the
 * queue and if negative, later.
 */
static void act_move(void) {
  const char *id, *delta;
  struct queue_entry *q;

  if(dcgi_client) {
    if(!(id = cgi_get("id")))
      error(0, "missing 'id' argument");
    else if(!(delta = cgi_get("delta")))
      error(0, "missing 'delta' argument");
    else if(!(q = dcgi_findtrack(id)))
      error(0, "unknown queue id %s", id);
    else switch(q->state) {
    case playing_random:                /* unplayed randomly chosen track */
    case playing_unplayed:              /* haven't played this track yet */
      disorder_move(dcgi_client, id, atol(delta));
      break;
    default:
      error(0, "does not make sense to scratch %s", id);
      break;
    }
  }
  redirect(0);
}

/*! play
 *
 * Play the track given by the \fBtrack\fR argument, or if that is not set all
 * the tracks in the directory given by the \fBdir\fR argument.
 */
static void act_play(void) {
  const char *track, *dir;
  char **tracks;
  int ntracks, n;
  struct dcgi_entry *e;
  
  if(dcgi_client) {
    if((track = cgi_get("track"))) {
      disorder_play(dcgi_client, track);
    } else if((dir = cgi_get("dir"))) {
      if(disorder_files(dcgi_client, dir, 0, &tracks, &ntracks))
        ntracks = 0;
      /* TODO use tracksort_init */
      e = xmalloc(ntracks * sizeof (struct dcgi_entry));
      for(n = 0; n < ntracks; ++n) {
        e[n].track = tracks[n];
        e[n].sort = trackname_transform("track", tracks[n], "sort");
        e[n].display = trackname_transform("track", tracks[n], "display");
      }
      qsort(e, ntracks, sizeof (struct dcgi_entry), dcgi_compare_entry);
      for(n = 0; n < ntracks; ++n)
        disorder_play(dcgi_client, e[n].track);
    }
  }
  redirect(0);
}

static int clamp(int n, int min, int max) {
  if(n < min)
    return min;
  if(n > max)
    return max;
  return n;
}

/*! volume
 *
 * If the \fBdelta\fR argument is set: adjust both channels by that amount (up
 * if positive, down if negative).
 *
 * Otherwise if \fBleft\fR and \fBright\fR are set, set the channels
 * independently to those values.
 */
static void act_volume(void) {
  const char *l, *r, *d;
  int nd;

  if(dcgi_client) {
    if((d = cgi_get("delta"))) {
      dcgi_lookup(DCGI_VOLUME);
      nd = clamp(atoi(d), -255, 255);
      disorder_set_volume(dcgi_client,
                          clamp(dcgi_volume_left + nd, 0, 255),
                          clamp(dcgi_volume_right + nd, 0, 255));
    } else if((l = cgi_get("left")) && (r = cgi_get("right")))
      disorder_set_volume(dcgi_client, atoi(l), atoi(r));
  }
  redirect(0);
}

/** @brief Expand the login template with @b @@error set to @p error
 * @param e Error keyword
 */
static void login_error(const char *e) {
  dcgi_error_string = e;
  dcgi_expand("login", 1);
}

/** @brief Log in
 * @param username Login name
 * @param password Password
 * @return 0 on success, non-0 on error
 *
 * On error, calls login_error() to expand the login template.
 */
static int login_as(const char *username, const char *password) {
  disorder_client *c;

  if(dcgi_cookie && dcgi_client)
    disorder_revoke(dcgi_client);
  /* We'll need a new connection as we are going to stop being guest */
  c = disorder_new(0);
  if(disorder_connect_user(c, username, password)) {
    login_error("loginfailed");
    return -1;
  }
  /* Generate a cookie so we can log in again later */
  if(disorder_make_cookie(c, &dcgi_cookie)) {
    login_error("cookiefailed");
    return -1;
  }
  /* Use the new connection henceforth */
  dcgi_client = c;
  dcgi_lookup_reset();
  return 0;                             /* OK */
}

/*! login
 *
 * If \fBusername\fR and \fBpassword\fR are set (and the username isn't
 * "guest") then attempt to log in using those credentials.  On success,
 * redirects to the \fBback\fR argument if that is set, or just expands
 * \fIlogin.tmpl\fI otherwise, with \fB@status\fR set to \fBloginok\fR.
 *
 * If they aren't set then just expands \fIlogin.tmpl\fI.
 */
static void act_login(void) {
  const char *username, *password;

  /* We try all this even if not connected since the subsequent connection may
   * succeed. */
  
  username = cgi_get("username");
  password = cgi_get("password");
  if(!username
     || !password
     || !strcmp(username, "guest")/*bodge to avoid guest cookies*/) {
    /* We're just visiting the login page, not performing an action at all. */
    dcgi_expand("login", 1);
    return;
  }
  if(!login_as(username, password)) {
    /* Report the succesful login */
    dcgi_status_string = "loginok";
    /* Redirect back to where we came from, if necessary */
    if(cgi_get("back"))
      redirect(0);
    else
      dcgi_expand("login", 1);
  }
}

/*! logout
 *
 * Logs out the current user and expands \fIlogin.tmpl\fR with \fBstatus\fR or
 * \fB@error\fR set according to the result.
 */
static void act_logout(void) {
  if(dcgi_client) {
    /* Ask the server to revoke the cookie */
    if(!disorder_revoke(dcgi_client))
      dcgi_status_string = "logoutok";
    else
      dcgi_error_string = "revokefailed";
  } else {
    /* We can't guarantee a logout if we can't connect to the server to revoke
     * the cookie, so we report an error.  We'll still ask the browser to
     * forget the cookie though. */
    dcgi_error_string = "connect";
  }
  /* Attempt to reconnect without the cookie */
  dcgi_cookie = 0;
  dcgi_login();
  /* Back to login page, hopefuly forcing the browser to forget the cookie. */
  dcgi_expand("login", 1);
}

/*! register
 *
 * Register a new user using \fBusername\fR, \fBpassword1\fR, \fBpassword2\fR
 * and \fBemail\fR and expands \fIlogin.tmpl\fR with \fBstatus\fR or
 * \fB@error\fR set according to the result.
 */
static void act_register(void) {
  const char *username, *password, *password2, *email;
  char *confirm, *content_type;
  const char *text, *encoding, *charset;

  /* If we're not connected then this is a hopeless exercise */
  if(!dcgi_client) {
    login_error("connect");
    return;
  }

  /* Collect arguments */
  username = cgi_get("username");
  password = cgi_get("password1");
  password2 = cgi_get("password2");
  email = cgi_get("email");

  /* Verify arguments */
  if(!username || !*username) {
    login_error("nousername");
    return;
  }
  if(!password || !*password) {
    login_error("nopassword");
    return;
  }
  if(!password2 || !*password2 || strcmp(password, password2)) {
    login_error("passwordmismatch");
    return;
  }
  if(!email || !*email) {
    login_error("noemail");
    return;
  }
  /* We could well do better address validation but for now we'll just do the
   * minimum */
  if(!email_valid(email)) {
    login_error("bademail");
    return;
  }
  if(disorder_register(dcgi_client, username, password, email, &confirm)) {
    login_error("cannotregister");
    return;
  }
  /* Send the user a mail */
  /* TODO templatize this */
  byte_xasprintf((char **)&text,
		 "Welcome to DisOrder.  To active your login, please visit this URL:\n"
		 "\n"
		 "%s?c=%s\n", config->url, urlencodestring(confirm));
  if(!(text = mime_encode_text(text, &charset, &encoding)))
    fatal(0, "cannot encode email");
  byte_xasprintf(&content_type, "text/plain;charset=%s",
		 quote822(charset, 0));
  sendmail("", config->mail_sender, email, "Welcome to DisOrder",
	   encoding, content_type, text); /* TODO error checking  */
  /* We'll go back to the login page with a suitable message */
  dcgi_status_string = "registered";
  dcgi_expand("login", 1);
}

/*! confirm
 *
 * Confirm a user registration using the nonce supplied in \fBc\fR and expands
 * \fIlogin.tmpl\fR with \fBstatus\fR or \fB@error\fR set according to the
 * result.
 */
static void act_confirm(void) {
  const char *confirmation;

  /* If we're not connected then this is a hopeless exercise */
  if(!dcgi_client) {
    login_error("connect");
    return;
  }

  if(!(confirmation = cgi_get("c"))) {
    login_error("noconfirm");
    return;
  }
  /* Confirm our registration */
  if(disorder_confirm(dcgi_client, confirmation)) {
    login_error("badconfirm");
    return;
  }
  /* Get a cookie */
  if(disorder_make_cookie(dcgi_client, &dcgi_cookie)) {
    login_error("cookiefailed");
    return;
  }
  /* Junk cached data */
  dcgi_lookup_reset();
  /* Report success */
  dcgi_status_string = "confirmed";
  dcgi_expand("login", 1);
}

/*! edituser
 *
 * Edit user details using \fBusername\fR, \fBchangepassword1\fR,
 * \fBchangepassword2\fR and \fBemail\fR and expands \fIlogin.tmpl\fR with
 * \fBstatus\fR or \fB@error\fR set according to the result.
 */
static void act_edituser(void) {
  const char *email = cgi_get("email"), *password = cgi_get("changepassword1");
  const char *password2 = cgi_get("changepassword2");
  int newpassword = 0;

  /* If we're not connected then this is a hopeless exercise */
  if(!dcgi_client) {
    login_error("connect");
    return;
  }

  /* Verify input */

  /* If either password or password2 is set we insist they match.  If they
   * don't we report an error. */
  if((password && *password) || (password2 && *password2)) {
    if(!password || !password2 || strcmp(password, password2)) {
      login_error("passwordmismatch");
      return;
    }
  } else
    password = password2 = 0;
  if(email && !email_valid(email)) {
    login_error("bademail");
    return;
  }

  /* Commit changes */
  if(email) {
    if(disorder_edituser(dcgi_client, disorder_user(dcgi_client),
			 "email", email)) {
      login_error("badedit");
      return;
    }
  }
  if(password) {
    if(disorder_edituser(dcgi_client, disorder_user(dcgi_client),
			 "password", password)) {
      login_error("badedit");
      return;
    }
    newpassword = 1;
  }

  if(newpassword) {
    /* If we changed the password, the cookie is now invalid, so we must log
     * back in. */
    if(login_as(disorder_user(dcgi_client), password))
      return;
  }
  /* Report success */
  dcgi_status_string = "edited";
  dcgi_expand("login", 1);
}

/*! reminder
 *
 * Issue an email password reminder to \fBusername\fR and expands
 * \fIlogin.tmpl\fR with \fBstatus\fR or \fB@error\fR set according to the
 * result.
 */
static void act_reminder(void) {
  const char *const username = cgi_get("username");

  /* If we're not connected then this is a hopeless exercise */
  if(!dcgi_client) {
    login_error("connect");
    return;
  }

  if(!username || !*username) {
    login_error("nousername");
    return;
  }
  if(disorder_reminder(dcgi_client, username)) {
    login_error("reminderfailed");
    return;
  }
  /* Report success */
  dcgi_status_string = "reminded";
  dcgi_expand("login", 1);
}

/** @brief Get the numbered version of an argument
 * @param argname Base argument name
 * @param numfile File number
 * @return cgi_get(NUMFILE_ARGNAME)
 */
static const char *numbered_arg(const char *argname, int numfile) {
  char *fullname;

  byte_xasprintf(&fullname, "%d_%s", numfile, argname);
  return cgi_get(fullname);
}

/** @brief Set preferences for file @p numfile
 * @return 0 on success, -1 if there is no such track number
 *
 * The old @b nfiles parameter has been abolished, we just keep look for more
 * files until we run out.
 */
static int process_prefs(int numfile) {
  const char *file, *name, *value, *part, *parts, *context;
  char **partslist;

  if(!(file = numbered_arg("track", numfile)))
    return -1;
  if(!(parts = cgi_get("parts")))
    parts = "artist album title";
  if(!(context = cgi_get("context")))
    context = "display";
  partslist = split(parts, 0, 0, 0, 0);
  while((part = *partslist++)) {
    if(!(value = numbered_arg(part, numfile)))
      continue;
    byte_xasprintf((char **)&name, "trackname_%s_%s", context, part);
    disorder_set(dcgi_client, file, name, value);
  }
  if((value = numbered_arg("random", numfile)))
    disorder_unset(dcgi_client, file, "pick_at_random");
  else
    disorder_set(dcgi_client, file, "pick_at_random", "0");
  if((value = numbered_arg("tags", numfile))) {
    if(!*value)
      disorder_unset(dcgi_client, file, "tags");
    else
      disorder_set(dcgi_client, file, "tags", value);
  }
  if((value = numbered_arg("weight", numfile))) {
    if(!*value)
      disorder_unset(dcgi_client, file, "weight");
    else
      disorder_set(dcgi_client, file, "weight", value);
  }
  return 0;
}

/*! prefs
 *
 * Set preferences on a number of tracks.
 *
 * The tracks to modify are specified in arguments \fB0_track\fR, \fB1_track\fR
 * etc.  The number sequence must be contiguous and start from 0.
 *
 * For each track \fIINDEX\fB_track\fR:
 * - \fIINDEX\fB_\fIPART\fR is used to set the trackname preference for
 * that part.  (See \fBparts\fR below.)
 * - \fIINDEX\fB_\fIrandom\fR if present enables random play for this track
 * or disables it if absent.
 * - \fIINDEX\fB_\fItags\fR sets the list of tags for this track.
 * - \fIINDEX\fB_\fIweight\fR sets the weight for this track.
 *
 * \fBparts\fR can be set to the track name parts to modify.  The default is
 * "artist album title".
 *
 * \fBcontext\fR can be set to the context to modify.  The default is
 * "display".
 *
 * If the server detects a preference being set to its default, it removes the
 * preference, thus keeping the database tidy.
 */
static void act_set(void) {
  int numfile;

  if(dcgi_client) {
    for(numfile = 0; !process_prefs(numfile); ++numfile)
      ;
  }
  redirect(0);
}

/** @brief Table of actions */
static const struct action {
  /** @brief Action name */
  const char *name;
  /** @brief Action handler */
  void (*handler)(void);
  /** @brief Union of suitable rights */
  rights_type rights;
} actions[] = {
  { "confirm", act_confirm, 0 },
  { "disable", act_disable, RIGHT_GLOBAL_PREFS },
  { "edituser", act_edituser, 0 },
  { "enable", act_enable, RIGHT_GLOBAL_PREFS },
  { "login", act_login, 0 },
  { "logout", act_logout, 0 },
  { "manage", act_playing, 0 },
  { "move", act_move, RIGHT_MOVE__MASK },
  { "pause", act_pause, RIGHT_PAUSE },
  { "play", act_play, RIGHT_PLAY },
  { "playing", act_playing, 0 },
  { "randomdisable", act_random_disable, RIGHT_GLOBAL_PREFS },
  { "randomenable", act_random_enable, RIGHT_GLOBAL_PREFS },
  { "register", act_register, 0 },
  { "reminder", act_reminder, 0 },
  { "remove", act_remove, RIGHT_MOVE__MASK|RIGHT_SCRATCH__MASK },
  { "resume", act_resume, RIGHT_PAUSE },
  { "set", act_set, RIGHT_PREFS },
  { "volume", act_volume, RIGHT_VOLUME },
};

/** @brief Check that an action name is valid
 * @param name Action
 * @return 1 if valid, 0 if not
 */
static int dcgi_valid_action(const char *name) {
  int c;

  /* First character must be letter or digit (this also requires there to _be_
   * a first character) */
  if(!isalnum((unsigned char)*name))
    return 0;
  /* Only letters, digits, '.' and '-' allowed */
  while((c = (unsigned char)*name++)) {
    if(!(isalnum(c)
         || c == '.'
         || c == '_'))
      return 0;
  }
  return 1;
}

/** @brief Expand a template
 * @param name Base name of template, or NULL to consult CGI args
 * @param header True to write header
 */
void dcgi_expand(const char *name, int header) {
  const char *p, *found;

  /* Parse macros first */
  if((found = mx_find("macros.tmpl", 1/*report*/)))
    mx_expand_file(found, sink_discard(), 0);
  if((found = mx_find("user.tmpl", 0/*report*/)))
    mx_expand_file(found, sink_discard(), 0);
  /* For unknown actions check that they aren't evil */
  if(!dcgi_valid_action(name))
    fatal(0, "invalid action name '%s'", name);
  byte_xasprintf((char **)&p, "%s.tmpl", name);
  if(!(found = mx_find(p, 0/*report*/)))
    fatal(errno, "cannot find %s", p);
  if(header) {
    if(printf("Content-Type: text/html; charset=UTF-8\n"
              "%s\n"
              "\n", dcgi_cookie_header()) < 0)
      fatal(errno, "error writing to stdout");
  }
  if(mx_expand_file(found, sink_stdio("stdout", stdout), 0) == -1
     || fflush(stdout) < 0)
    fatal(errno, "error writing to stdout");
}

/** @brief Execute a web action
 * @param action Action to perform, or NULL to consult CGI args
 *
 * If no recognized action is specified then 'playing' is assumed.
 */
void dcgi_action(const char *action) {
  int n;

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
    /* Make sure 'action' is always set */
    cgi_set("action", action);
  }
  if((n = TABLE_FIND(actions, name, action)) >= 0) {
    if(actions[n].rights) {
      /* Some right or other is required */
      dcgi_lookup(DCGI_RIGHTS);
      if(!(actions[n].rights & dcgi_rights)) {
        const char *back = cgi_thisurl(config->url);
        /* Failed operations jump you to the login screen with an error
         * message.  On success, the user comes back to the page they were
         * after. */
        cgi_clear();
        cgi_set("back", back);
        login_error("noright");
        return;
      }
    }
    /* It's a known action */
    actions[n].handler();
  } else {
    /* Just expand the template */
    dcgi_expand(action, 1/*header*/);
  }
}

/** @brief Generate an error page */
void dcgi_error(const char *key) {
  dcgi_error_string = xstrdup(key);
  dcgi_expand("error", 1);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
