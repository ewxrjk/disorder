/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <pcre.h>
#include <assert.h>

#include "client.h"
#include "mem.h"
#include "vector.h"
#include "sink.h"
#include "cgi.h"
#include "dcgi.h"
#include "log.h"
#include "configuration.h"
#include "table.h"
#include "queue.h"
#include "plugin.h"
#include "split.h"
#include "wstat.h"
#include "kvp.h"
#include "syscalls.h"
#include "printf.h"
#include "regsub.h"
#include "defs.h"
#include "trackname.h"
#include "charset.h"

char *login_cookie;

static void expand(cgi_sink *output,
		   const char *template,
		   dcgi_state *ds);
static void expandstring(cgi_sink *output,
			 const char *string,
			 dcgi_state *ds);

struct entry {
  const char *path;
  const char *sort;
  const char *display;
};

static const char *nonce(void) {
  static unsigned long count;
  char *s;

  byte_xasprintf(&s, "%lx%lx%lx",
	   (unsigned long)time(0),
	   (unsigned long)getpid(),
	   count++);
  return s;
}

static int compare_entry(const void *a, const void *b) {
  const struct entry *ea = a, *eb = b;

  return compare_tracks(ea->sort, eb->sort,
			ea->display, eb->display,
			ea->path, eb->path);
}

static const char *front_url(void) {
  char *url;
  const char *mgmt;

  /* preserve management interface visibility */
  if((mgmt = cgi_get("mgmt")) && !strcmp(mgmt, "true")) {
    byte_xasprintf(&url, "%s?mgmt=true", config->url);
    return url;
  }
  return config->url;
}

static void header_cookie(struct sink *output) {
  struct dynstr d[1];
  char *s;

  if(login_cookie) {
    dynstr_init(d);
    for(s = login_cookie; *s; ++s) {
      if(*s == '"')
	dynstr_append(d, '\\');
      dynstr_append(d, *s);
    }
    dynstr_terminate(d);
    byte_xasprintf(&s, "disorder=\"%s\"", d->vec); /* TODO domain, path, expiry */
    cgi_header(output, "Set-Cookie", s);
  } else
    /* Force browser to discard cookie */
    cgi_header(output, "Set-Cookie", "disorder=none;Max-Age=0");
}

static void redirect(struct sink *output) {
  const char *back;

  back = cgi_get("back");
  cgi_header(output, "Location", back && *back ? back : front_url());
  header_cookie(output);
  cgi_body(output);
}

static void expand_template(dcgi_state *ds, cgi_sink *output,
			    const char *action) {
  cgi_header(output->sink, "Content-Type", "text/html");
  header_cookie(output->sink);
  cgi_body(output->sink);
  expand(output, action, ds);
}

static void lookups(dcgi_state *ds, unsigned want) {
  unsigned need;
  struct queue_entry *r, *rnext;
  const char *dir, *re;

  if(ds->g->client && (need = want ^ (ds->g->flags & want)) != 0) {
    if(need & DC_QUEUE)
      disorder_queue(ds->g->client, &ds->g->queue);
    if(need & DC_PLAYING)
      disorder_playing(ds->g->client, &ds->g->playing);
    if(need & DC_NEW)
      disorder_new_tracks(ds->g->client, &ds->g->new, &ds->g->nnew, 0);
    if(need & DC_RECENT) {
      /* we need to reverse the order of the list */
      disorder_recent(ds->g->client, &r);
      while(r) {
	rnext = r->next;
	r->next = ds->g->recent;
	ds->g->recent = r;
	r = rnext;
      }
    }
    if(need & DC_VOLUME)
      disorder_get_volume(ds->g->client,
			  &ds->g->volume_left, &ds->g->volume_right);
    if(need & (DC_FILES|DC_DIRS)) {
      if(!(dir = cgi_get("directory")))
	dir = "";
      re = cgi_get("regexp");
      if(need & DC_DIRS)
	if(disorder_directories(ds->g->client, dir, re,
				&ds->g->dirs, &ds->g->ndirs))
	  ds->g->ndirs = 0;
      if(need & DC_FILES)
	if(disorder_files(ds->g->client, dir, re,
			  &ds->g->files, &ds->g->nfiles))
	  ds->g->nfiles = 0;
    }
    ds->g->flags |= need;
  }
}

/* actions ********************************************************************/

static void act_disable(cgi_sink *output,
			dcgi_state *ds) {
  if(ds->g->client)
    disorder_disable(ds->g->client);
  redirect(output->sink);
}

static void act_enable(cgi_sink *output,
			      dcgi_state *ds) {
  if(ds->g->client)
    disorder_enable(ds->g->client);
  redirect(output->sink);
}

static void act_random_disable(cgi_sink *output,
			       dcgi_state *ds) {
  if(ds->g->client)
    disorder_random_disable(ds->g->client);
  redirect(output->sink);
}

static void act_random_enable(cgi_sink *output,
			      dcgi_state *ds) {
  if(ds->g->client)
    disorder_random_enable(ds->g->client);
  redirect(output->sink);
}

static void act_remove(cgi_sink *output,
		       dcgi_state *ds) {
  const char *id;

  if(!(id = cgi_get("id"))) fatal(0, "missing id argument");
  if(ds->g->client)
    disorder_remove(ds->g->client, id);
  redirect(output->sink);
}

static void act_move(cgi_sink *output,
		     dcgi_state *ds) {
  const char *id, *delta;

  if(!(id = cgi_get("id"))) fatal(0, "missing id argument");
  if(!(delta = cgi_get("delta"))) fatal(0, "missing delta argument");
  if(ds->g->client)
    disorder_move(ds->g->client, id, atoi(delta));
  redirect(output->sink);
}

