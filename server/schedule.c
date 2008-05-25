/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file server/schedule.c
 * @brief Scheduled events
 *
 * @ref trackdb_scheduledb is a mapping from ID strings to encoded
 * key-value pairs called 'actiondata'.
 *
 * Possible actiondata keys are:
 * - @b when: when to perform this action (required)
 * - @b who: originator for action (required)
 * - @b action: action to perform (required)
 * - @b track: for @c action=play, the track to play
 * - @b key: for @c action=set-global, the global pref to set
 * - @b value: for @c action=set-global, the value to set (omit to unset)
 * - @b priority: the importance of this action
 * - @b recurs: how the event recurs; NOT IMPLEMENTED
 * - ...others to be defined
 *
 * Possible actions are:
 * - @b play: play a track
 * - @b set-global: set or unset a global pref
 * - ...others to be defined
 *
 * Possible priorities are:
 * - @b junk: junk actions that are in the past at startup are discarded
 * - @b normal: normal actions that are in the past at startup are run
 *   immediately.  (This the default.)
 * - ...others to be defined
 *
 * On startup the schedule database is read and a timeout set on the event loop
 * for each action.  Similarly when an action is added, a timeout is set on the
 * event loop.  The timeout has the ID attached as user data so that the action
 * can easily be found again.
 *
 * Recurring events are NOT IMPLEMENTED yet but this is the proposed
 * interface:
 *
 * Recurring events are updated with a new 'when' field when they are processed
 * (event on error).  Non-recurring events are just deleted after processing.
 *
 * The recurs field is a whitespace-delimited list of criteria:
 * - nn:nn or nn:nn:nn define a time of day, in local time.  There must be
 *   at least one of these but can be more than one.
 * - a day name (monday, tuesday, ...) defines the days of the week on
 *   which the event will recur.  There can be more than one.
 * - a day number and month name (1 january, 5 february, ...) defines
 *   the days of the year on which the event will recur.  There can be
 *   more than one of these.
 *
 * Day and month names are case insensitive.  Multiple languages are
 * likely to be supported, especially if people send me pointers to
 * their month and day names.  Abbreviations are NOT supported, as
 * there is more of a risk of a clash between different languages that
 * way.
 *
 * If there are no week or year days then the event recurs every day.
 *
 * If there are both week and year days then the union of them is
 * taken, rather than the intersection.
 *
 * TODO: support recurring events.
 *
 * TODO: add disorder-dump support
 */
#include "disorder-server.h"

static int schedule_trigger(ev_source *ev,
			    const struct timeval *now,
			    void *u);
static int schedule_lookup(const char *id,
			   struct kvp *actiondata);

/** @brief List of required fields in a scheduled event */
static const char *const schedule_required[] = {"when", "who", "action"};

/** @brief Number of elements in @ref schedule_required */
#define NREQUIRED (int)(sizeof schedule_required / sizeof *schedule_required)

/** @brief Parse a scheduled event key and data
 * @param k Pointer to key
 * @param whenp Where to store timestamp
 * @return 0 on success, non-0 on error
 *
 * Rejects entries that are invalid in various ways.
 */
static int schedule_parse(const DBT *k,
			  const DBT *d,
			  char **idp,
			  struct kvp **actiondatap,
			  time_t *whenp) {
  char *id;
  struct kvp *actiondata;
  int n;

  /* Reject bogus keys */
  if(!k->size || k->size > 128) {
    error(0, "bogus schedule.db key (%lu bytes)", (unsigned long)k->size);
    return -1;
  }
  id = xstrndup(k->data, k->size);
  actiondata = kvp_urldecode(d->data, d->size);
  /* Reject items without the required fields */
  for(n = 0; n < NREQUIRED; ++n) {
    if(!kvp_get(actiondata, schedule_required[n])) {
      error(0, "scheduled event %s: missing required field '%s'",
	    id, schedule_required[n]);
      return -1;
    }
  }
  /* Return the results */
  if(idp)
    *idp = id;
  if(actiondatap)
    *actiondatap = actiondata;
  if(whenp)
    *whenp = (time_t)atoll(kvp_get(actiondata, "when"));
  return 0;
}

/** @brief Delete via a cursor
 * @return 0 or @c DB_LOCK_DEADLOCK */
static int cdel(DBC *cursor) {
  int err;

  switch(err = cursor->c_del(cursor, 0)) {
  case 0:
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error deleting from schedule.db: %s", db_strerror(err));
    break;
  default:
    fatal(0, "error deleting from schedule.db: %s", db_strerror(err));
  }
  return err;
}

/** @brief Initialize the schedule
 * @param ev Event loop
 * @param tid Transaction ID
 *
 * Sets a callback for all action times except for junk actions that are
 * already in the past, which are discarded.
 */
