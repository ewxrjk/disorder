/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file server/macros-disorder.c
 * @brief DisOrder-specific expansions
 */

#include "disorder-cgi.h"

/** @brief For error template */
const char *dcgi_error_string;

/** @brief For login template */
const char *dcgi_status_string;

/** @brief Return @p i as a string */
static const char *make_index(int i) {
  char *s;

  byte_xasprintf(&s, "%d", i);
  return s;
}

/*! @server-version
 *
 * Expands to the server's version string, or a (safe to use) error
 * value if the server is unavailable or broken.
 */
static int exp_server_version(int attribute((unused)) nargs,
			      char attribute((unused)) **args,
			      struct sink *output,
			      void attribute((unused)) *u) {
  const char *v;
  
  if(dcgi_client) {
    if(disorder_version(dcgi_client, (char **)&v))
      v = "(cannot get version)";
  } else
    v = "(server not running)";
  return sink_writes(output, cgi_sgmlquote(v)) < 0 ? -1 : 0;
}

/*! @version
 *
 * Expands to the local version string.
 */
static int exp_version(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       struct sink *output,
		       void attribute((unused)) *u) {
  return sink_writes(output,
                     cgi_sgmlquote(disorder_short_version_string)) < 0 ? -1 : 0;
}

/*! @url
 *
 * Expands to the base URL of the web interface.
 */
static int exp_url(int attribute((unused)) nargs,
		   char attribute((unused)) **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  return sink_writes(output,
                     cgi_sgmlquote(config->url)) < 0 ? -1 : 0;
}

/*! @arg{NAME}
 *
 * Expands to the UNQUOTED form of CGI argument NAME, or the empty string if
 * there is no such argument.  Use @argq for a quick way to quote the argument.
 */
static int exp_arg(int attribute((unused)) nargs,
		   char **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  const char *s = cgi_get(args[0]);

  if(s)
    return sink_writes(output, s) < 0 ? -1 : 0;
  else
    return 0;
}

/*! @argq{NAME}
 *
 * Expands to the (quoted) form of CGI argument NAME, or the empty string if
 * there is no such argument.  Use @arg for the unquoted argument.
 */
static int exp_argq(int attribute((unused)) nargs,
                    char **args,
                    struct sink *output,
                    void attribute((unused)) *u) {
  const char *s = cgi_get(args[0]);

  if(s)
    return sink_writes(output, cgi_sgmlquote(s)) < 0 ? -1 : 0;
  else
    return 0;
}

/*! @user
 *
 * Expands to the logged-in username (which might be "guest"), or to
 * the empty string if not connected.
 */
static int exp_user(int attribute((unused)) nargs,
		    char attribute((unused)) **args,
		    struct sink *output,
		    void attribute((unused)) *u) {
  const char *user;
  
  if(dcgi_client && (user = disorder_user(dcgi_client)))
    return sink_writes(output, cgi_sgmlquote(user)) < 0 ? -1 : 0;
  return 0;
}

/*! @part{TRACK|ID}{PART}{CONTEXT}
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
  const char *s;

  if(track[0] != '/') {
    struct queue_entry *q = dcgi_findtrack(track);

    if(q)
      track = q->track;
    else
      return 0;
  }
  if(dcgi_client
     && !disorder_part(dcgi_client, (char **)&s,
                       track,
                       !strcmp(context, "short") ? "display" : context,
                       part)) {
    if(!strcmp(context, "short"))
      s = truncate_for_display(s, config->short_display);
    return sink_writes(output, cgi_sgmlquote(s)) < 0 ? -1 : 0;
  }
  return 0;
}

/*! @quote{STRING}
 *
 * SGML-quotes STRING.  Note that most expansion results are already suitable
 * quoted, so this expansion is usually not required.
 */
static int exp_quote(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  return sink_writes(output, cgi_sgmlquote(args[0])) < 0 ? -1 : 0;
}

/*! @who{ID}
 *
 * Expands to the name of the submitter of track ID, which must be a playing
 * track, in the queue, or in the recent list.
 */