static void act_scratch(cgi_sink *output,
			dcgi_state *ds) {
  if(ds->g->client)
    disorder_scratch(ds->g->client, cgi_get("id"));
  redirect(output->sink);
}

static void act_playing(cgi_sink *output, dcgi_state *ds) {
  char r[1024];
  long refresh = config->refresh, length;
  time_t now, fin;
  int random_enabled = 0;
  int enabled = 0;

  lookups(ds, DC_PLAYING|DC_QUEUE);
  cgi_header(output->sink, "Content-Type", "text/html");
  disorder_random_enabled(ds->g->client, &random_enabled);
  disorder_enabled(ds->g->client, &enabled);
  if(ds->g->playing
     && ds->g->playing->state == playing_started /* i.e. not paused */
     && !disorder_length(ds->g->client, ds->g->playing->track, &length)
     && length
     && ds->g->playing->sofar >= 0) {
    /* Try to put the next refresh at the start of the next track. */
    time(&now);
    fin = now + length - ds->g->playing->sofar + config->gap;
    if(now + refresh > fin)
      refresh = fin - now;
  }
  if(ds->g->queue && ds->g->queue->state == playing_isscratch) {
    /* next track is a scratch, don't leave more than the inter-track gap */
    if(refresh > config->gap)
      refresh = config->gap;
  }
  if(!ds->g->playing && ((ds->g->queue
			  && ds->g->queue->state != playing_random)
			 || random_enabled) && enabled) {
    /* no track playing but playing is enabled and there is something coming
     * up, must be in a gap */
    if(refresh > config->gap)
      refresh = config->gap;
  }
  byte_snprintf(r, sizeof r, "%ld;url=%s", refresh > 0 ? refresh : 1,
		front_url());
  cgi_header(output->sink, "Refresh", r);
  header_cookie(output->sink);
  cgi_body(output->sink);
  expand(output, "playing", ds);
}

static void act_play(cgi_sink *output,
		     dcgi_state *ds) {
  const char *track, *dir;
  char **tracks;
  int ntracks, n;
  struct entry *e;

  if((track = cgi_get("file"))) {
    disorder_play(ds->g->client, track);
  } else if((dir = cgi_get("directory"))) {
    if(disorder_files(ds->g->client, dir, 0, &tracks, &ntracks)) ntracks = 0;
    if(ntracks) {
      e = xmalloc(ntracks * sizeof (struct entry));
      for(n = 0; n < ntracks; ++n) {
	e[n].path = tracks[n];
	e[n].sort = trackname_transform("track", tracks[n], "sort");
	e[n].display = trackname_transform("track", tracks[n], "display");
      }
      qsort(e, ntracks, sizeof (struct entry), compare_entry);
      for(n = 0; n < ntracks; ++n)
	disorder_play(ds->g->client, e[n].path);
    }
  }
  /* XXX error handling */
  redirect(output->sink);
}

static int clamp(int n, int min, int max) {
  if(n < min)
    return min;
  if(n > max)
    return max;
  return n;
}

static const char *volume_url(void) {
  char *url;
  
  byte_xasprintf(&url, "%s?action=volume", config->url);
  return url;
}

static void act_volume(cgi_sink *output, dcgi_state *ds) {
  const char *l, *r, *d, *back;
  int nd, changed = 0;;

  if((d = cgi_get("delta"))) {
    lookups(ds, DC_VOLUME);
    nd = clamp(atoi(d), -255, 255);
    disorder_set_volume(ds->g->client,
			clamp(ds->g->volume_left + nd, 0, 255),
			clamp(ds->g->volume_right + nd, 0, 255));
    changed = 1;
  } else if((l = cgi_get("left")) && (r = cgi_get("right"))) {
    disorder_set_volume(ds->g->client, atoi(l), atoi(r));
    changed = 1;
  }
  if(changed) {
    /* redirect back to ourselves (but without the volume-changing bits in the
     * URL) */
    cgi_header(output->sink, "Location",
	       (back = cgi_get("back")) ? back : volume_url());
    header_cookie(output->sink);
    cgi_body(output->sink);
  } else {
    cgi_header(output->sink, "Content-Type", "text/html");
    header_cookie(output->sink);
    cgi_body(output->sink);
    expand(output, "volume", ds);
  }
}

static void act_prefs_errors(const char *msg,
			     void attribute((unused)) *u) {
  fatal(0, "error splitting parts list: %s", msg);
}

static const char *numbered_arg(const char *argname, int numfile) {
  char *fullname;

  byte_xasprintf(&fullname, "%d_%s", numfile, argname);
  return cgi_get(fullname);
}

static void process_prefs(dcgi_state *ds, int numfile) {
  const char *file, *name, *value, *part, *parts, *current, *context;
  char **partslist;

  if(!(file = numbered_arg("file", numfile)))
    /* The first file doesn't need numbering. */
    if(numfile > 0 || !(file = cgi_get("file")))
      return;
  if((parts = numbered_arg("parts", numfile))
     || (parts = cgi_get("parts"))) {
    /* Default context is display.  Other contexts not actually tested. */
    if(!(context = numbered_arg("context", numfile))) context = "display";
    partslist = split(parts, 0, 0, act_prefs_errors, 0);
    while((part = *partslist++)) {
      if(!(value = numbered_arg(part, numfile)))
	continue;
      /* If it's already right (whether regexps or db) don't change anything,
       * so we don't fill the database up with rubbish. */
      if(disorder_part(ds->g->client, (char **)&current,
		       file, context, part))
	fatal(0, "disorder_part() failed");
      if(!strcmp(current, value))
	continue;
      byte_xasprintf((char **)&name, "trackname_%s_%s", context, part);
      disorder_set(ds->g->client, file, name, value);
    }
    if((value = numbered_arg("random", numfile)))
      disorder_unset(ds->g->client, file, "pick_at_random");
    else
      disorder_set(ds->g->client, file, "pick_at_random", "0");
    if((value = numbered_arg("tags", numfile)))
      disorder_set(ds->g->client, file, "tags", value);
  } else if((name = cgi_get("name"))) {
    /* Raw preferences.  Not well supported in the templates at the moment. */
    value = cgi_get("value");
    if(value)
      disorder_set(ds->g->client, file, name, value);
    else
      disorder_unset(ds->g->client, file, name);
  }
}

