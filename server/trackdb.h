/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
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

#ifndef TRACKDB_H
#define TRACKDB_H

struct ev_source;

extern const struct cache_type cache_files_type;
extern unsigned long cache_files_hits, cache_files_misses;
/* Cache entry type and tracking for regexp-based lookups */

void trackdb_init(int recover);
#define TRACKDB_NO_RECOVER 0
#define TRACKDB_NORMAL_RECOVER 1
#define TRACKDB_FATAL_RECOVER 2
void trackdb_deinit(void);
/* close/close environment */

void trackdb_master(struct ev_source *ev);
/* start deadlock manager */

void trackdb_open(void);
void trackdb_close(void);
/* open/close track databases */

char **trackdb_stats(int *nstatsp);
/* return a list of database stats */

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

void trackdb_rescan(struct ev_source *ev);
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

#endif /* TRACKDB_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