static int exp_who(int attribute((unused)) nargs,
                   char **args,
                   struct sink *output,
                   void attribute((unused)) *u) {
  struct queue_entry *q = dcgi_findtrack(args[0]);

  if(q && q->submitter)
    return sink_writes(output, cgi_sgmlquote(q->submitter)) < 0 ? -1 : 0;
  return 0;
}

/*! @when{ID}
 *
 * Expands to the time a track started or is expected to start.  The track must
 * be a playing track, in the queue, or in the recent list.
 */
static int exp_when(int attribute((unused)) nargs,
                   char **args,
                   struct sink *output,
                    void attribute((unused)) *u) {
  struct queue_entry *q = dcgi_findtrack(args[0]);
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
  return sink_writes(output, "&nbsp;") < 0 ? -1 : 0;
}

/*! @length{ID|TRACK}
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
    if(!(q = dcgi_findtrack(args[0])))
      return 0;
    if(q->state == playing_started || q->state == playing_paused)
      if(sink_printf(output, "%ld:%02ld/", q->sofar / 60, q->sofar % 60) < 0)
        return -1;
    name = q->track;
  }
  if(dcgi_client && !disorder_length(dcgi_client, name, &length))
    return sink_printf(output, "%ld:%02ld",
                       length / 60, length % 60) < 0 ? -1 : 0;
  return sink_writes(output, "&nbsp;") < 0 ? -1 : 0;
}

/*! @removable{ID}
 *
 * Expands to "true" if track ID is removable (or scratchable, if it is the
 * playing track) and "false" otherwise.
 */
static int exp_removable(int attribute((unused)) nargs,
                         char **args,
                         struct sink *output,
                         void attribute((unused)) *u) {
  struct queue_entry *q = dcgi_findtrack(args[0]);
  /* TODO would be better to reject recent */

  if(!q || !dcgi_client)
    return mx_bool_result(output, 0);
  dcgi_lookup(DCGI_RIGHTS);
  return mx_bool_result(output,
                        (q == dcgi_playing ? right_scratchable : right_removable)
                            (dcgi_rights, disorder_user(dcgi_client), q));
}

/*! @movable{ID}{DIR}
 *
 * Expands to "true" if track ID is movable and "false" otherwise.
 *
 * DIR (which is optional) should be a non-zero integer.  If it is negative
 * then the intended move is down (later in the queue), if it is positive then
 * the intended move is up (earlier in the queue).  The first track is not
 * movable up and the last track not movable down.
 */
static int exp_movable(int  nargs,
                       char **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  struct queue_entry *q = dcgi_findtrack(args[0]);
  /* TODO would be better to recent playing/recent */

  if(!q || !dcgi_client)
    return mx_bool_result(output, 0);
  if(nargs > 1) {
    const long dir = atoi(args[1]);

    if(dir > 0 && q == dcgi_queue)
      return mx_bool_result(output, 0);
    if(dir < 0 && q->next == 0) 
      return mx_bool_result(output, 0);
  }
  dcgi_lookup(DCGI_RIGHTS);
  return mx_bool_result(output,
                        right_movable(dcgi_rights,
                                      disorder_user(dcgi_client),
                                      q));
}

/*! @playing{TEMPLATE}
 *
 * Expands to TEMPLATE, with the following expansions:
 * - @id: the queue ID of the playing track
 * - @track: the playing track's
 UNQUOTED name
 *
 * If no track is playing expands to nothing.
 *
 * TEMPLATE is optional.  If it is left out then instead expands to the queue
 * ID of the playing track.
 */
static int exp_playing(int nargs,
                       const struct mx_node **args,
                       struct sink *output,
                       void *u) {
  dcgi_lookup(DCGI_PLAYING);
  if(!dcgi_playing)
    return 0;
  if(!nargs)
    return sink_writes(output, dcgi_playing->id) < 0 ? -1 : 0;
  return mx_expand(mx_rewritel(args[0],
                               "id", dcgi_playing->id,
                               "track", dcgi_playing->track,
                               (char *)0),
                   output, u);
}

/*! @queue{TEMPLATE}
 *
 * For each track in the queue, expands TEMPLATE with the following expansions:
 * - @id: the queue ID of the track
 * - @track: the UNQUOTED track name
 * - @index: the track number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first track and "false" otherwise
 * - @last: "true" on the last track and "false" otherwise
 */