static void act_prefs(cgi_sink *output, dcgi_state *ds) {
  const char *files;
  int nfiles, numfile;

  if((files = cgi_get("files"))) nfiles = atoi(files);
  else nfiles = 1;
  for(numfile = 0; numfile < nfiles; ++numfile)
    process_prefs(ds, numfile);
  cgi_header(output->sink, "Content-Type", "text/html");
  header_cookie(output->sink);
  cgi_body(output->sink);
  expand(output, "prefs", ds);
}

static void act_pause(cgi_sink *output,
		      dcgi_state *ds) {
  if(ds->g->client)
    disorder_pause(ds->g->client);
  redirect(output->sink);
}

static void act_resume(cgi_sink *output,
		       dcgi_state *ds) {
  if(ds->g->client)
    disorder_resume(ds->g->client);
  redirect(output->sink);
}

static void act_login(cgi_sink *output,
		      dcgi_state *ds) {
  const char *username, *password, *back;
  disorder_client *c;

  username = cgi_get("username");
  password = cgi_get("password");
  if(!username || !password
     || !strcmp(username, "guest")/*bodge to avoid guest cookies*/) {
    /* We're just visiting the login page */
    expand_template(ds, output, "login");
    return;
  }
  c = disorder_new(1);
  if(disorder_connect_user(c, username, password)) {
    cgi_set_option("error", "loginfailed");
    expand_template(ds, output, "login");
    return;
  }
  if(disorder_make_cookie(c, &login_cookie)) {
    cgi_set_option("error", "cookiefailed");
    expand_template(ds, output, "login");
    return;
  }
  /* We have a new cookie */
  header_cookie(output->sink);
  if((back = cgi_get("back")) && back)
    /* Redirect back to somewhere or other */
    redirect(output->sink);
  else
    /* Stick to the login page */
    expand_template(ds, output, "login");
}

static void act_logout(cgi_sink *output,
		       dcgi_state *ds) {
  disorder_revoke(ds->g->client);
  login_cookie = 0;
  /* Reconnect as guest */
  ds->g->client = disorder_new(0);
  if(disorder_connect_cookie(ds->g->client, 0)) {
    disorder_cgi_error(output, ds, "connect");
    exit(0);
  }
  /* Back to the login page */
  expand_template(ds, output, "login");
}

static void act_register(cgi_sink *output,
			 dcgi_state *ds) {
  const char *username, *password, *email;
  char *confirm;

  username = cgi_get("username");
  password = cgi_get("password");
  email = cgi_get("email");

  if(!username || !*username) {
    cgi_set_option("error", "nousername");
    expand_template(ds, output, "login");
    return;
  }
  if(!password || !*password) {
    cgi_set_option("error", "nopassword");
    expand_template(ds, output, "login");
    return;
  }
  if(!email || !*email) {
    cgi_set_option("error", "noemail");
    expand_template(ds, output, "login");
    return;
  }
  /* We could well do better address validation but for now we'll just do the
   * minimum */
  if(!strchr(email, '@')) {
    cgi_set_option("error", "bademail");
    expand_template(ds, output, "login");
    return;
  }
  if(disorder_register(ds->g->client, username, password, email, &confirm)) {
    cgi_set_option("error", "cannotregister");
    expand_template(ds, output, "login");
    return;
  }
  /* We'll go back to the login page with a suitable message */
  cgi_set_option("registered", "registeredok");
  expand_template(ds, output, "login");
}

static const struct action {
  const char *name;
  void (*handler)(cgi_sink *output, dcgi_state *ds);
} actions[] = {
  { "disable", act_disable },
  { "enable", act_enable },
  { "login", act_login },
  { "logout", act_logout },
  { "move", act_move },
  { "pause", act_pause },
  { "play", act_play },
  { "playing", act_playing },
  { "prefs", act_prefs },
  { "random-disable", act_random_disable },
  { "random-enable", act_random_enable },
  { "register", act_register },
  { "remove", act_remove },
  { "resume", act_resume },
  { "scratch", act_scratch },
  { "volume", act_volume },
};

/* expansions *****************************************************************/

static void exp_include(int attribute((unused)) nargs,
			char **args,
			cgi_sink *output,
			void *u) {
  expand(output, args[0], u);
}

static void exp_server_version(int attribute((unused)) nargs,
			       char attribute((unused)) **args,
			       cgi_sink *output,
			       void *u) {
  dcgi_state *ds = u;
  const char *v;

  if(ds->g->client) {
    if(disorder_version(ds->g->client, (char **)&v)) v = "(cannot get version)";
  } else
    v = "(server not running)";
  cgi_output(output, "%s", v);
}

static void exp_version(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void attribute((unused)) *u) {
  cgi_output(output, "%s", disorder_short_version_string);
}

static void exp_nonce(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void attribute((unused)) *u) {
  cgi_output(output, "%s", nonce());
}

