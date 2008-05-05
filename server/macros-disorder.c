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
/** @file server/macros-disorder.c
 * @brief DisOrder-specific expansions
 */

#include <config.h>
#include "types.h"

#include "sink.h"
#include "client.h"
#include "cgi.h"
#include "macros-disorder.h"

/** @brief Client to use for DisOrder-specific expansions
 *
 * The caller should arrange for this to be created before any of
 * these expansions are used (if it cannot connect then it's safe to
 * leave it as NULL).
 */
disorder_client *client;

/** @brief Cached data */
static unsigned flags;

#define DC_QUEUE 0x0001
#define DC_PLAYING 0x0002
#define DC_RECENT 0x0004
#define DC_VOLUME 0x0008
#define DC_DIRS 0x0010
#define DC_FILES 0x0020
#define DC_NEW 0x0040
#define DC_RIGHTS 0x0080

static struct queue_entry *queue;
static struct queue_entry *playing;
static struct queue_entry *recent;

static int volume_left;
static int volume_right;

static char **files;
static int nfiles;

static char **dirs;
static int ndirs;

static char **new;
static int nnew;

static rights_type rights;

/** @brief Fetch cachable data */
static void lookup(unsigned want) {
  unsigned need = want ^ (flags & want);
  struct queue_entry *r, *rnext;
  const char *dir, *re;
  char *rights;

  if(!client || !need)
    return;
  if(need & DC_QUEUE)
    disorder_queue(client, &queue);
  if(need & DC_PLAYING)
    disorder_playing(client, &playing);
  if(need & DC_NEW)
    disorder_new_tracks(client, &new, &nnew, 0);
  if(need & DC_RECENT) {
    /* we need to reverse the order of the list */
    disorder_recent(client, &r);
    while(r) {
      rnext = r->next;
      r->next = recent;
      recent = r;
      r = rnext;
    }
  }
  if(need & DC_VOLUME)
    disorder_get_volume(client,
                        &volume_left, &volume_right);
  if(need & (DC_FILES|DC_DIRS)) {
    if(!(dir = cgi_get("directory")))
      dir = "";
    re = cgi_get("regexp");
    if(need & DC_DIRS)
      if(disorder_directories(client, dir, re,
                              &dirs, &ndirs))
        ndirs = 0;
    if(need & DC_FILES)
      if(disorder_files(client, dir, re,
                        &files, &nfiles))
        nfiles = 0;
  }
  if(need & DC_RIGHTS) {
    rights = RIGHT_READ;	/* fail-safe */
    if(!disorder_userinfo(client, disorder_user(client),
                          "rights", &rights))
      parse_rights(rights, &rights, 1);
  }
  flags |= need;
}

/** @brief Locate a track by ID */
static struct queue_entry *findtrack(const char *id) {
  struct queue_entry *q;

  lookups(DC_PLAYING);
  if(playing && !strcmp(playing->id, id))
    return playing;
  lookups(DC_QUEUE);
  for(q = queue; q; q = q->next)
    if(!strcmp(q->id, id))
      return q;
  lookups(DC_RECENT);
  for(q = recent; q; q = q->next)
    if(!strcmp(q->id, id))
      return q;
}

/** @brief Return @p i as a string */
static const char *make_index(int i) {
  char *s;

  byte_xasprintf(&s, "%d", i);
  return s;
}

/* @server-version@
 *
 * Expands to the server's version string, or a (safe to use) error
 * value if the server is unavailable or broken.
 */
static int exp_server_version(int attribute((unused)) nargs,
			      char attribute((unused)) **args,
			      struct sink *output,
			      void attribute((unused)) *u) {
  const char *v;
  
  if(client) {
    if(disorder_version(client, (char **)&v))
      v = "(cannot get version)";
  } else
    v = "(server not running)";
  return sink_write(output, cgi_sgmlquote(v)) < 0 ? -1 : 0;
}

/* @version@
 *
 * Expands to the local version string.
 */
static int exp_version(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       struct sink *output,
		       void attribute((unused)) *u) {
  return sink_write(output,
		    cgi_sgmlquote(disorder_short_version_string)) < 0 ? -1 : 0;
}

