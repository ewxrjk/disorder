/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2008 Richard Kettlewell
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
/** @file lib/trackdb.h
 * @brief Track database public interface */

#ifndef TRACKDB_H
#define TRACKDB_H

#include <pcre.h>

#include "event.h"
#include "rights.h"

extern const struct cache_type cache_files_type;
extern unsigned long cache_files_hits, cache_files_misses;
/* Cache entry type and tracking for regexp-based lookups */

/** @brief Do not attempt database recovery (trackdb_init()) */
#define TRACKDB_NO_RECOVER 0x0000

/** @brief Attempt normal recovery (trackdb_init()) */
#define TRACKDB_NORMAL_RECOVER 0x0001

/** @brief Attempt catastrophic trcovery (trackdb_init()) */
#define TRACKDB_FATAL_RECOVER 0x0002

/** @brief Mask of recovery bits (trackdb_init()) */
#define TRACKDB_RECOVER_MASK 0x0003

/** @brief Open for database upgrade (trackdb_open()) */
#define TRACKDB_OPEN_FOR_UPGRADE 0x0004

/** @brief Permit upgrade (trackdb_open()) */
#define TRACKDB_CAN_UPGRADE 0x0008

/** @brief Do not permit upgrade (trackdb_open()) */
#define TRACKDB_NO_UPGRADE 0x0000

/** @brief Mask of upgrade bits (trackdb_open()) */
#define TRACKDB_UPGRADE_MASK 0x000C

/** @brief May create database environment (trackdb_init()) */
#define TRACKDB_MAY_CREATE 0x0010

/** @brief Read-only access (trackdb_open()) */
#define TRACKDB_READ_ONLY 0x0020

void trackdb_init(int flags);
void trackdb_deinit(ev_source *ev);
/* close/close environment */

void trackdb_master(struct ev_source *ev);
/* start deadlock manager */

void trackdb_open(int flags);
void trackdb_close(void);
/* open/close track databases */

extern int trackdb_existing_database;

char **trackdb_stats(int *nstatsp);
/* return a list of database stats */

void trackdb_stats_subprocess(struct ev_source *ev,
                              void (*done)(char *data, void *u),
                              void *u);
/* collect stats in background and call done() with results */

int trackdb_set(const char *track,
                const char *name,
                const char *value);
/* set a pref (remove if value=0).  Return 0 t */

const char *trackdb_get(const char *track,
                        const char *name);
/* get a pref */

struct kvp *trackdb_get_all(const char *track);
/* get all prefs */

const char *trackdb_resolve(const char *track);
/* resolve alias - returns a null pointer if not found */

int trackdb_isalias(const char *track);
/* return true if TRACK is an alias */

int trackdb_exists(const char *track);
/* test whether a track exists (perhaps an alias) */

const char *trackdb_random(int tries);
/* Pick a random non-alias track, making at most TRIES attempts.  Returns a
 * null pointer on failure. */

char **trackdb_alltags(void);
/* Return the list of all tags */

const char *trackdb_getpart(const char *track,
                            const char *context,
                            const char *part);
/* get a track name part, like trackname_part(), but taking the database into
 * account. */

const char *trackdb_rawpath(const char *track);
/* get the raw path name for TRACK (might be an alias); returns a null pointer
 * if not found. */

enum trackdb_listable {
  trackdb_files = 1,
  trackdb_directories = 2
};

char **trackdb_list(const char *dir, int *np, enum trackdb_listable what,
                    const pcre *rec);
/* Return the directories and/or files below DIR.  If DIR is a null pointer
 * then concatenate the listing of all collections.
 *
 * If REC is not a null pointer then only names where the basename matches the
 * regexp are returned.
 */

char **trackdb_search(char **wordlist, int nwordlist, int *ntracks);
/* return a list of tracks containing all of the words given.  If you
 * ask for only stopwords you get no tracks. */

void trackdb_rescan(struct ev_source *ev, int recheck,
                    void (*rescanned)(void *ru),
                    void *ru);
/* Start a rescan, if one is not running already */

int trackdb_rescan_cancel(void);
/* interrupt any running rescan.  Return 1 if one was running, else 0. */

void trackdb_gc(void);
/* tidy up old database log files */

void trackdb_set_global(const char *name,
                        const char *value,
                        const char *who);
/* set a global pref (remove if value=0). */

const char *trackdb_get_global(const char *name);
/* get a global pref */

char **trackdb_new(int *ntracksp, int maxtracks);

void trackdb_expire_noticed(time_t when);
void trackdb_old_users(void);
void trackdb_create_root(void);
const char *trackdb_get_password(const char *user);
int trackdb_adduser(const char *user,
                    const char *password,
                    const char *rights,
                    const char *email,
                    const char *confirmation);
int trackdb_deluser(const char *user);
struct kvp *trackdb_getuserinfo(const char *user);
int trackdb_edituserinfo(const char *user,
                         const char *key, const char *value);
char **trackdb_listusers(void);
int trackdb_confirm(const char *user, const char *confirmation,
                    rights_type *rightsp);
int trackdb_readable(void);

typedef void random_callback(struct ev_source *ev,
                             const char *track);
int trackdb_request_random(struct ev_source *ev,
                           random_callback *callback);
void trackdb_add_rescanned(void (*rescanned)(void *ru),
                           void *ru);
int trackdb_rescan_underway(void);

int trackdb_playlist_get(const char *name,
                         const char *who,
                         char ***tracksp,
                         int *ntracksp,
                         char **sharep);
int trackdb_playlist_set(const char *name,
                         const char *who,
                         char **tracks,
                         int ntracks,
                         const char *share);
void trackdb_playlist_list(const char *who,
                           char ***playlistsp,
                           int *nplaylistsp);
int trackdb_playlist_delete(const char *name,
                            const char *who);

#endif /* TRACKDB_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