static void exp_label(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void attribute((unused)) *u) {
  cgi_output(output, "%s", cgi_label(args[0]));
}

struct trackinfo_state {
  dcgi_state *ds;
  const struct queue_entry *q;
  long length;
  time_t when;
};

static void exp_who(int attribute((unused)) nargs,
		    char attribute((unused)) **args,
		    cgi_sink *output,
		    void *u) {
  dcgi_state *ds = u;
  
  if(ds->track && ds->track->submitter)
    cgi_output(output, "%s", ds->track->submitter);
}

static void exp_length(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  long length = 0;

  if(ds->track
     && (ds->track->state == playing_started
	 || ds->track->state == playing_paused)
     && ds->track->sofar >= 0)
    cgi_output(output, "%ld:%02ld/",
	       ds->track->sofar / 60, ds->track->sofar % 60);
  length = 0;
  if(ds->track)
    disorder_length(ds->g->client, ds->track->track, &length);
  else if(ds->tracks)
    disorder_length(ds->g->client, ds->tracks[0], &length);
  if(length)
    cgi_output(output, "%ld:%02ld", length / 60, length % 60);
  else
    sink_printf(output->sink, "%s", "&nbsp;");
}

static void exp_when(int attribute((unused)) nargs,
		     char attribute((unused)) **args,
		     cgi_sink *output,
		     void *u) {
  dcgi_state *ds = u;
  const struct tm *w = 0;

  if(ds->track)
    switch(ds->track->state) {
    case playing_isscratch:
    case playing_unplayed:
    case playing_random:
      if(ds->track->expected)
	w = localtime(&ds->track->expected);
      break;
    case playing_failed:
    case playing_no_player:
    case playing_ok:
    case playing_scratched:
    case playing_started:
    case playing_paused:
    case playing_quitting:
      if(ds->track->played)
	w = localtime(&ds->track->played);
      break;
    }
  if(w)
    cgi_output(output, "%d:%02d", w->tm_hour, w->tm_min);
  else
    sink_printf(output->sink, "&nbsp;");
}

static void exp_part(int nargs,
		     char **args,
		     cgi_sink *output,
		     void *u) {
  dcgi_state *ds = u;
  const char *s, *track, *part, *context;

  if(nargs == 3)
    track = args[2];
  else {
    if(ds->track)
      track = ds->track->track;
    else if(ds->tracks)
      track = ds->tracks[0];
    else
      track = 0;
  }
  if(track) {
    switch(nargs) {
    case 1:
      context = "display";
      part = args[0];
      break;
    case 2:
    case 3:
      context = args[0];
      part = args[1];
      break;
    default:
      abort();
    }
    if(disorder_part(ds->g->client, (char **)&s, track,
		     !strcmp(context, "short") ? "display" : context, part))
      fatal(0, "disorder_part() failed");
    if(!strcmp(context, "short"))
      s = truncate_for_display(s, config->short_display);
    cgi_output(output, "%s", s);
  } else
    sink_printf(output->sink, "&nbsp;");
}

static void exp_playing(int attribute((unused)) nargs,
			char **args,
			cgi_sink *output,
			void  *u) {
  dcgi_state *ds = u;
  dcgi_state s;

  lookups(ds, DC_PLAYING);
  memset(&s, 0, sizeof s);
  s.g = ds->g;
  if(ds->g->playing) {
    s.track = ds->g->playing;
    expandstring(output, args[0], &s);
  }
}

static void exp_queue(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void  *u) {
  dcgi_state *ds = u;
  dcgi_state s;
  struct queue_entry *q;

  lookups(ds, DC_QUEUE);
  memset(&s, 0, sizeof s);
  s.g = ds->g;
  s.first = 1;
  for(q = ds->g->queue; q; q = q->next) {
    s.last = !q->next;
    s.track = q;
    expandstring(output, args[0], &s);
    s.index++;
    s.first = 0;
  }
}

static void exp_recent(int attribute((unused)) nargs,
		       char **args,
		       cgi_sink *output,
		       void  *u) {
  dcgi_state *ds = u;
  dcgi_state s;
  struct queue_entry *q;

  lookups(ds, DC_RECENT);
  memset(&s, 0, sizeof s);
  s.g = ds->g;
  s.first = 1;
  for(q = ds->g->recent; q; q = q->next) {
    s.last = !q;
    s.track = q;
    expandstring(output, args[0], &s);
    s.index++;
    s.first = 0;
  }
}

static void exp_new(int attribute((unused)) nargs,
		    char **args,
		    cgi_sink *output,
		    void  *u) {
  dcgi_state *ds = u;
  dcgi_state s;

  lookups(ds, DC_NEW);
  memset(&s, 0, sizeof s);
  s.g = ds->g;
  s.first = 1;
  for(s.index = 0; s.index < ds->g->nnew; ++s.index) {
    s.last = s.index + 1 < ds->g->nnew;
    s.tracks = &ds->g->new[s.index];
    expandstring(output, args[0], &s);
    s.first = 0;
  }
}

static void exp_url(int attribute((unused)) nargs,
		    char attribute((unused)) **args,
		    cgi_sink *output,
		    void attribute((unused)) *u) {
  cgi_output(output, "%s", config->url);
}

struct result {
  char *track;
  const char *sort;
};

static int compare_result(const void *a, const void *b) {
  const struct result *ra = a, *rb = b;
  int c;

  if(!(c = strcmp(ra->sort, rb->sort)))
    c = strcmp(ra->track, rb->track);
  return c;
}