/* @url@
 *
 * Expands to the base URL of the web interface.
 */
static int exp_url(int attribute((unused)) nargs,
		   char attribute((unused)) **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  return sink_write(output,
		    cgi_sgmlquote(config->url)) < 0 ? -1 : 0;
}

/* @arg{NAME}@
 *
 * Expands to the CGI argument NAME, or the empty string if there is
 * no such argument.
 */
static int exp_arg(int attribute((unused)) nargs,
		   char **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  const char *s = cgi_get(args[0]);
  if(s)
    return sink_write(output,
		      cgi_sgmlquote(s)) < 0 ? -1 : 0;
  else
    return 0;
}

/* @user@
 *
 * Expands to the logged-in username (which might be "guest"), or to
 * the empty string if not connected.
 */
static int exp_user(int attribute((unused)) nargs,
		    char attribute((unused)) **args,
		    struct sink *output,
		    void attribute((unused)) *u) {
  const char *u;
  
  if(client && (u = disorder_user(client)))
    return sink_write(output, cgi_sgmlquote(u)) < 0 ? -1 : 0;
  return 0;
}

/* @part{TRACK|ID}{PART}{CONTEXT}
 *
 * Expands to a track name part.
 *
 * A track may be identified by name or by queue ID.
 *
 * CONTEXT may be omitted.  If it is then 'display' is assumed.
 *
 * If the CONTEXT is 'short' then the 'display' part is looked up, and the
 * result truncated according to the length defined by the short_display
 * configuration directive.
 */
static int exp_part(int nargs,
		    char **args,
		    struct sink *output,
		    void attribute((unused)) *u) {
  const char *track = args[0], *part = args[1];
  const char *context = nargs > 2 ? args[2] : "display";
  char *s;

  if(track[0] != '/') {
    struct queue_entry *q = findtrack(track);

    if(q)
      track = q->track;
    else
      return 0;
  }
  if(client
     && !disorder_get(client, &s, track,
                      !strcmp(context, "short") ? "display" : context,
                      part))
    return sink_write(output, cgi_sgmlquote(s)) < 0 ? -1 : 0;
  return 0;
}

/* @quote{STRING}
 *
 * SGML-quotes STRING.  Note that most expansion results are already suitable
 * quoted, so this expansion is usually not required.
 */
static int exp_quote(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  return sink_write(output, cgi_sgmlquote(args[0])) < 0 ? -1 : 0;
}

/* @who{ID}
 *
 * Expands to the name of the submitter of track ID, which must be a playing
 * track, in the queue, or in the recent list.
 */
static int exp_who(int attribute((unused)) nargs,
                   char **args,
                   struct sink *output,
                   void attribute((unused)) *u) {
  struct queue_entry *q = findtrack(args[0]);

  if(q && q->submitter)
    return sink_write(output, cgi_sgmlquote(q->submitter)) < 0 ? -1 : 0;
  return 0;
}

/* @when{ID}
 *
 * Expands to the time a track started or is expected to start.  The track must
 * be a playing track, in the queue, or in the recent list.
 */
static int exp_when(int attribute((unused)) nargs,
                   char **args,
                   struct sink *output,
                    void attribute((unused)) *u) {
  struct queue_entry *q = findtrack(args[0]);
  const struct tm *w = 0;

  if(q) {
    switch(q->state) {
    case playing_isscratch:
    case playing_unplayed:
    case playing_random:
      if(q->expected)
	w = localtime(&q->expected);
      break;
    case playing_failed:
    case playing_no_player:
    case playing_ok:
    case playing_scratched:
    case playing_started:
    case playing_paused:
    case playing_quitting:
      if(q->played)
	w = localtime(&q->played);
      break;
    }
    if(w)
      return sink_printf(output, "%d:%02d", w->tm_hour, w->tm_min) < 0 ? -1 : 0;
  }
  return sink_write(output, "&nbsp;") < 0 ? -1 : 0;
}

/* @length{ID|TRACK}
 *
 * Expands to the length of a track, identified by its queue ID or its name.
 * If it is the playing track (identified by ID) then the amount played so far
 * is included.
 */