static int schedule_init_tid(ev_source *ev,
			     DB_TXN *tid) {
  DBC *cursor;
  DBT k, d;
  int err;

  cursor = trackdb_opencursor(trackdb_scheduledb, tid);
  while(!(err = cursor->c_get(cursor, prepare_data(&k),  prepare_data(&d),
                              DB_NEXT))) {
    struct timeval when;
    struct kvp *actiondata;
    char *id;

    /* Parse the key.  We destroy bogus entries on sight. */
    if(schedule_parse(&k, &d, &id, &actiondata, &when.tv_sec)) {
      if((err = cdel(cursor)))
	goto deadlocked;
      continue;
    }
    when.tv_usec = 0;
    /* The action might be in the past */
    if(when.tv_sec < time(0)) {
      const char *priority = kvp_get(actiondata, "priority");

      if(priority && !strcmp(priority, "junk")) {
        /* Junk actions that are in the past are discarded during startup */
	/* TODO recurring events should be handled differently here */
        info("junk event %s is in the past, discarding", id);
	if(cdel(cursor))
	  goto deadlocked;
        /* Skip this time */
        continue;
      }
    }
    /* Arrange a callback when the scheduled event is due */
    ev_timeout(ev, 0/*handlep*/, &when, schedule_trigger, id);
  }
  switch(err) {
  case DB_NOTFOUND:
    err = 0;
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error querying schedule.db: %s", db_strerror(err));
    break;
  default:
    fatal(0, "error querying schedule.db: %s", db_strerror(err));
  }
deadlocked:
  if(trackdb_closecursor(cursor))
    err = DB_LOCK_DEADLOCK;
  return err;
}

/** @brief Initialize the schedule
 * @param ev Event loop
 *
 * Sets a callback for all action times except for junk actions that are
 * already in the past, which are discarded.
 */
void schedule_init(ev_source *ev) {
  int e;
  WITH_TRANSACTION(schedule_init_tid(ev, tid));
}

/******************************************************************************/

/** @brief Create a scheduled event
 * @param ev Event loop
 * @param actiondata Action data
 */
static int schedule_add_tid(const char *id,
			    struct kvp *actiondata,
			    DB_TXN *tid) {
  int err;
  DBT k, d;

  memset(&k, 0, sizeof k);
  k.data = (void *)id;
  k.size = strlen(id);
  switch(err = trackdb_scheduledb->put(trackdb_scheduledb, tid, &k,
                                       encode_data(&d, actiondata),
				       DB_NOOVERWRITE)) {
  case 0:
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error updating schedule.db: %s", db_strerror(err));
    return err;
  case DB_KEYEXIST:
    return err;
  default:
    fatal(0, "error updating schedule.db: %s", db_strerror(err));
  }
  return 0;
}

/** @brief Create a scheduled event
 * @param ev Event loop
 * @param actiondata Action actiondata
 * @return Scheduled event ID or NULL on error
 *
 * Events are rejected if they lack the required fields, if the user
 * is not allowed to perform them or if they are scheduled for a time
 * in the past.
 */
const char *schedule_add(ev_source *ev,
			 struct kvp *actiondata) {
  int e, n;
  const char *id;
  struct timeval when;

  /* TODO: handle recurring events */
  /* Check that the required field are present */
  for(n = 0; n < NREQUIRED; ++n) {
    if(!kvp_get(actiondata, schedule_required[n])) {
      error(0, "new scheduled event is missing required field '%s'",
	    schedule_required[n]);
      return 0;
    }
  }
  /* Check that the user is allowed to do whatever it is */
  if(schedule_lookup("[new]", actiondata) < 0)
    return 0;
  when.tv_sec = atoll(kvp_get(actiondata, "when"));
  when.tv_usec = 0;
  /* Reject events in the past */
  if(when.tv_sec <= time(0)) {
    error(0, "new scheduled event is in the past");
    return 0;
  }
  do {
    id = random_id();
    WITH_TRANSACTION(schedule_add_tid(id, actiondata, tid));
  } while(e == DB_KEYEXIST);
  ev_timeout(ev, 0/*handlep*/, &when, schedule_trigger, (void *)id);
  return id;
}

/******************************************************************************/

/** @brief Get the action data for a scheduled event
 * @param id Event ID
 * @return Event data or NULL
 */
struct kvp *schedule_get(const char *id) {
  int e, n;
  struct kvp *actiondata;
  
  WITH_TRANSACTION(trackdb_getdata(trackdb_scheduledb, id, &actiondata, tid));
  /* Check that the required field are present */
  for(n = 0; n < NREQUIRED; ++n) {
    if(!kvp_get(actiondata, schedule_required[n])) {
      error(0, "scheduled event %s is missing required field '%s'",
	    id, schedule_required[n]);
      return 0;
    }
  }
  return actiondata;
}

/******************************************************************************/

/** @brief Delete a scheduled event
 * @param id Event to delete
 * @return 0 on success, non-0 if it did not exist
 */