static void exp_search(int nargs,
		       char **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u, substate;
  char **tracks;
  const char *q, *context, *part, *template;
  int ntracks, n, m;
  struct result *r;

  switch(nargs) {
  case 2:
    part = args[0];
    context = "sort";
    template = args[1];
    break;
  case 3:
    part = args[0];
    context = args[1];
    template = args[2];
    break;
  default:
    assert(!"should never happen");
    part = context = template = 0;	/* quieten compiler */
  }
  if(ds->tracks == 0) {
    /* we are the top level, let's get some search results */
    if(!(q = cgi_get("query"))) return;	/* no results yet */
    if(disorder_search(ds->g->client, q, &tracks, &ntracks)) return;
    if(!ntracks) return;
  } else {
    tracks = ds->tracks;
    ntracks = ds->ntracks;
  }
  assert(ntracks != 0);
  /* sort tracks by the appropriate part */
  r = xmalloc(ntracks * sizeof *r);
  for(n = 0; n < ntracks; ++n) {
    r[n].track = tracks[n];
    if(disorder_part(ds->g->client, (char **)&r[n].sort,
		     tracks[n], context, part))
      fatal(0, "disorder_part() failed");
  }
  qsort(r, ntracks, sizeof (struct result), compare_result);
  /* expand the 2nd arg once for each group.  We re-use the passed-in tracks
   * array as we know it's guaranteed to be big enough and isn't going to be
   * used for anything else any more. */
  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  substate.first = 1;
  n = 0;
  while(n < ntracks) {
    substate.tracks = tracks;
    substate.ntracks = 0;
    m = n;
    while(m < ntracks
	  && !strcmp(r[m].sort, r[n].sort))
      tracks[substate.ntracks++] = r[m++].track;
    substate.last = (m == ntracks);
    expandstring(output, template, &substate);
    substate.index++;
    substate.first = 0;
    n = m;
  }
  assert(substate.last != 0);
}

static void exp_arg(int attribute((unused)) nargs,
		    char **args,
		    cgi_sink *output,
		    void attribute((unused)) *u) {
  const char *v;

  if((v = cgi_get(args[0])))
    cgi_output(output, "%s", v);
}

static void exp_stats(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;
  char **v;

  cgi_opentag(output->sink, "pre", "class", "stats", (char *)0);
  if(!disorder_stats(ds->g->client, &v, 0)) {
    while(*v)
      cgi_output(output, "%s\n", *v++);
  }
  cgi_closetag(output->sink, "pre");
}

static void exp_volume(int attribute((unused)) nargs,
		       char **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_VOLUME);
  if(!strcmp(args[0], "left"))
    cgi_output(output, "%d", ds->g->volume_left);
  else
    cgi_output(output, "%d", ds->g->volume_right);
}

static void exp_shell(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void attribute((unused)) *u) {
  int w, p[2], n;
  char buffer[4096];
  pid_t pid;
  
  xpipe(p);
  if(!(pid = xfork())) {
    exitfn = _exit;
    xclose(p[0]);
    xdup2(p[1], 1);
    xclose(p[1]);
    execlp("sh", "sh", "-c", args[0], (char *)0);
    fatal(errno, "error executing sh");
  }
  xclose(p[1]);
  while((n = read(p[0], buffer, sizeof buffer))) {
    if(n < 0) {
      if(errno == EINTR) continue;
      else fatal(errno, "error reading from pipe");
    }
    output->sink->write(output->sink, buffer, n);
  }
  xclose(p[0]);
  while((n = waitpid(pid, &w, 0)) < 0 && errno == EINTR)
    ;
  if(n < 0) fatal(errno, "error calling waitpid");
  if(w)
    error(0, "shell command '%s' %s", args[0], wstat(w));
}

static inline int str2bool(const char *s) {
  return !strcmp(s, "true");
}

static inline const char *bool2str(int n) {
  return n ? "true" : "false";
}

static char *expandarg(const char *arg, dcgi_state *ds) {
  struct dynstr d;
  cgi_sink output;

  dynstr_init(&d);
  output.quote = 0;
  output.sink = sink_dynstr(&d);
  expandstring(&output, arg, ds);
  dynstr_terminate(&d);
  return d.vec;
}

static void exp_prefs(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  struct kvp *k;
  const char *file = expandarg(args[0], ds);
  
  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  substate.first = 1;
  if(disorder_prefs(ds->g->client, file, &k)) return;
  while(k) {
    substate.last = !k->next;
    substate.pref = k;
    expandstring(output, args[1], &substate);
    ++substate.index;
    k = k->next;
    substate.first = 0;
  }
}

static void exp_pref(int attribute((unused)) nargs,
		     char **args,
		     cgi_sink *output,
		     void *u) {
  char *value;
  dcgi_state *ds = u;

  if(!disorder_get(ds->g->client, args[0], args[1], &value))
    cgi_output(output, "%s", value);
}

static void exp_if(int nargs,
		   char **args,
		   cgi_sink *output,
		   void *u) {
  dcgi_state *ds = u;
  int n = str2bool(expandarg(args[0], ds)) ? 1 : 2;
  
  if(n < nargs)
    expandstring(output, args[n], ds);
}

static void exp_and(int nargs,
		    char **args,
		    cgi_sink *output,
		    void *u) {
  dcgi_state *ds = u;
  int n, result = 1;

  for(n = 0; n < nargs; ++n)
    if(!str2bool(expandarg(args[n], ds))) {
      result = 0;
      break;
    }
  sink_printf(output->sink, "%s", bool2str(result));
}

static void exp_or(int nargs,
		   char **args,
		   cgi_sink *output,
		   void *u) {
  dcgi_state *ds = u;
  int n, result = 0;

  for(n = 0; n < nargs; ++n)
    if(str2bool(expandarg(args[n], ds))) {
      result = 1;
      break;
    }
  sink_printf(output->sink, "%s", bool2str(result));
}