static int exp_queue(int attribute((unused)) nargs,
                     const struct mx_node **args,
                     struct sink *output,
                     void *u) {
  struct queue_entry *q;
  int rc, i;
  
  dcgi_lookup(DCGI_QUEUE);
  for(q = dcgi_queue, i = 0; q; q = q->next, ++i)
    if((rc = mx_expand(mx_rewritel(args[0],
                                   "id", q->id,
                                   "track", q->track,
                                   "index", make_index(i),
                                   "parity", i % 2 ? "odd" : "even",
                                   "first", q == dcgi_queue ? "true" : "false",
                                   "last", q->next ? "false" : "true",
                                   (char *)0),
                       output, u)))
      return rc;
  return 0;
}

/*! @recent{TEMPLATE}
 *
 * For each track in the recently played list, expands TEMPLATE with the
 * following expansions:
 * - @id: the queue ID of the track
 * - @track: the UNQUOTED track name
 * - @index: the track number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first track and "false" otherwise
 * - @last: "true" on the last track and "false" otherwise
 */
static int exp_recent(int attribute((unused)) nargs,
                      const struct mx_node **args,
                      struct sink *output,
                      void *u) {
  struct queue_entry *q;
  int rc, i;
  
  dcgi_lookup(DCGI_RECENT);
  for(q = dcgi_recent, i = 0; q; q = q->next, ++i)
    if((rc = mx_expand(mx_rewritel(args[0],
                                   "id", q->id,
                                   "track", q->track,
                                   "index", make_index(i),
                                   "parity", i % 2 ? "odd" : "even",
                                   "first", q == dcgi_recent ? "true" : "false",
                                   "last", q->next ? "false" : "true",
                                   (char *)0),
                       output, u)))
      return rc;
  return 0;
}

/*! @new{TEMPLATE}
 *
 * For each track in the newly added list, expands TEMPLATE wit the following
 * expansions:
 * - @track: the UNQUOTED track name
 * - @index: the track number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first track and "false" otherwise
 * - @last: "true" on the last track and "false" otherwise
 *
 * Note that unlike @playing, @queue and @recent which are otherwise
 * superficially similar, there is no @id sub-expansion here.
 */
static int exp_new(int attribute((unused)) nargs,
                   const struct mx_node **args,
                   struct sink *output,
                   void *u) {
  int rc, i;
  
  dcgi_lookup(DCGI_NEW);
  /* TODO perhaps we should generate an ID value for tracks in the new list */
  for(i = 0; i < dcgi_nnew; ++i)
    if((rc = mx_expand(mx_rewritel(args[0],
                                   "track", dcgi_new[i],
                                   "index", make_index(i),
                                   "parity", i % 2 ? "odd" : "even",
                                   "first", i == 0 ? "true" : "false",
                                   "last", i == dcgi_nnew - 1 ? "false" : "true",
                                   (char *)0),
                       output, u)))
      return rc;
  return 0;
}

/*! @volume{CHANNEL}
 *
 * Expands to the volume in a given channel.  CHANNEL must be "left" or
 * "right".
 */
static int exp_volume(int attribute((unused)) nargs,
                      char **args,
                      struct sink *output,
                      void attribute((unused)) *u) {
  dcgi_lookup(DCGI_VOLUME);
  return sink_printf(output, "%d",
                     !strcmp(args[0], "left")
                         ? dcgi_volume_left : dcgi_volume_right) < 0 ? -1 : 0;
}

/*! @isplaying
 *
 * Expands to "true" if there is a playing track, otherwise "false".
 */
static int exp_isplaying(int attribute((unused)) nargs,
                         char attribute((unused)) **args,
                         struct sink *output,
                         void attribute((unused)) *u) {
  dcgi_lookup(DCGI_PLAYING);
  return mx_bool_result(output, !!dcgi_playing);
}

/*! @isqueue
 *
 * Expands to "true" if there the queue is nonempty, otherwise "false".
 */
static int exp_isqueue(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  dcgi_lookup(DCGI_QUEUE);
  return mx_bool_result(output, !!dcgi_queue);
}

/*! @isrecent@
 *
 * Expands to "true" if there the recently played list is nonempty, otherwise
 * "false".
 */