static int exp_length(int attribute((unused)) nargs,
                   char **args,
                   struct sink *output,
                   void attribute((unused)) *u) {
  struct queue_entry *q;
  long length = 0;
  const char *name;

  if(args[0][0] == '/')
    /* Track identified by name */
    name = args[0];
  else {
    /* Track identified by queue ID */
    if(!(q = findtrack(args[0])))
      return 0;
    if(q->state == play_start || q->state == playing_paused)
      if(sink_printf(output, "%ld:%02ld/", q->sofar / 60, q->sofar % 60) < 0)
        return -1;
    name = q->track;
  }
  if(client && disorder_length(client, name, &length))
    return sink_printf(output, "%ld:%02ld",
                       length / 60, length % 60) < 0 ? -1 : 0;
  return sink_write(output, "&nbsp;") < 0 ? -1 : 0;
}

/* @removable{ID}@
 *
 * Expands to "true" if track ID is removable (or scratchable, if it is the
 * playing track) and "false" otherwise.
 */
static int exp_removable(int attribute((unused)) nargs,
                         char **args,
                         struct sink *output,
                         void attribute((unused)) *u) {
  struct queue_entry *q = findtrack(args[0]);
  /* TODO would be better to reject recent */

  if(!q || !client)
    return mx_bool_result(output, 0);
  lookup(DC_RIGHTS);
  return mx_bool_result(output,
                        (q == playing ? right_scratchable : right_removable)
                            (rights, disorder_user(client), q));
}

/* @movable{ID}@
 *
 * Expands to "true" if track ID is movable and "false" otherwise.
 */
static int exp_movable(int attribute((unused)) nargs,
                       char **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  struct queue_entry *q = findtrack(args[0]);
  /* TODO would be better to recent playing/recent */

  if(!q || !client)
    return mx_bool_result(output, 0);
  lookup(DC_RIGHTS);
  return mx_bool_result(output,
                        right_movable(rights, disorder_user(client), q));
}

/* @playing{TEMPLATE}
 *
 * Expands to TEMPLATE, with:
 * - @id@ expanded to the queue ID of the playing track
 * - @track@ expanded to its UNQUOTED name
 *
 * If no track is playing expands to nothing.
 *
 * TEMPLATE is optional.  If it is left out then instead expands to the queue
 * ID of the playing track.
 */
static int exp_playing(int nargs,
                       const struct mx_node **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  lookup(DC_PLAYING);
  if(!playing)
    return 0;
  if(!nargs)
    return sink_write(output, playing->id) < 0 ? -1 : 0;
  return mx_rewritel(args[0],
                     "id", playing->id,
                     "track", playing->track,
                     (char *)0);
}

/* @queue{TEMPLATE}
 *
 * For each track in the queue, expands TEMPLATE with the following expansions:
 * - @id@ to the queue ID of the track
 * - @track@ to the UNQUOTED track name
 * - @index@ to the track number from 0
 * - @parity@ to "even" or "odd" alternately
 * - @first@ to "true" on the first track and "false" otherwise
 * - @last@ to "true" on the last track and "false" otherwise
 */
static int exp_queue(int attribute((unused)) nargs,
                     const struct mx_node **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  struct queue_entry *q;
  int rc, i;
  
  lookup(DC_QUEUE);
  for(q = queue, i = 0; q; q = q->next, ++i)
    if((rc = mx_rewritel(args[0],
                         "id", q->id,
                         "track", q->track,
                         "index", make_index(i),
                         "parity", i % 2 ? "odd" : "even",
                         "first", q == queue ? "true" : "false",
                         "last", q->next ? "false" : "true",
                         (char *)0)))
      return rc;
  return 0;
}

/* @recent{TEMPLATE}
 *
 * For each track in the recently played list, expands TEMPLATE with the
 * following expansions:
 * - @id@ to the queue ID of the track
 * - @track@  to the UNQUOTED track name
 * - @index@ to the track number from 0
 * - @parity@ to "even" or "odd" alternately
 * - @first@ to "true" on the first track and "false" otherwise
 * - @last@ to "true" on the last track and "false" otherwise
 */