static void exp_not(int attribute((unused)) nargs,
		    char **args,
		    cgi_sink *output,
		    void attribute((unused)) *u) {
  sink_printf(output->sink, "%s", bool2str(!str2bool(args[0])));
}

static void exp_isplaying(int attribute((unused)) nargs,
			  char attribute((unused)) **args,
			  cgi_sink *output,
			  void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_PLAYING);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->playing));
}

static void exp_isqueue(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_QUEUE);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->queue));
}

static void exp_isrecent(int attribute((unused)) nargs,
			 char attribute((unused)) **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_RECENT);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->recent));
}

static void exp_isnew(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_NEW);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->nnew));
}

static void exp_id(int attribute((unused)) nargs,
		   char attribute((unused)) **args,
		   cgi_sink *output,
		   void *u) {
  dcgi_state *ds = u;

  if(ds->track)
    cgi_output(output, "%s", ds->track->id);
}

static void exp_track(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;

  if(ds->track)
    cgi_output(output, "%s", ds->track->track);
}

static void exp_parity(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;

  cgi_output(output, "%s", ds->index % 2 ? "odd" : "even");
}

static void exp_comment(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink attribute((unused)) *output,
			void attribute((unused)) *u) {
  /* do nothing */
}

static void exp_prefname(int attribute((unused)) nargs,
			 char attribute((unused)) **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;

  if(ds->pref && ds->pref->name)
    cgi_output(output, "%s", ds->pref->name);
}

static void exp_prefvalue(int attribute((unused)) nargs,
			  char attribute((unused)) **args,
			  cgi_sink *output,
			  void *u) {
  dcgi_state *ds = u;

  if(ds->pref && ds->pref->value)
    cgi_output(output, "%s", ds->pref->value);
}

static void exp_isfiles(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_FILES);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->nfiles));
}

static void exp_isdirectories(int attribute((unused)) nargs,
			      char attribute((unused)) **args,
			      cgi_sink *output,
			      void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_DIRS);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->ndirs));
}

static void exp_choose(int attribute((unused)) nargs,
		       char **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  int nfiles, n;
  char **files;
  struct entry *e;
  const char *type, *what = expandarg(args[0], ds);

  if(!strcmp(what, "files")) {
    lookups(ds, DC_FILES);
    files = ds->g->files;
    nfiles = ds->g->nfiles;
    type = "track";
  } else if(!strcmp(what, "directories")) {
    lookups(ds, DC_DIRS);
    files = ds->g->dirs;
    nfiles = ds->g->ndirs;
    type = "dir";
  } else {
    error(0, "unknown @choose@ argument '%s'", what);
    return;
  }
  e = xmalloc(nfiles * sizeof (struct entry));
  for(n = 0; n < nfiles; ++n) {
    e[n].path = files[n];
    e[n].sort = trackname_transform(type, files[n], "sort");
    e[n].display = trackname_transform(type, files[n], "display");
  }
  qsort(e, nfiles, sizeof (struct entry), compare_entry);
  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  substate.first = 1;
  for(n = 0; n < nfiles; ++n) {
    substate.last = (n == nfiles - 1);
    substate.index = n;
    substate.entry = &e[n];
    expandstring(output, args[1], &substate);
    substate.first = 0;
  }
}

static void exp_file(int attribute((unused)) nargs,
		     char attribute((unused)) **args,
		     cgi_sink *output,
		     void *u) {
  dcgi_state *ds = u;

  if(ds->entry)
    cgi_output(output, "%s", ds->entry->path);
  else if(ds->track)
    cgi_output(output, "%s", ds->track->track);
  else if(ds->tracks)
    cgi_output(output, "%s", ds->tracks[0]);
}

static void exp_transform(int nargs,
			  char **args,
			  cgi_sink *output,
			  void attribute((unused)) *u) {
  const char *context = nargs > 2 ? args[2] : "display";

  cgi_output(output, "%s", trackname_transform(args[1], args[0], context));
}

static void exp_urlquote(int attribute((unused)) nargs,
			 char **args,
			 cgi_sink *output,
			 void attribute((unused)) *u) {
  cgi_output(output, "%s", urlencodestring(args[0]));
}

static void exp_scratchable(int attribute((unused)) nargs,
			    char attribute((unused)) **args,
			    cgi_sink *output,
			    void attribute((unused)) *u) {
  dcgi_state *ds = u;
  int result;

  if(config->restrictions & RESTRICT_SCRATCH) {
    lookups(ds, DC_PLAYING);
    result = (ds->g->playing
	      && (!ds->g->playing->submitter
		  || !strcmp(ds->g->playing->submitter,
			     disorder_user(ds->g->client))));
  } else
    result = 1;
  sink_printf(output->sink, "%s", bool2str(result));
}

static void exp_removable(int attribute((unused)) nargs,
			  char attribute((unused)) **args,
			  cgi_sink *output,
			  void attribute((unused)) *u) {
  dcgi_state *ds = u;
  int result;

  if(config->restrictions & RESTRICT_REMOVE)
    result = (ds->track
	      && ds->track->submitter
	      && !strcmp(ds->track->submitter,
			 disorder_user(ds->g->client)));
  else
    result = 1;
  sink_printf(output->sink, "%s", bool2str(result));
}