static int exp_isrecent(int attribute((unused)) nargs,
                        char attribute((unused)) **args,
                        struct sink *output,
                        void attribute((unused)) *u) {
  dcgi_lookup(DCGI_RECENT);
  return mx_bool_result(output, !!dcgi_recent);
}

/*! @isnew
 *
 * Expands to "true" if there the newly added track list is nonempty, otherwise
 * "false".
 */
static int exp_isnew(int attribute((unused)) nargs,
                     char attribute((unused)) **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  dcgi_lookup(DCGI_NEW);
  return mx_bool_result(output, !!dcgi_nnew);
}

/*! @pref{TRACK}{KEY}
 *
 * Expands to a track preference.
 */
static int exp_pref(int attribute((unused)) nargs,
                    char **args,
                    struct sink *output,
                    void attribute((unused)) *u) {
  char *value;

  if(dcgi_client && !disorder_get(dcgi_client, args[0], args[1], &value))
    return sink_writes(output, cgi_sgmlquote(value)) < 0 ? -1 : 0;
  return 0;
}

/*! @prefs{TRACK}{TEMPLATE}
 *
 * For each track preference of track TRACK, expands TEMPLATE with the
 * following expansions:
 * - @name: the UNQUOTED preference name
 * - @index: the preference number from 0
 * - @value: the UNQUOTED preference value
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first preference and "false" otherwise
 * - @last: "true" on the last preference and "false" otherwise
 *
 * Use @quote to quote preference names and values where necessary; see below.
 */
static int exp_prefs(int attribute((unused)) nargs,
                     const struct mx_node **args,
                     struct sink *output,
                     void *u) {
  int rc, i;
  struct kvp *k, *head;
  char *track;

  if((rc = mx_expandstr(args[0], &track, u, "argument #0 (TRACK)")))
    return rc;
  if(!dcgi_client || disorder_prefs(dcgi_client, track, &head))
    return 0;
  for(k = head, i = 0; k; k = k->next, ++i)
    if((rc = mx_expand(mx_rewritel(args[1],
                                   "index", make_index(i),
                                   "parity", i % 2 ? "odd" : "even",
                                   "name", k->name,
                                   "value", k->value,
                                   "first", k == head ? "true" : "false",
                                   "last", k->next ? "false" : "true",
                                   (char *)0),
                       output, u)))
      return rc;
  return 0;
}

/*! @transform{TRACK}{TYPE}{CONTEXT}
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
                                      (nargs > 2 ? args[2] : "display"));
  return sink_writes(output, cgi_sgmlquote(t)) < 0 ? -1 : 0;
}

/*! @enabled@
 *
 * Expands to "true" if playing is enabled, otherwise "false".
 */
static int exp_enabled(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  int e = 0;

  if(dcgi_client)
    disorder_enabled(dcgi_client, &e);
  return mx_bool_result(output, e);
}

/*! @random-enabled
 *
 * Expands to "true" if random play is enabled, otherwise "false".
 */
static int exp_random_enabled(int attribute((unused)) nargs,
                              char attribute((unused)) **args,
                              struct sink *output,
                              void attribute((unused)) *u) {
  int e = 0;

  if(dcgi_client)
    disorder_random_enabled(dcgi_client, &e);
  return mx_bool_result(output, e);
}

/*! @trackstate{TRACK}
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

  if(!dcgi_client)
    return 0;
  if(disorder_resolve(dcgi_client, &track, args[0]))
    return 0;
  dcgi_lookup(DCGI_PLAYING);
  if(dcgi_playing && !strcmp(track, dcgi_playing->track))
    return sink_writes(output, "playing") < 0 ? -1 : 0;
  dcgi_lookup(DCGI_QUEUE);
  for(q = dcgi_queue; q; q = q->next)
    if(!strcmp(track, q->track))
      return sink_writes(output, "queued") < 0 ? -1 : 0;
  return 0;
}

/*! @thisurl
 *
 * Expands to an UNQUOTED URL which points back to the current page.  (NB it
 * might not be byte for byte identical - for instance, CGI arguments might be
 * re-ordered.)
 */
static int exp_thisurl(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  return sink_writes(output, cgi_thisurl(config->url)) < 0 ? -1 : 0;
}