static int exp_recent(int attribute((unused)) nargs,
                      const struct mx_node **args,
                      struct sink *output,
                      void attribute((unused)) *u) {
  struct queue_entry *q;
  int rc, i;
  
  lookup(DC_RECENT);
  for(q = recent, i = 0; q; q = q->next, ++i)
    if((rc = mx_rewritel(args[0],
                         "id", q->id,
                         "track", q->track,
                         "index", make_index(i),
                         "parity", i % 2 ? "odd" : "even",
                         "first", q == recent ? "true" : "false",
                         "last", q->next ? "false" : "true",
                         (char *)0)))
      return rc;
  return 0;
}

/* @new{TEMPLATE}
 *
 * For each track in the newly added list, expands TEMPLATE wit the following
 * expansions:
 * - @track@ to the UNQUOTED track name
 * - @index@ to the track number from 0
 * - @parity@ to "even" or "odd" alternately
 * - @first@ to "true" on the first track and "false" otherwise
 * - @last@ to "true" on the last track and "false" otherwise
 *
 * Note that unlike @playing@, @queue@ and @recent@ which are otherwise
 * superficially similar, there is no @id@ sub-expansion here.
 */
static int exp_new(int attribute((unused)) nargs,
                   const struct mx_node **args,
                   struct sink *output,
                   void attribute((unused)) *u) {
  struct queue_entry *q;
  int rc, i;
  
  lookup(DC_NEW);
  /* TODO perhaps we should generate an ID value for tracks in the new list */
  for(i = 0; i < nnew; ++i)
    if((rc = mx_rewritel(args[0],
                         "track", new[i],
                         "index", make_index(i),
                         "parity", i % 2 ? "odd" : "even",
                         "first", i == 0 ? "true" : "false",
                         "last", i == nnew - 1 ? "false" : "true",
                         (char *)0)))
      return rc;
  return 0;
}

/* @volume{CHANNEL}@
 *
 * Expands to the volume in a given channel.  CHANNEL must be "left" or
 * "right".
 */
static int exp_volume(int attribute((unused)) nargs,
                      char **args,
                      struct sink *output,
                      void attribute((unused)) *u) {
  lookup(DC_VOLUME);
  return sink_write(output, "%d",
                    !strcmp(args[0], "left")
                            ? volume_left : volume_right) < 0 ? -1 : 0;
}

/* @isplaying@
 *
 * Expands to "true" if there is a playing track, otherwise "false".
 */
static int exp_isplaying(int attribute((unused)) nargs,
                         char attribute((unused)) **args,
                         struct sink *output,
                         void attribute((unused)) *u) {
  lookup(DC_PLAYING);
  return mx_bool_result(output, !!playing);
}

/* @isqueue@
 *
 * Expands to "true" if there the queue is nonempty, otherwise "false".
 */
static int exp_isqueue(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  lookup(DC_QUEUE);
  return mx_bool_result(output, !!queue);
}

/* @isrecent@
 *
 * Expands to "true" if there the recently played list is nonempty, otherwise
 * "false".
 */
static int exp_isrecent(int attribute((unused)) nargs,
                        char attribute((unused)) **args,
                        struct sink *output,
                        void attribute((unused)) *u) {
  lookup(DC_RECENT);
  return mx_bool_result(output, !!recent);
}

/* @isnew@
 *
 * Expands to "true" if there the newly added track list is nonempty, otherwise
 * "false".
 */
static int exp_isnew(int attribute((unused)) nargs,
                     char attribute((unused)) **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  lookup(DC_NEW);
  return mx_bool_result(output, !!nnew);
}

/* @pref{TRACK}{KEY}@
 *
 * Expands to a track preference.
 */
static int exp_pref(int attribute((unused)) nargs,
                    char **args,
                    struct sink *output,
                    void attribute((unused)) *u) {
  char *value;

  if(client && !disorder_get(client, args[0], args[1], &value))
    return sink_write(output, cgi_sgmlquote(value)) < 0 ? -1 : 0;
}