static void exp_navigate(int attribute((unused)) nargs,
			 char **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  const char *path = expandarg(args[0], ds);
  const char *ptr;
  int dirlen;

  if(*path) {
    memset(&substate, 0, sizeof substate);
    substate.g = ds->g;
    ptr = path + 1;			/* skip root */
    dirlen = 0;
    substate.nav_path = path;
    substate.first = 1;
    while(*ptr) {
      while(*ptr && *ptr != '/')
	++ptr;
      substate.last = !*ptr;
      substate.nav_len = ptr - path;
      substate.nav_dirlen = dirlen;
      expandstring(output, args[1], &substate);
      dirlen = substate.nav_len;
      if(*ptr) ++ptr;
      substate.first = 0;
    }
  }
}

static void exp_fullname(int attribute((unused)) nargs,
			 char attribute((unused)) **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;
  cgi_output(output, "%.*s", ds->nav_len, ds->nav_path);
}

static void exp_basename(int nargs,
			 char **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;
  const char *s;
  
  if(nargs) {
    if((s = strrchr(args[0], '/'))) ++s;
    else s = args[0];
    cgi_output(output, "%s", s);
  } else
    cgi_output(output, "%.*s", ds->nav_len - ds->nav_dirlen - 1,
	       ds->nav_path + ds->nav_dirlen + 1);
}

static void exp_dirname(int nargs,
			char **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;
  const char *s;
  
  if(nargs) {
    if((s = strrchr(args[0], '/')))
      cgi_output(output, "%.*s", (int)(s - args[0]), args[0]);
  } else
    cgi_output(output, "%.*s", ds->nav_dirlen, ds->nav_path);
}

static void exp_eq(int attribute((unused)) nargs,
		   char **args,
		   cgi_sink *output,
		   void attribute((unused)) *u) {
  cgi_output(output, "%s", bool2str(!strcmp(args[0], args[1])));
}

static void exp_ne(int attribute((unused)) nargs,
		   char **args,
		   cgi_sink *output,
		   void attribute((unused)) *u) {
  cgi_output(output, "%s", bool2str(strcmp(args[0], args[1])));
}

static void exp_enabled(int attribute((unused)) nargs,
			       char attribute((unused)) **args,
			       cgi_sink *output,
			       void *u) {
  dcgi_state *ds = u;
  int enabled = 0;

  if(ds->g->client)
    disorder_enabled(ds->g->client, &enabled);
  cgi_output(output, "%s", bool2str(enabled));
}

static void exp_random_enabled(int attribute((unused)) nargs,
			       char attribute((unused)) **args,
			       cgi_sink *output,
			       void *u) {
  dcgi_state *ds = u;
  int enabled = 0;

  if(ds->g->client)
    disorder_random_enabled(ds->g->client, &enabled);
  cgi_output(output, "%s", bool2str(enabled));
}

static void exp_trackstate(int attribute((unused)) nargs,
			   char **args,
			   cgi_sink *output,
			   void *u) {
  dcgi_state *ds = u;
  struct queue_entry *q;
  char *track;

  if(disorder_resolve(ds->g->client, &track, args[0])) return;
  lookups(ds, DC_QUEUE|DC_PLAYING);
  if(ds->g->playing && !strcmp(ds->g->playing->track, track))
    cgi_output(output, "playing");
  else {
    for(q = ds->g->queue; q && strcmp(q->track, track); q = q->next)
      ;
    if(q)
      cgi_output(output, "queued");
  }
}

static void exp_thisurl(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void attribute((unused)) *u) {
  kvp_set(&cgi_args, "nonce", nonce());	/* nonces had better differ! */
  cgi_output(output, "%s?%s", config->url, kvp_urlencode(cgi_args, 0));
}

static void exp_isfirst(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;

  sink_printf(output->sink, "%s", bool2str(!!ds->first));
}

static void exp_islast(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;

  sink_printf(output->sink, "%s", bool2str(!!ds->last));
}

static void exp_action(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void attribute((unused)) *u) {
  const char *action = cgi_get("action"), *mgmt;

  if(!action) action = "playing";
  if(!strcmp(action, "playing")
     && (mgmt = cgi_get("mgmt"))
     && !strcmp(mgmt, "true"))
    action = "manage";
  sink_printf(output->sink, "%s", action);
}

static void exp_resolve(int attribute((unused)) nargs,
                      char  **args,
                      cgi_sink *output,
                      void attribute((unused)) *u) {
  dcgi_state *ds = u;
  char *track;
  
  if(!disorder_resolve(ds->g->client, &track, args[0]))
    sink_printf(output->sink, "%s", track);
}
 
static void exp_paused(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  int paused = 0;

  lookups(ds, DC_PLAYING);
  if(ds->g->playing && ds->g->playing->state == playing_paused)
    paused = 1;
  cgi_output(output, "%s", bool2str(paused));
}

static void exp_state(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;

  if(ds->track)
    cgi_output(output, "%s", playing_states[ds->track->state]);
}

static void exp_files(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  const char *nfiles_arg, *directory;
  int nfiles, numfile;
  struct kvp *k;

  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  if((directory = cgi_get("directory"))) {
    /* Prefs for whole directory. */
    lookups(ds, DC_FILES);
    /* Synthesize args for the file list. */
    nfiles = ds->g->nfiles;
    for(numfile = 0; numfile < nfiles; ++numfile) {
      k = xmalloc(sizeof *k);
      byte_xasprintf((char **)&k->name, "%d_file", numfile);
      k->value = ds->g->files[numfile];
      k->next = cgi_args;
      cgi_args = k;
    }
  } else {
    /* Args already present. */
    if((nfiles_arg = cgi_get("files"))) nfiles = atoi(nfiles_arg);
    else nfiles = 1;
  }
  for(numfile = 0; numfile < nfiles; ++numfile) {
    substate.index = numfile;
    expandstring(output, args[0], &substate);
  }
}

static void exp_index(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;

  cgi_output(output, "%d", ds->index);
}