/*! @resolve{TRACK}
 *
 * Expands to an UNQUOTED name for the TRACK that is not an alias, or to
 * nothing if it is not a valid track.
 */
static int exp_resolve(int attribute((unused)) nargs,
                       char **args,
                       struct sink *output,
                       void attribute((unused)) *u) {
  char *r;

  if(dcgi_client && !disorder_resolve(dcgi_client, &r, args[0]))
    return sink_writes(output, r) < 0 ? -1 : 0;
  return 0;
}

/*! @paused
 *
 * Expands to "true" if the playing track is paused, to "false" if it is
 * playing (or if there is no playing track at all).
 */
static int exp_paused(int attribute((unused)) nargs,
                      char attribute((unused)) **args,
                      struct sink *output,
                     void attribute((unused)) *u) {
  dcgi_lookup(DCGI_PLAYING);
  return mx_bool_result(output, (dcgi_playing
                                 && dcgi_playing->state == playing_paused));
}

/*! @state{ID}@
 *
 * Expands to the current state of track ID.
 */
static int exp_state(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  struct queue_entry *q = dcgi_findtrack(args[0]);

  if(q)
    return sink_writes(output, playing_states[q->state]) < 0 ? -1 : 0;
  return 0;
}

/*! @right{RIGHT}{WITH-RIGHT}{WITHOUT-RIGHT}@
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
                     void *u) {
  char *right;
  rights_type r;
  int rc;

  if(!dcgi_client)
    return 0;
  dcgi_lookup(DCGI_RIGHTS);
  if((rc = mx_expandstr(args[0], &right, u, "argument #0 (RIGHT)")))
    return rc;
  if(parse_rights(right, &r, 1/*report*/))
    return 0;
  /* Single-argument form */
  if(nargs == 1)
    return mx_bool_result(output, !!(r & dcgi_rights));
  /* Multiple argument form */
  if(r & dcgi_rights)
    return mx_expand(args[1], output, u);
  if(nargs == 3)
    return mx_expand(args[2], output, u);
  return 0;
}

/*! @userinfo{PROPERTY}
 *
 * Expands to the named property of the current user.
 */
static int exp_userinfo(int attribute((unused)) nargs,
                        char **args,
                        struct sink *output,
                        void attribute((unused)) *u) {
  char *v;

  if(dcgi_client
     && !disorder_userinfo(dcgi_client, disorder_user(dcgi_client),
                           args[0], &v))
    return sink_writes(output, v) < 0 ? -1 : 0;
  return 0;
}

/*! @error
 *
 * Expands to the latest error string.
 */
static int exp_error(int attribute((unused)) nargs,
                     char attribute((unused)) **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  return sink_writes(output, dcgi_error_string ? dcgi_error_string : "")
              < 0 ? -1 : 0;
}

/*! @status
 *
 * Expands to the latest status string.
 */
static int exp_status(int attribute((unused)) nargs,
                      char attribute((unused)) **args,
                      struct sink *output,
                      void attribute((unused)) *u) {
  return sink_writes(output, dcgi_status_string ? dcgi_status_string : "")
              < 0 ? -1 : 0;
}

/*! @image{NAME}
 *
 * Expands to the URL of the image called NAME.
 *
 * Firstly if there is a label called images.NAME then the image stem will be
 * the value of that label.  Otherwise the stem will be NAME.png.
 *
 * If the label url.static exists then it will give the base URL for images.
 * Otherwise the base url will be /disorder/.
 */
static int exp_image(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  const char *url, *stem;
  char *labelname;

  /* Compute the stem */
  byte_xasprintf(&labelname, "images.%s", args[0]);
  if(option_label_exists(labelname))
    stem = option_label(labelname);
  else
    byte_xasprintf((char **)&stem, "%s.png", args[0]);
  /* If the stem looks like it's reasonalby complete, use that */
  if(stem[0] == '/'
     || !strncmp(stem, "http:", 5)
     || !strncmp(stem, "https:", 6))
    url = stem;
  else {
    /* Compute the URL */
    if(option_label_exists("url.static"))
      byte_xasprintf((char **)&url, "%s/%s",
                     option_label("url.static"), stem);
    else
      /* Default base is /disorder */
      byte_xasprintf((char **)&url, "/disorder/%s", stem);
  }
  return sink_writes(output, cgi_sgmlquote(url)) < 0 ? -1 : 0;
}

