/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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

#include <string.h>
#include <getopt.h>
#include <db.h>
#include <locale.h>
#include <errno.h>
#include <syslog.h>
#include <pcre.h>
#include <unistd.h>

#include "syscalls.h"
#include "log.h"
#include "defs.h"
#include "kvp.h"
#include "trackdb.h"
#include "trackdb-int.h"
#include "mem.h"
#include "configuration.h"
#include "unicode.h"

static DB_TXN *global_tid;

#define BADKEY_WARN 0
#define BADKEY_FAIL 1
#define BADKEY_DELETE 2

/** @brief Bad key behavior */
static int badkey = BADKEY_WARN;

static long aliases_removed, keys_normalized, values_normalized, renoticed;
static long keys_already_ok, values_already_ok;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { "delete-bad-keys", no_argument, 0, 'x' },
  { "fail-bad-keys", no_argument, 0, 'X' },
  { "syslog", no_argument, 0, 's' },
  { "no-syslog", no_argument, 0, 'S' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-dbupgrade [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Force logging\n"
          "  --delete-bad-keys, -x   Delete unconvertible keys\n"
          "  --fail-bad-keys, -X     Fail if bad keys are found\n"
          "\n"
          "Database upgrader for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder-dbupgrade version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

/** @brief Visit each key in a database and call @p callback
 * @return 0 or DB_LOCK_DEADLOCK
 *
 * @p global_tid must be set.  @p callback should return 0 or DB_LOCK_DEADLOCK.
 */
static int scan_core(const char *name, DB *db,
                     int (*callback)(const char *name, DB *db, DBC *c,
                                     DBT *k, DBT *d)) {
  long count = 0;
  DBC *c = trackdb_opencursor(db, global_tid);
  int err, r = 0;
  DBT k[1], d[1];

  values_normalized = 0;
  keys_normalized = 0;
  aliases_removed = 0;
  renoticed = 0;
  keys_already_ok = 0;
  values_already_ok = 0;
  memset(k, 0, sizeof k);
  memset(d, 0, sizeof d);
  while((err = c->c_get(c, k, d, DB_NEXT)) == 0) {
    if((err = callback(name, db, c, k, d)))
      break;
    ++count;
    if(count % 1000 == 0)
      info("scanning %s, %ld so far", name, count);
  }
  if(err && err != DB_NOTFOUND && err != DB_LOCK_DEADLOCK)
    fatal(0, "%s: error scanning database: %s", name, db_strerror(err));
  r = (err == DB_LOCK_DEADLOCK ? err : 0);
  if((err = c->c_close(c)))
    fatal(0, "%s: error closing cursor: %s", name, db_strerror(err));
  info("%s: %ld entries scanned", name, count);
  if(values_normalized || values_already_ok)
    info("%s: %ld values converted, %ld already ok", name,
         values_normalized, values_already_ok);
  if(keys_normalized || keys_already_ok)
    info("%s: %ld keys converted, %ld already OK", name,
         keys_normalized, keys_already_ok);
  if(aliases_removed)
    info("%s: %ld aliases removed", name, aliases_removed);
  if(renoticed)
    info("%s: %ld tracks re-noticed", name, renoticed);
  return r;
}

/** @brief Visit each key in a database and call @p callback
 *
 * Everything happens inside the @p global_tid tranasction.  @p callback
 * should return 0 or DB_LOCK_DEADLOCK.
 */
static void scan(const char *name, DB *db,
                 int (*callback)(const char *name, DB *db, DBC *c,
                                 DBT *k, DBT *d)) {
  info("scanning %s", name);
  for(;;) {
    global_tid = trackdb_begin_transaction();
    if(scan_core(name, db, callback)) {
      trackdb_abort_transaction(global_tid);
      global_tid = 0;
      error(0, "detected deadlock, restarting scan");
      continue;
    } else {
      trackdb_commit_transaction(global_tid);
      global_tid = 0;
      break;
    }
  }
}

/** @brief Truncate database @p db */
static void truncate_database(const char *name, DB *db) {
  u_int32_t count;
  int err;
  
  do {
    err = db->truncate(db, 0, &count, DB_AUTO_COMMIT);
  } while(err == DB_LOCK_DEADLOCK);
  if(err)
    fatal(0, "error truncating %s: %s", name, db_strerror(err));
}

/* scan callbacks */

static int normalize_keys(const char *name, DB *db, DBC *c,
                          DBT *k, DBT *d) {
  char *knfc;
  size_t nknfc;
  int err;

  /* Find the normalized form of the key */
  knfc = utf8_compose_canon(k->data, k->size, &nknfc);
  if(!knfc) {
    switch(badkey) {
    case BADKEY_WARN:
      error(0, "%s: invalid key: %.*s", name,
            (int)k->size, (const char *)k->data);
      break;
    case BADKEY_DELETE:
      error(0, "%s: deleting invalid key: %.*s", name,
            (int)k->size, (const char *)k->data);
      if((err = c->c_del(c, 0))) {
        if(err != DB_LOCK_DEADLOCK)
          fatal(0, "%s: error removing denormalized key: %s",
                name, db_strerror(err));
        return err;
      }
      break;
    case BADKEY_FAIL:
      fatal(0, "%s: invalid key: %.*s", name,
            (int)k->size, (const char *)k->data);
    }
    return 0;
  }
  /* If the key is already in NFC then do nothing */
  if(nknfc == k->size && !memcmp(k->data, knfc, nknfc)) {
    ++keys_already_ok;
    return 0;
  }
  /* To rename the key we must delete the old one and insert a new one */
  if((err = c->c_del(c, 0))) {
    if(err != DB_LOCK_DEADLOCK)
      fatal(0, "%s: error removing denormalized key: %s",
            name, db_strerror(err));
    return err;
  }
  k->size = nknfc;
  k->data = knfc;
  if((err = db->put(db, global_tid, k, d, DB_NOOVERWRITE))) {
    if(err != DB_LOCK_DEADLOCK)
      fatal(0, "%s: error storing normalized key: %s", name, db_strerror(err));
    return err;
  }
  ++keys_normalized;
  return 0;
}

static int normalize_values(const char *name, DB *db,
                            DBC attribute((unused)) *c,
                            DBT *k, DBT *d) {
  char *dnfc;
  size_t ndnfc;
  int err;

  /* Find the normalized form of the value */
  dnfc = utf8_compose_canon(d->data, d->size, &ndnfc);
  if(!dnfc)
    fatal(0, "%s: cannot convert data to NFC: %.*s", name,
          (int)d->size, (const char *)d->data);
  /* If the key is already in NFC then do nothing */
  if(ndnfc == d->size && !memcmp(d->data, dnfc, ndnfc)) {
    ++values_already_ok;
    return 0;
  }
  d->size = ndnfc;
  d->data = dnfc;
  if((err = db->put(db, global_tid, k, d, 0))) {
    if(err != DB_LOCK_DEADLOCK)
      fatal(0, "%s: error storing normalized data: %s", name, db_strerror(err));
    return err;
  }
  ++values_normalized;
  return 0;
}

static int renotice(const char *name, DB attribute((unused)) *db,
                    DBC attribute((unused)) *c,
                    DBT *k, DBT *d) {
  const struct kvp *const t = kvp_urldecode(d->data, d->size);
  const char *const track = xstrndup(k->data, k->size);
  const char *const path = kvp_get(t, "_path");
  int err;

  if(!path) {
    /* If an alias sorts later than the actual filename then it'll appear
     * in the scan. */
    if(kvp_get(t, "_alias_for"))
      return 0;
    fatal(0, "%s: no '_path' for %.*s", name,
          (int)k->size, (const char *)k->data);
  }
  switch(err = trackdb_notice_tid(track, path, global_tid)) {
  case 0:
    ++renoticed;
    return 0;
  case DB_LOCK_DEADLOCK:
    return err;
  default:
    fatal(0, "%s: unexpected return from trackdb_notice_tid: %s",
          name, db_strerror(err));
  }
}
 
static int remove_aliases_normalize_keys(const char *name, DB *db, DBC *c,
                                         DBT *k, DBT *d) {
  const struct kvp *const t = kvp_urldecode(d->data, d->size);
  int err;

  if(kvp_get(t, "_alias_for")) {
    /* This is an alias.  We remove all the alias entries. */
    if((err = c->c_del(c, 0))) {
      if(err != DB_LOCK_DEADLOCK)
        fatal(0, "%s: error removing alias: %s", name, db_strerror(err));
      return err;
    }
    ++aliases_removed;
    return 0;
  } else if(!kvp_get(t, "_path"))
    error(0, "%s: %.*s has neither _alias_for nor _path", name,
          (int)k->size, (const char *)k->data);
  return normalize_keys(name, db, c, k, d);
}

/** @brief Upgrade the database to the current version
 *
 * This function is supposed to be idempotent, so if it is interrupted
 * half way through it is safe to restart.
 */
static void upgrade(void) {
  char buf[32];

  info("upgrading database to dbversion %ld", config->dbversion);
  /* Normalize keys and values as required.  We will also remove aliases as
   * they will be regenerated when we re-noticed the tracks. */
  info("renormalizing keys");
  scan("tracks.db", trackdb_tracksdb, remove_aliases_normalize_keys);
  scan("prefs.db", trackdb_prefsdb, normalize_keys);
  scan("global.db", trackdb_globaldb, normalize_keys);
  scan("noticed.db", trackdb_noticeddb, normalize_values);
  /* search.db and tags.db we will rebuild */
  info("regenerating search database and aliases");
  truncate_database("search.db", trackdb_searchdb);
  truncate_database("tags.db", trackdb_tagsdb);
  /* Regenerate the search database and aliases */
  scan("tracks.db", trackdb_tracksdb, renotice);
  /* Finally update the database version */
  snprintf(buf, sizeof buf, "%ld", config->dbversion);
  trackdb_set_global("_dbversion", buf, 0);
  info("completed database upgrade");
}

int main(int argc, char **argv) {
  int n, logsyslog = !isatty(2);
  
  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSsxX", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    case 'x': badkey = BADKEY_DELETE; break;
    case 'X': badkey = BADKEY_FAIL; break;
    default: fatal(0, "invalid option");
    }
  }
  /* If stderr is a TTY then log there, otherwise to syslog. */
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  if(config_read(0)) fatal(0, "cannot read configuration");
  /* Open the database */
  trackdb_init(TRACKDB_NO_RECOVER);
  trackdb_open(TRACKDB_OPEN_FOR_UPGRADE);
  upgrade();
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
