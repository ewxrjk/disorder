/*
 * This file is part of DisOrder
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file server/dbparams.c
 * @brief Parameters affecting the database
 *
 * Rescan can regenerate aliases and the search and tag databases but
 * we rather assume that they are either empty or good.  Therefore we
 * need to store anything that can affect these values and erase them
 * if they change.
 *
 * The solution is a global pref _dbparams which contains the hash of
 * the alias, stopword and namepart data.
 */
#include "disorder-server.h"
#include <gcrypt.h>

static int dbparams_cleanup(DB_TXN *tid);
static void h_write_string(gcrypt_hash_handle h,
                           const char *s);
static char *compute_dbparams(void);

/** @brief Check whether database parameters have changed
 *
 * If the database parameters have changed then deletes the search and
 * tag database contents and all aliases.  The subsequent rescan will
 * regenerate them.
 */
void dbparams_check(void) {
  const char *newparams = compute_dbparams();
  const char *oldparams = trackdb_get_global("_dbparams");

  // If the parameters match, return straight away
  if(oldparams && !strcmp(newparams, oldparams))
    return;
  // Log what we're going to do
  for(;;) {
    DB_TXN *tid;
    if(oldparams)
      info("database parameter string changed from %s to %s - removing old data",
           oldparams, newparams);
    else {
      info("new database parameter string %s - removing old data",
           newparams);
      /* This is a slightly annoying case; the global pref wasn't present.  In
       * practice this is almost certainly either an upgrade (with no change to
       * any relevant parameters) or a new installation (with no tracks).
       *
       * The new installation case doesn't matter much; clearing an empty
       * search database and iterating over a likewise track database won't
       * take long.
       *
       * However for upgrade this will throw away a lot of data and laboriously
       * regenerate it, which is rather a shame.
       */
    }
    tid = trackdb_begin_transaction();
    int err = dbparams_cleanup(tid);
    if(!err)
      err = trackdb_set_global_tid("_dbparams", newparams, tid);
    switch(err) {
    case 0:
      trackdb_commit_transaction(tid);
      info("removed old data OK, will regenerate on rescan");
      return;
    case DB_LOCK_DEADLOCK:
      /* Deadlocked, try again */
      trackdb_abort_transaction(tid);
      break;
    default:
      fatal(0, "error updating database: %s", db_strerror(err));
    }
  }
}

/** @brief Clean up databases */
static int dbparams_cleanup(DB_TXN *tid) {
  int err;
  u_int32_t count;

  /* We'll regenerate search.db based on the new set of stopwords */
  if((err = trackdb_searchdb->truncate(trackdb_searchdb, tid, &count, 0))) {
    error(err, "truncating search.db: %s", db_strerror(err));
    return err;
  }
  /* We'll regenerate aliases based on the new alias/namepart settings, so
   * delete all the alias records currently present
   *
   * TODO this looks suspiciously similar to part of dump.c
   */
  DBC *cursor;
  DBT k, d;
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if((err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_FIRST)) == DB_LOCK_DEADLOCK) {
    error(0, "cursor->c_get: %s", db_strerror(err));
    goto done;
  }
  while(err == 0) {
    struct kvp *data = kvp_urldecode(d.data, d.size);
    if(kvp_get(data, "_alias_for")) {
      if((err = cursor->c_del(cursor, 0))) {
        error(0, "cursor->c_del: %s", db_strerror(err));
        goto done;
      }
    }
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d), DB_NEXT);
  }
  if(err == DB_LOCK_DEADLOCK) {
    error(0, "cursor operation: %s", db_strerror(err));
    goto done;
  }
  if(err != DB_NOTFOUND) 
    fatal(0, "cursor->c_get: %s", db_strerror(err));
  err = 0;
done:
  if(trackdb_closecursor(cursor) && !err) err = DB_LOCK_DEADLOCK;
  return err;
}

/** @brief Write a string into a gcrypt hash function
 * @param h Hash handle
 * @param s String to write
 *
 * The 0 terminator is included in the byte written.
 */
static void h_write_string(gcrypt_hash_handle h,
                           const char *s) {
  gcry_md_write(h, s, strlen(s) + 1);
}

/** @brief Compute database parameters hash
 * @return Opaque string encapsulating database parameters
 */
static char *compute_dbparams(void) {
  gcry_error_t e;
  gcrypt_hash_handle h;

  if((e = gcry_md_open(&h, GCRY_MD_SHA256, 0)))
    fatal(0, "gcry_md_open: %s", gcry_strerror(e));
  h_write_string(h, "alias");
  h_write_string(h, config->alias);
  for(int n = 0; n < config->stopword.n; ++n) {
    h_write_string(h, "stopword");
    h_write_string(h, config->stopword.s[n]);
  }
  for(int n = 0; n < config->namepart.n; ++n) {
    h_write_string(h, "namepart");
    h_write_string(h, config->namepart.s[n].part);
    h_write_string(h, config->namepart.s[n].res);
    h_write_string(h, config->namepart.s[n].replace);
    h_write_string(h, config->namepart.s[n].context);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%u", config->namepart.s[n].reflags);
    h_write_string(h, buffer);
  }
  char *result;
  byte_xasprintf(&result, "dbparams-0-sha256:%s", 
                 hex(gcry_md_read(h, GCRY_MD_SHA256), 
                     gcry_md_get_algo_dlen(GCRY_MD_SHA256)));
  gcry_md_close(h);
  return result;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