int schedule_del(const char *id) {
  int e;

  WITH_TRANSACTION(trackdb_delkey(trackdb_scheduledb, id, tid));
  return e == 0 ? 0 : -1;
}

/******************************************************************************/

/** @brief Get a list of scheduled events
 * @param neventsp Where to put count of events (or NULL)
 * @return 0-terminate list of ID strings
 */
char **schedule_list(int *neventsp) {
  int e;
  struct vector v[1];

  vector_init(v);
  WITH_TRANSACTION(trackdb_listkeys(trackdb_scheduledb, v, tid));
  if(neventsp)
    *neventsp = v->nvec;
  return v->vec;
}

/******************************************************************************/

static void schedule_play(ev_source *ev,
			  const char *id,
			  const char *who,
			  struct kvp *actiondata) {
  const char *track = kvp_get(actiondata, "track");
  struct queue_entry *q;

  /* This stuff has rather a lot in common with c_play() */
  if(!track) {
    error(0, "scheduled event %s: no track field", id);
    return;
  }
  if(!trackdb_exists(track)) {
    error(0, "scheduled event %s: no such track as %s", id, track);
    return;
  }
  if(!(track = trackdb_resolve(track))) {
    error(0, "scheduled event %s: cannot resolve track %s", id, track);
    return;
  }
  info("scheduled event %s: %s play %s", id,  who, track);
  q = queue_add(track, who, WHERE_START);
  queue_write();
  if(q == qhead.next && playing)
    prepare(ev, q);
  play(ev);
}

static void schedule_set_global(ev_source attribute((unused)) *ev,
				const char *id,
				const char *who,
				struct kvp *actiondata) {
  const char *key = kvp_get(actiondata, "key");
  const char *value = kvp_get(actiondata, "value");

  if(!key) {
    error(0, "scheduled event %s: no key field", id);
    return;
  }
  if(key[0] == '_') {
    error(0, "scheduled event %s: cannot set internal global preferences (%s)",
	  id, key);
    return;
  }
  if(value)
    info("scheduled event %s: %s set-global %s=%s", id, who, key, value);
  else
    info("scheduled event %s: %s set-global %s unset", id,  who, key);
  trackdb_set_global(key, value, who);
}

/** @brief Table of schedule actions
 *
 * Must be kept sorted.
 */
static struct {
  const char *name;
  void (*callback)(ev_source *ev,
		   const char *id, const char *who,
		   struct kvp *actiondata);
  rights_type right;
} schedule_actions[] = {
  { "play", schedule_play, RIGHT_PLAY },
  { "set-global", schedule_set_global, RIGHT_GLOBAL_PREFS },
};

/** @brief Look up a scheduled event
 * @param actiondata Event description
 * @return index in schedule_actions[] on success, -1 on error
 *
 * Unknown events are rejected as are those that the user is not allowed to do.
 */
static int schedule_lookup(const char *id,
			   struct kvp *actiondata) {
  const char *who = kvp_get(actiondata, "who");
  const char *action = kvp_get(actiondata, "action");
  const char *rights;
  struct kvp *userinfo;
  rights_type r;
  int n;

  /* Look up the action */
  n = TABLE_FIND(schedule_actions, name, action);
  if(n < 0) {
    error(0, "scheduled event %s: unrecognized action '%s'", id, action);
    return -1;
  }
  /* Find the user */
  if(!(userinfo = trackdb_getuserinfo(who))) {
    error(0, "scheduled event %s: user '%s' does not exist", id, who);
    return -1;
  }
  /* Check that they have suitable rights */
  if(!(rights = kvp_get(userinfo, "rights"))) {
    error(0, "scheduled event %s: user %s' has no rights???", id, who);
    return -1;
  }
  if(parse_rights(rights, &r, 1)) {
    error(0, "scheduled event %s: user %s has invalid rights '%s'",
	  id, who, rights);
    return -1;
  }
  if(!(r & schedule_actions[n].right)) {
    error(0, "scheduled event %s: user %s lacks rights for action %s",
	  id, who, action);
    return -1;
  }
  return n;
}

/** @brief Called when an action is due */
static int schedule_trigger(ev_source *ev,
			    const struct timeval attribute((unused)) *now,
			    void *u) {
  const char *action, *id = u;
  struct kvp *actiondata = schedule_get(id);
  int n;

  if(!actiondata)
    return 0;
  /* schedule_get() enforces these being present */
  action = kvp_get(actiondata, "action");
  /* Look up the action */
  n = schedule_lookup(id, actiondata);
  if(n < 0)
    goto done;
  /* Go ahead and do it */
  schedule_actions[n].callback(ev, id, kvp_get(actiondata, "who"), actiondata);
done:
  /* TODO: rewrite recurring events for their next trigger time,
   * rather than deleting them */
  schedule_del(id);
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