/* @prefs{TRACK}{TEMPLATE}@
 *
 * For each track preference of track TRACK, expands TEMPLATE with the
 * following expansions:
 * - @name@ to the UNQUOTED preference name
 * - @index@ to the preference number from 0
 * - @value@ to the UNQUOTED preference value
 * - @parity@ to "even" or "odd" alternately
 * - @first@ to "true" on the first preference and "false" otherwise
 * - @last@ to "true" on the last preference and "false" otherwise
 *
 * Use @quote@ to quote preference names and values where necessary; see below.
 */
static int exp_prefs(int attribute((unused)) nargs,
                     const struct mx_node **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  int rc, i;
  struct kvp *k, *head;
  char *track;

  if((rc = mx_expandstr(args[0], &track, u, "argument #0 (TRACK)")))
    return rc;
  if(!client || disorder_prefs(client, track, &head))
    return 0;
  for(k = head, i = 0; k; k = k->next, ++i)
    if((rc = mx_rewritel(args[1],
                         "index", make_index(i),
                         "parity", i % 2 ? "odd" : "even",
                         "name", k->name,
                         "value", k->value,
                         "first", k == head ? "true" : "false",
                         "last", k->next ? "false" : "true",
                         (char *)0)))
      return rc;
  return 0;
}

/* @transform{TRACK}{TYPE}{CONTEXT}@
 *
 * Transforms a track name (if TYPE is "track") or directory name (if type is
 * "dir").  CONTEXT should be the context, if it is left out then "display" is
 * assumed.
 */
static int exp_transform(int nargs,
                         char **args,
                         struct sink *output,
                         void attribute((unused)) *u) {
  const char *t = trackname_transform(args[1], args[0],
                                      (nargs > 2 ? args[2] : "display")));
  return sink_write(output, cgi_sgmlquote(t)) < 0 ? -1 : 0;
}

/* @enabled@
 *
 * Expands to "true" if playing is enabled, otherwise "false".
 */
static int exp_enabled(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  int enabled = 0;

  if(client)
    disorder_enabled(client, &enabled);
  return mx_bool_result(output, enabled);
}

/* @random-enabled@
 *
 * Expands to "true" if random play is enabled, otherwise "false".
 */
static int exp_enabled(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  int enabled = 0;

  if(client)
    disorder_random_enabled(client, &enabled);
  return mx_bool_result(output, enabled);
}

/* @trackstate{TRACK}@
 *
 * Expands to "playing" if TRACK is currently playing, or "queue" if it is in
 * the queue, otherwise to nothing.
 */
static int exp_trackstate(int attribute((unused)) nargs,
                          char **args,
                          struct sink *output,
                          void attribute((unused)) *u) {
  char *track;
  struct queue_entry *q;

  if(!client)
    return 0;
  if(disorder_resolve(client, &track, args[0]))
    return 0;
  lookup(DC_PLAYING);
  if(playing && !strcmp(track, playing->track))
    return sink_write(output, "playing") < 0 ? -1 : 0;
  lookup(DC_QUEUE);
  for(q = queue; q; q = q->next)
    if(!strcmp(track, q->track))
      return sink_write(output, "queued") < 0 ? -1 : 0;
  return 0;
}

/* @thisurl@
 *
 * Expands to an UNQUOTED URL which points back to the current page.  (NB it
 * might not be byte for byte identical - for instance, CGI arguments might be
 * re-ordered.)
 */
static int exp_thisurl(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  return cgi_thisurl(config->url);
}

/* @resolve{TRACK}@
 *
 * Expands to an UNQUOTED name for the TRACK that is not an alias, or to
 * nothing if it is not a valid track.
 */
static int exp_resolve(int attribute((unused)) nargs,
                       char **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  char *r;

  if(client && !disorder_resolve(client, &r, args[0]))
    return sink_write(output, r) < 0 ? -1 : 0;
  return 0;
}

/* @paused@
 *
 * Expands to "true" if the playing track is paused, to "false" if it is
 * playing (or if there is no playing track at all).
 */
