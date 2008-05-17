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
  if(url) {
    if(!strncmp(url, "http", 4))
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

/* 'playing' and 'manage' just add a Refresh: header */
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
  if(printf("Content-Type: text/html\n"
            "Refresh: %ld;url=%s\n"
            "%s\n"
            "\n",
            refresh, url, dcgi_cookie_header()) < 0)
    fatal(errno, "error writing to stdout");
  dcgi_expand("playing");
}

static void act_disable(void) {
  if(dcgi_client)
    disorder_disable(dcgi_client);
  redirect(0);
}

static void act_enable(void) {
  if(dcgi_client)
    disorder_enable(dcgi_client);
  redirect(0);
}

static void act_random_disable(void) {
  if(dcgi_client)
    disorder_random_disable(dcgi_client);
  redirect(0);
}

static void act_random_enable(void) {
  if(dcgi_client)
    disorder_random_enable(dcgi_client);
  redirect(0);
}

static void act_pause(void) {
  if(dcgi_client)
    disorder_pause(dcgi_client);
  redirect(0);
}

static void act_resume(void) {
  if(dcgi_client)
    disorder_resume(dcgi_client);
  redirect(0);
}

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

static void act_play(void) {
  const char *track, *dir;
  char **tracks;
  int ntracks, n;
  struct dcgi_entry *e;
  
  if(dcgi_client) {
    if((track = cgi_get("file"))) {
      disorder_play(dcgi_client, track);
    } else if((dir = cgi_get("dir"))) {
      if(disorder_files(dcgi_client, dir, 0, &tracks, &ntracks))
        ntracks = 0;
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

/** @brief Table of actions */
static const struct action {
  /** @brief Action name */
  const char *name;
  /** @brief Action handler */
  void (*handler)(void);
} actions[] = {
  { "disable", act_disable },
  { "enable", act_enable },
  { "manage", act_playing },
  { "move", act_move },
  { "pause", act_pause },
  { "play", act_play },
  { "playing", act_playing },
  { "randomdisable", act_random_disable },
  { "randomenable", act_random_enable },
  { "remove", act_remove },
  { "resume", act_resume },
  { "volume", act_volume },
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
 */
void dcgi_expand(const char *name) {
  const char *p, *found;

  /* Parse macros first */
  if((found = mx_find("macros.tmpl")))
    mx_expand_file(found, sink_discard(), 0);
  /* For unknown actions check that they aren't evil */
  if(!dcgi_valid_action(name))
    fatal(0, "invalid action name '%s'", name);
  byte_xasprintf((char **)&p, "%s.tmpl", name);
  if(!(found = mx_find(p)))
    fatal(errno, "cannot find %s", p);
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
  if((n = TABLE_FIND(actions, struct action, name, action)) >= 0)
    /* Its a known action */
    actions[n].handler();
  else {
    /* Just expand the template */
    if(printf("Content-Type: text/html\n"
              "%s\n"
              "\n", dcgi_cookie_header()) < 0)
      fatal(errno, "error writing to stdout");
    dcgi_expand(action);
  }
}

/** @brief Generate an error page */
void dcgi_error(const char *key) {
  dcgi_error_string = xstrdup(key);
  dcgi_expand("error");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