static void exp_nfiles(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  const char *files_arg;

  if(cgi_get("directory")) {
    lookups(ds, DC_FILES);
    cgi_output(output, "%d", ds->g->nfiles);
  } else if((files_arg = cgi_get("files")))
    cgi_output(output, "%s", files_arg);
  else
    cgi_output(output, "1");
}

static void exp_user(int attribute((unused)) nargs,
		     char attribute((unused)) **args,
		     cgi_sink *output,
		     void *u) {
  dcgi_state *const ds = u;

  cgi_output(output, "%s", disorder_user(ds->g->client));
}

static const struct cgi_expansion expansions[] = {
  { "#", 0, INT_MAX, EXP_MAGIC, exp_comment },
  { "action", 0, 0, 0, exp_action },
  { "and", 0, INT_MAX, EXP_MAGIC, exp_and },
  { "arg", 1, 1, 0, exp_arg },
  { "basename", 0, 1, 0, exp_basename },
  { "choose", 2, 2, EXP_MAGIC, exp_choose },
  { "dirname", 0, 1, 0, exp_dirname },
  { "enabled", 0, 0, 0, exp_enabled },
  { "eq", 2, 2, 0, exp_eq },
  { "file", 0, 0, 0, exp_file },
  { "files", 1, 1, EXP_MAGIC, exp_files },
  { "fullname", 0, 0, 0, exp_fullname },
  { "id", 0, 0, 0, exp_id },
  { "if", 2, 3, EXP_MAGIC, exp_if },
  { "include", 1, 1, 0, exp_include },
  { "index", 0, 0, 0, exp_index },
  { "isdirectories", 0, 0, 0, exp_isdirectories },
  { "isfiles", 0, 0, 0, exp_isfiles },
  { "isfirst", 0, 0, 0, exp_isfirst },
  { "islast", 0, 0, 0, exp_islast },
  { "isnew", 0, 0, 0, exp_isnew },
  { "isplaying", 0, 0, 0, exp_isplaying },
  { "isqueue", 0, 0, 0, exp_isqueue },
  { "isrecent", 0, 0, 0, exp_isrecent },
  { "label", 1, 1, 0, exp_label },
  { "length", 0, 0, 0, exp_length },
  { "navigate", 2, 2, EXP_MAGIC, exp_navigate },
  { "ne", 2, 2, 0, exp_ne },
  { "new", 1, 1, EXP_MAGIC, exp_new },
  { "nfiles", 0, 0, 0, exp_nfiles },
  { "nonce", 0, 0, 0, exp_nonce },
  { "not", 1, 1, 0, exp_not },
  { "or", 0, INT_MAX, EXP_MAGIC, exp_or },
  { "parity", 0, 0, 0, exp_parity },
  { "part", 1, 3, 0, exp_part },
  { "paused", 0, 0, 0, exp_paused },
  { "playing", 1, 1, EXP_MAGIC, exp_playing },
  { "pref", 2, 2, 0, exp_pref },
  { "prefname", 0, 0, 0, exp_prefname },
  { "prefs", 2, 2, EXP_MAGIC, exp_prefs },
  { "prefvalue", 0, 0, 0, exp_prefvalue },
  { "queue", 1, 1, EXP_MAGIC, exp_queue },
  { "random-enabled", 0, 0, 0, exp_random_enabled },
  { "recent", 1, 1, EXP_MAGIC, exp_recent },
  { "removable", 0, 0, 0, exp_removable },
  { "resolve", 1, 1, 0, exp_resolve },
  { "scratchable", 0, 0, 0, exp_scratchable },
  { "search", 2, 3, EXP_MAGIC, exp_search },
  { "server-version", 0, 0, 0, exp_server_version },
  { "shell", 1, 1, 0, exp_shell },
  { "state", 0, 0, 0, exp_state },
  { "stats", 0, 0, 0, exp_stats },
  { "thisurl", 0, 0, 0, exp_thisurl },
  { "track", 0, 0, 0, exp_track },
  { "trackstate", 1, 1, 0, exp_trackstate },
  { "transform", 2, 3, 0, exp_transform },
  { "url", 0, 0, 0, exp_url },
  { "urlquote", 1, 1, 0, exp_urlquote },
  { "user", 0, 0, 0, exp_user },
  { "version", 0, 0, 0, exp_version },
  { "volume", 1, 1, 0, exp_volume },
  { "when", 0, 0, 0, exp_when },
  { "who", 0, 0, 0, exp_who }
};

static void expand(cgi_sink *output,
		   const char *template,
		   dcgi_state *ds) {
  cgi_expand(template,
	     expansions, sizeof expansions / sizeof *expansions,
	     output,
	     ds);
}

static void expandstring(cgi_sink *output,
			 const char *string,
			 dcgi_state *ds) {
  cgi_expand_string("",
		    string,
		    expansions, sizeof expansions / sizeof *expansions,
		    output,
		    ds);
}

static void perform_action(cgi_sink *output, dcgi_state *ds,
			   const char *action) {
  int n;

  /* We don't ever want anything to be cached */
  cgi_header(output->sink, "Cache-Control", "no-cache");
  if((n = TABLE_FIND(actions, struct action, name, action)) >= 0)
    actions[n].handler(output, ds);
  else
    expand_template(ds, output, action);
}

void disorder_cgi(cgi_sink *output, dcgi_state *ds) {
  const char *action = cgi_get("action");

  if(!action) action = "playing";
  perform_action(output, ds, action);
}

void disorder_cgi_error(cgi_sink *output, dcgi_state *ds,
			const char *msg) {
  cgi_set_option("error", msg);
  perform_action(output, ds, "error");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
