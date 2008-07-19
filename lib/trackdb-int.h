/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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

#ifndef TRACKDB_INT_H
#define TRACKDB_INT_H

#include <db.h>

#include "trackdb.h"
#include "kvp.h"

struct vector;                          /* forward declaration */

extern DB_ENV *trackdb_env;

extern DB *trackdb_tracksdb;
extern DB *trackdb_prefsdb;
extern DB *trackdb_searchdb;
extern DB *trackdb_tagsdb;
extern DB *trackdb_noticeddb;
extern DB *trackdb_globaldb;
extern DB *trackdb_usersdb;
extern DB *trackdb_scheduledb;
extern DB *trackdb_playlistsdb;

DBC *trackdb_opencursor(DB *db, DB_TXN *tid);
/* open a transaction */

int trackdb_closecursor(DBC *c);
/* close transaction, returns 0 or DB_LOCK_DEADLOCK */

int trackdb_notice(const char *track,
                   const char *path);
int trackdb_notice_tid(const char *track,
                       const char *path,
                       DB_TXN *tid);
/* notice a track; return DB_NOTFOUND if new, else 0.  _tid can return
 * DB_LOCK_DEADLOCK too. */

int trackdb_obsolete(const char *track, DB_TXN *tid);
/* obsolete a track */

DB_TXN *trackdb_begin_transaction(void);
void trackdb_abort_transaction(DB_TXN *tid);
void trackdb_commit_transaction(DB_TXN *tid);
/* begin, abort or commit a transaction */

/** @brief Evaluate @p expr in a transaction, looping on deadlock
 *
 * @c tid will be the transaction handle.  @p e will be the error code.
 */
#define WITH_TRANSACTION(expr) do {             \
  DB_TXN *tid;                                  \
                                                \
  tid = trackdb_begin_transaction();            \
  while((e = (expr)) == DB_LOCK_DEADLOCK) {     \
    trackdb_abort_transaction(tid);             \
    tid = trackdb_begin_transaction();          \
  }                                             \
  if(e)                                         \
    trackdb_abort_transaction(tid);             \
  else                                          \
    trackdb_commit_transaction(tid);            \
} while(0)

int trackdb_getdata(DB *db,
                    const char *track,
                    struct kvp **kp,
                    DB_TXN *tid);
/* fetch and decode a database entry.  Returns 0, DB_NOTFOUND or
 * DB_LOCK_DEADLOCK. */

int trackdb_putdata(DB *db,
                    const char *track,
                    const struct kvp *k,
                    DB_TXN *tid,
                    u_int32_t flags);
/* encode and store a database entry.  Returns 0, DB_KEYEXIST or
 * DB_LOCK_DEADLOCK. */

int trackdb_delkey(DB *db,
                   const char *track,
                   DB_TXN *tid);
/* delete a database entry.  Returns 0, DB_NOTFOUND or DB_LOCK_DEADLOCK. */

int trackdb_delkeydata(DB *db,
                       const char *word,
                       const char *track,
                       DB_TXN *tid);
/* delete a (key,data) pair.  Returns 0, DB_NOTFOUND or DB_LOCK_DEADLOCK. */

int trackdb_scan(const char *root,
                 int (*callback)(const char *track,
                                 struct kvp *data,
                                 struct kvp *prefs,
                                 void *u,
                                 DB_TXN *tid),
                 void *u,
                 DB_TXN *tid);
/* Call CALLBACK for each non-alias track below ROOT (or all tracks if ROOT is
 * 0).  Return 0 or DB_LOCK_DEADLOCK.  CALLBACK should return 0 on success or
 * EINTR to cancel the scan. */

int trackdb_listkeys(DB *db, struct vector *v, DB_TXN *tid);

/* fill KEY in with S, returns KEY */
static inline DBT *make_key(DBT *key, const char *s) {
  memset(key, 0, sizeof *key);
  key->data = (void *)s;
  key->size = strlen(s);
  return key;
}

/* set DATA up to receive data, returns DATA */
static inline DBT *prepare_data(DBT *data) {
  memset(data, 0, sizeof *data);
  data->flags = DB_DBT_MALLOC;
  return data;
}

/* encode K and store in DATA, returns DATA */
static inline DBT *encode_data(DBT *data, const struct kvp *k) {
  size_t size;
  
  memset(data, 0, sizeof *data);
  data->data = kvp_urlencode(k, &size);
  data->size = size;
  return data;
}

int trackdb_set_global_tid(const char *name,
                           const char *value,
                           DB_TXN *tid);
int trackdb_get_global_tid(const char *name,
                           DB_TXN *tid,
                           const char **rp);

char **parsetags(const char *s);
int tag_intersection(char **a, char **b);
int valid_username(const char *user);

#endif /* TRACKDB_INT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