/** @brief Implementation of exp_tracks() and exp_dirs() */
static int exp__files_dirs(int nargs,
                           const struct mx_node **args,
                           struct sink *output,
                           void *u,
                           const char *type,
                           int (*fn)(disorder_client *client,
                                     const char *name,
                                     const char *re,
                                     char ***vecp,
                                     int *nvecp)) {
  char **tracks, *dir, *re;
  int n, ntracks, rc;
  const struct mx_node *m;
  struct tracksort_data *tsd;

  if((rc = mx_expandstr(args[0], &dir, u, "argument #0 (DIR)")))
    return rc;
  if(nargs == 3)  {
    if((rc = mx_expandstr(args[1], &re, u, "argument #1 (RE)")))
      return rc;
    m = args[2];
  } else {
    re = 0;
    m = args[1];
  }
  if(!dcgi_client)
    return 0;
  /* Get the list */
  if(fn(dcgi_client, dir, re, &tracks, &ntracks))
    return 0;
  /* Sort it.  NB trackname_transform() does not go to the server. */
  tsd = tracksort_init(ntracks, tracks, type);
  /* Expand the subsiduary templates.  We chuck in @sort and @display because
   * it is particularly easy to do so. */
  for(n = 0; n < ntracks; ++n)
    if((rc = mx_expand(mx_rewritel(m,
                                   "index", make_index(n),
                                   "parity", n % 2 ? "odd" : "even",
                                   "track", tsd[n].track,
                                   "first", n == 0 ? "true" : "false",
                                   "last", n + 1 == ntracks ? "false" : "true",
                                   "sort", tsd[n].sort,
                                   "display", tsd[n].display,
                                   (char *)0),
                       output, u)))
      return rc;
  return 0;

}

/*! @tracks{DIR}{RE}{TEMPLATE}
 *
 * For each track below DIR, expands TEMPLATE with the
 * following expansions:
 * - @track: the UNQUOTED track name
 * - @index: the track number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first track and "false" otherwise
 * - @last: "true" on the last track and "false" otherwise
 * - @sort: the sort key for this track
 * - @display: the UNQUOTED display string for this track
 *
 * RE is optional and if present is the regexp to match against.
 */
static int exp_tracks(int nargs,
                      const struct mx_node **args,
                      struct sink *output,
                      void *u) {
  return exp__files_dirs(nargs, args, output, u, "track", disorder_files);
}

/*! @dirs{DIR}{RE}{TEMPLATE}
 *
 * For each directory below DIR, expands TEMPLATE with the
 * following expansions:
 * - @track: the UNQUOTED directory name
 * - @index: the directory number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first directory and "false" otherwise
 * - @last: "true" on the last directory and "false" otherwise
 * - @sort: the sort key for this directory
 * - @display: the UNQUOTED display string for this directory
 *
 * RE is optional and if present is the regexp to match against.
 */
static int exp_dirs(int nargs,
                    const struct mx_node **args,
                    struct sink *output,
                    void *u) {
  return exp__files_dirs(nargs, args, output, u, "dir", disorder_directories);
}

static int exp__search_shim(disorder_client *c, const char *terms,
                            const char attribute((unused)) *re,
                            char ***vecp, int *nvecp) {
  return disorder_search(c, terms, vecp, nvecp);
}

/*! @search{KEYWORDS}{TEMPLATE}
 *
 * For each track matching KEYWORDS, expands TEMPLATE with the
 * following expansions:
 * - @track: the UNQUOTED directory name
 * - @index: the directory number from 0
 * - @parity: "even" or "odd" alternately
 * - @first: "true" on the first directory and "false" otherwise
 * - @last: "true" on the last directory and "false" otherwise
 * - @sort: the sort key for this track
 * - @display: the UNQUOTED display string for this track
 */
static int exp_search(int nargs,
                      const struct mx_node **args,
                      struct sink *output,
                      void *u) {
  return exp__files_dirs(nargs, args, output, u, "track", exp__search_shim);
}