static int exp_paused(int attribute((unused)) nargs,
                      char attribute((unused)) **args,
                      struct sink *output,
                      void attribute((unused)) *u) {
  lookup(DC_PLAYING);
  return mx_bool_result(output, playing && playing->state == playing_paused);
}

/* @state{ID}@
 *
 * Expands to the current state of track ID.
 */
static int exp_state(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  struct queue_entry *q = findtrack(args[0]);

  if(q)
    return sink_write(output, playing_states[q->state]) < 0 ? -1 : 0;
  return 0;
}

/* @right{RIGHT}{WITH-RIGHT}{WITHOUT-RIGHT}@
 *
 * Expands to WITH-RIGHT if the current user has right RIGHT, otherwise to
 * WITHOUT-RIGHT.  The WITHOUT-RIGHT argument may be left out.
 *
 * If both WITH-RIGHT and WITHOUT-RIGHT are left out then expands to "true" if
 * the user has the right and "false" otherwise.
 *
 * If there is no connection to the server then expands to nothing (in all
 * cases).
 */
static int exp_right(int nargs,
                     const struct mx_node **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  char *right;
  rights_type r;

  if(!client)
    return 0;
  lookup(DC_RIGHTS);
  if((rc = mx_expandstr(args[0], &rightname, u, "argument #0 (RIGHT)")))
    return rc;
  if(parse_rights(right, &r, 1/*report*/))
    return 0;
  /* Single-argument form */
  if(nargs == 1)
    return mx_bool_result(output, !!(r & rights));
  /* Multiple argument form */
  if(r & rights)
    return mx_expandl(args[1], (char *)0);
  if(nargs == 3)
    return mx_expandl(args[2], (char *)0);
  return 0;
}

/* @userinfo{PROPERTY}@
 *
 * Expands to the named property of the current user.
 */
static int exp_userinfo(int attribute((unused)) nargs,
                        char **args,
                        struct sink *output,
                        void attribute((unused)) *u) {
  char *v;

  if(client && !disorder_userinfo(client, disorder_user(client), args[0], &v))
    return sink_write(output, v) < 0 ? -1 : 0;
  return 0;
}

/** @brief Register DisOrder-specific expansions */
void register_disorder_expansions(void) {
  mx_register(exp_arg, 1, 1, "arg");
  mx_register(exp_enabled, 0, 0, "enabled");
  mx_register(exp_isnew, 0, 0, "isnew");
  mx_register(exp_isplaying, 0, 0, "isplaying");
  mx_register(exp_isqueue, 0, 0, "isplaying");
  mx_register(exp_isrecent, 0, 0, "isrecent");
  mx_register(exp_length, 1, 1, "length");
  mx_register(exp_movable, 1, 1, "movable");
  mx_register(exp_part, 2, 3, "part");
  mx_register(exp_pref, 2, 2, "pref");
  mx_register(exp_quote, 1, 1, "quote");
  mx_register(exp_random_enabled, 0, 0, "random-enabled");
  mx_register(exp_removable, 1, 1, "removable");
  mx_register(exp_resolve, 1, 1, "resolve");
  mx_register(exp_right, 1, 3, "right");
  mx_register(exp_server_version, 0, 0, "server-version");
  mx_register(exp_state, 1, 1, "state");
  mx_register(exp_thisurl, 0, 0, "thisurl");
  mx_register(exp_trackstate, 1, 1, "trackstate");
  mx_register(exp_transform, 2, 3, "transform");
  mx_register(exp_url, 0, 0, "url");
  mx_register(exp_user, 0, 0, "user");
  mx_register(exp_userinfo, 1, 1, "userinfo");
  mx_register(exp_version, 0, 0, "version");
  mx_register(exp_volume, 1, 1, "volume");
  mx_register(exp_when, 1, 1, "when");
  mx_register(exp_who, 1, 1, "who");
  mx_register_magic(exp_new, 1, 1, "new");
  mx_register_magic(exp_playing, 0, 1, "playing");
  mx_register_magic(exp_prefs, 2, 2, "prefs");
  mx_register_magic(exp_queue, 1, 1, "queue");
  mx_register_magic(exp_recent, 1, 1, "recent");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