/*! @label{NAME}
 *
 * Expands to label NAME from options.labels.  Undefined lables expand to the
 * last dot-separated component, e.g. X.Y.Z to Z.
 */
static int exp_label(int attribute((unused)) nargs,
                     char **args,
                     struct sink *output,
                     void attribute((unused)) *u) {
  return sink_writes(output, option_label(args[0])) < 0 ? -1 : 0;
}

/*! @breadcrumbs{DIR}{TEMPLATE}
 *
 * Expands TEMPLATE for each directory in the path up to DIR, excluding the root
 * but including DIR itself, with the following expansions:
 * - @dir: the directory
 * - @last: "true" if this is the last directory, otherwise "false"
 *
 * DIR must be an absolute path.
 */
static int exp_breadcrumbs(int attribute((unused)) nargs,
                           const struct mx_node **args,
                           struct sink *output,
                           void attribute((unused)) *u) {
  int rc;
  char *dir, *parent, *ptr;
  
  if((rc = mx_expandstr(args[0], &dir, u, "argument #0 (DIR)")))
    return rc;
  /* Reject relative paths */
  if(dir[0] != '/') {
    error(0, "breadcrumbs: '%s' is a relative path", dir);
    return 0;
  }
  /* Skip the root */
  ptr = dir + 1;
  while(*ptr) {
    /* Find the end of this directory */
    while(*ptr && *ptr != '/')
      ++ptr;
    parent = xstrndup(dir, ptr - dir);
    if((rc = mx_expand(mx_rewritel(args[1],
                                   "dir", parent,
                                   "last", *ptr ? "false" : "true",
                                   (char *)0),
                       output, u)))
      return rc;
    if(*ptr)
      ++ptr;
  }
  return 0;
}
  
/** @brief Register DisOrder-specific expansions */
void dcgi_expansions(void) {
  mx_register("arg", 1, 1, exp_arg);
  mx_register("argq", 1, 1, exp_argq);
  mx_register("enabled", 0, 0, exp_enabled);
  mx_register("error", 0, 0, exp_error);
  mx_register("image", 1, 1, exp_image);
  mx_register("isnew", 0, 0, exp_isnew);
  mx_register("isplaying", 0, 0, exp_isplaying);
  mx_register("isqueue", 0, 0, exp_isqueue);
  mx_register("isrecent", 0, 0, exp_isrecent);
  mx_register("label",  1, 1, exp_label);
  mx_register("length", 1, 1, exp_length);
  mx_register("movable", 1, 2, exp_movable);
  mx_register("part", 2, 3, exp_part);
  mx_register("paused", 0, 0, exp_paused);
  mx_register("pref", 2, 2, exp_pref);
  mx_register("quote", 1, 1, exp_quote);
  mx_register("random-enabled", 0, 0, exp_random_enabled);
  mx_register("removable", 1, 1, exp_removable);
  mx_register("resolve", 1, 1, exp_resolve);
  mx_register("server-version", 0, 0, exp_server_version);
  mx_register("state", 1, 1, exp_state);
  mx_register("status", 0, 0, exp_status);
  mx_register("thisurl", 0, 0, exp_thisurl);
  mx_register("trackstate", 1, 1, exp_trackstate);
  mx_register("transform", 2, 3, exp_transform);
  mx_register("url", 0, 0, exp_url);
  mx_register("user", 0, 0, exp_user);
  mx_register("userinfo", 1, 1, exp_userinfo);
  mx_register("version", 0, 0, exp_version);
  mx_register("volume", 1, 1, exp_volume);
  mx_register("when", 1, 1, exp_when);
  mx_register("who", 1, 1, exp_who);
  mx_register_magic("breadcrumbs", 2, 2, exp_breadcrumbs);
  mx_register_magic("dirs", 2, 3, exp_dirs);
  mx_register_magic("new", 1, 1, exp_new);
  mx_register_magic("playing", 0, 1, exp_playing);
  mx_register_magic("prefs", 2, 2, exp_prefs);
  mx_register_magic("queue", 1, 1, exp_queue);
  mx_register_magic("recent", 1, 1, exp_recent);
  mx_register_magic("right", 1, 3, exp_right);
  mx_register_magic("search", 2, 2, exp_search);
  mx_register_magic("tracks", 2, 3, exp_tracks);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
