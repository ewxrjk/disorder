/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2008 Richard Kettlewell
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
/** @file lib/trackdb.c
 * @brief Track database
 *
 * This file is getting in desparate need of splitting up...
 */

#include "common.h"

#include <db.h>
#include <sys/socket.h>
#include <pcre.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gcrypt.h>

#include "event.h"
#include "mem.h"
#include "kvp.h"
#include "log.h"
#include "vector.h"
#include "rights.h"
#include "trackdb.h"
#include "configuration.h"
#include "syscalls.h"
#include "wstat.h"
#include "printf.h"
#include "filepart.h"
#include "trackname.h"
#include "trackdb-int.h"
#include "logfd.h"
#include "cache.h"
#include "eventlog.h"
#include "hash.h"
#include "unicode.h"
#include "unidata.h"
#include "base64.h"
#include "sendmail.h"

#define RESCAN "disorder-rescan"
#define DEADLOCK "disorder-deadlock"

static const char *getpart(const char *track,
                           const char *context,
                           const char *part,
                           const struct kvp *p,
                           int *used_db);
static char **trackdb_new_tid(int *ntracksp,
                              int maxtracks,
                              DB_TXN *tid);
static int trackdb_expire_noticed_tid(time_t earliest, DB_TXN *tid);
static char *normalize_tag(const char *s, size_t ns);

const struct cache_type cache_files_type = { 86400 };
unsigned long cache_files_hits, cache_files_misses;

/** @brief Set by trackdb_open() */
int trackdb_existing_database;

/* setup and teardown ********************************************************/

static const char *home;                /* home had better not change */
DB_ENV *trackdb_env;			/* db environment */

/** @brief The tracks database
 * - Keys are UTF-8(NFC(unicode(path name)))
 * - Values are encoded key-value pairs
 * - Data is reconstructable data about tracks that currently exist
 */
DB *trackdb_tracksdb;

/** @brief The preferences database
 *
 * - Keys are UTF-8(NFC(unicode(path name)))
 * - Values are encoded key-value pairs
 * - Data is user data about tracks (that might not exist any more)
 * and cannot be reconstructed
 */
DB *trackdb_prefsdb;

/** @brief The search database
 *
 * - Keys are UTF-8(NFKC(casefold(search term)))
 * - Values are UTF-8(NFC(unicode(path name)))
 * - There can be more than one value per key
 * - Presence of key,value means that path matches the search terms
 * - Only tracks fond in @ref trackdb_tracksdb are represented here
 * - This database can be reconstructed, it contains no user data
 */
DB *trackdb_searchdb;

/** @brief The tags database
 *
 * - Keys are UTF-8(NFKC(casefold(tag)))
 * - Values are UTF-8(NFC(unicode(path name)))
 * - There can be more than one value per key
 * - Presence of key,value means that path matches the tag
 * - This is always in sync with the tags preference
 * - This database can be reconstructed, it contains no user data
 */
DB *trackdb_tagsdb;			/* the tags database */

/** @brief The global preferences database
 * - Keys are UTF-8(NFC(preference))
 * - Values are global preference values
 * - Data is user data and cannot be reconstructed
 */
DB *trackdb_globaldb;                   /* global preferences */

/** @brief The noticed database
 * - Keys are 64-bit big-endian timestamps
 * - Values are UTF-8(NFC(unicode(path name)))
 * - There can be more than one value per key
 * - Presence of key,value means that path was added at the given time
 * - Data cannot be reconstructed (but isn't THAT important)
 */
DB *trackdb_noticeddb;                   /* when track noticed */

/** @brief The schedule database
 *
 * - Keys are ID strings, generated at random
 * - Values are encoded key-value pairs
 * - There can be more than one value per key
 * - Data cannot be reconstructed
 *
 * See @ref server/schedule.c for further information.
 */
DB *trackdb_scheduledb;

/** @brief The user database
 * - Keys are usernames
 * - Values are encoded key-value pairs
 * - Data is user data and cannot be reconstructed
 */
DB *trackdb_usersdb;

/** @brief The playlists database
 * - Keys are playlist names
 * - Values are encoded key-value pairs
 * - Data is user data and cannot be reconstructed
 */
DB *trackdb_playlistsdb;

static pid_t db_deadlock_pid = -1;      /* deadlock manager PID */
static pid_t rescan_pid = -1;           /* rescanner PID */
static int initialized, opened;         /* state */

/* comparison function for keys */
static int compare(DB attribute((unused)) *db_,
		   const DBT *a, const DBT *b) {
  return compare_path_raw(a->data, a->size, b->data, b->size);
}

/** @brief Test whether the track database can be read
 * @return 1 if it can, 0 if it cannot
 */
int trackdb_readable(void) {
  char *usersdb;

  byte_xasprintf(&usersdb, "%s/users.db", config->home);
  return access(usersdb, R_OK) == 0;
}

/** @brief Open database environment
 * @param flags Flags word
 *
 * Flags should be one of:
 * - @ref TRACKDB_NO_RECOVER
 * - @ref TRACKDB_NORMAL_RECOVER
 * - @ref TRACKDB_FATAL_RECOVER
 * - @ref TRACKDB_MAY_CREATE
 */
void trackdb_init(int flags) {
  int err;
  const int recover = flags & TRACKDB_RECOVER_MASK;
  static int recover_type[] = { 0, DB_RECOVER, DB_RECOVER_FATAL };

  /* sanity checks */
  assert(initialized == 0);
  ++initialized;
  if(home) {
    if(strcmp(home, config->home))
      fatal(0, "cannot change db home without server restart");
    home = config->home;
  }

  if(flags & TRACKDB_MAY_CREATE) {
    DIR *dp;
    struct dirent *de;
    struct stat st;
    char *p;

    /* Remove world/group permissions on any regular files already in the
     * database directory.  Actually we don't care about all of them but it's
     * easier to just do the lot.  This can be revisited if it's a serious
     * practical inconvenience for anyone.
     *
     * The socket, not being a regular file, is excepted.
     */
    if(!(dp = opendir(config->home)))
      fatal(errno, "error reading %s", config->home);
    while((de = readdir(dp))) {
      byte_xasprintf(&p, "%s/%s", config->home, de->d_name);
      if(lstat(p, &st) == 0
         && S_ISREG(st.st_mode)
         && (st.st_mode & 077)) {
        if(chmod(p, st.st_mode & 07700) < 0)
          fatal(errno, "cannot chmod %s", p);
      }
      xfree(p);
    }
    closedir(dp);
  }

  /* create environment */
  if((err = db_env_create(&trackdb_env, 0))) fatal(0, "db_env_create: %s",
                                                   db_strerror(err));
  if((err = trackdb_env->set_alloc(trackdb_env,
                                   xmalloc_noptr, xrealloc_noptr, xfree)))
    fatal(0, "trackdb_env->set_alloc: %s", db_strerror(err));
  if((err = trackdb_env->set_lk_max_locks(trackdb_env, 10000)))
    fatal(0, "trackdb_env->set_lk_max_locks: %s", db_strerror(err));
  if((err = trackdb_env->set_lk_max_objects(trackdb_env, 10000)))
    fatal(0, "trackdb_env->set_lk_max_objects: %s", db_strerror(err));
  if((err = trackdb_env->open(trackdb_env, config->home,
                              DB_INIT_LOG
                              |DB_INIT_LOCK
                              |DB_INIT_MPOOL
                              |DB_INIT_TXN
                              |DB_CREATE
                              |recover_type[recover],
                              0600)))
    fatal(0, "trackdb_env->open %s: %s", config->home, db_strerror(err));
  trackdb_env->set_errpfx(trackdb_env, "DB");
  trackdb_env->set_errfile(trackdb_env, stderr);
  trackdb_env->set_verbose(trackdb_env, DB_VERB_DEADLOCK, 1);
  trackdb_env->set_verbose(trackdb_env, DB_VERB_RECOVERY, 1);
  trackdb_env->set_verbose(trackdb_env, DB_VERB_REPLICATION, 1);
  D(("initialized database environment"));
}

/* called when deadlock manager terminates */
static int reap_db_deadlock(ev_source attribute((unused)) *ev,
                            pid_t attribute((unused)) pid,
                            int status,
                            const struct rusage attribute((unused)) *rusage,
                            void attribute((unused)) *u) {
  db_deadlock_pid = -1;
  if(initialized)
    fatal(0, "deadlock manager unexpectedly terminated: %s",
          wstat(status));
  else
    D(("deadlock manager terminated: %s", wstat(status)));
  return 0;
}

static pid_t subprogram(ev_source *ev, int outputfd, const char *prog,
                        ...) {
  pid_t pid;
  va_list ap;
  const char *args[1024], **argp, *a;

  argp = args;
  *argp++ = prog;
  *argp++ = "--config";
  *argp++ = configfile;
  *argp++ = debugging ? "--debug" : "--no-debug";
  *argp++ = log_default == &log_syslog ? "--syslog" : "--no-syslog";
  va_start(ap, prog);
  while((a = va_arg(ap, const char *)))
    *argp++ = a;
  va_end(ap);
  *argp = 0;
  /* If we're in the background then trap subprocess stdout/stderr */
  if(!(pid = xfork())) {
    exitfn = _exit;
    if(ev)
      ev_signal_atfork(ev);
    signal(SIGPIPE, SIG_DFL);
    if(outputfd != -1) {
      xdup2(outputfd, 1);
      xclose(outputfd);
    }
    /* ensure we don't leak privilege anywhere */
    if(setuid(geteuid()) < 0)
      fatal(errno, "error calling setuid");
    /* If we were negatively niced, undo it.  We don't bother checking for 
    * error, it's not that important. */
    setpriority(PRIO_PROCESS, 0, 0);
    execvp(prog, (char **)args);
    fatal(errno, "error invoking %s", prog);
  }
  return pid;
}

/* start deadlock manager */
void trackdb_master(ev_source *ev) {
  assert(db_deadlock_pid == -1);
  db_deadlock_pid = subprogram(ev, -1, DEADLOCK, (char *)0);
  ev_child(ev, db_deadlock_pid, 0, reap_db_deadlock, 0);
  D(("started deadlock manager"));
}

/* close environment */
void trackdb_deinit(void) {
  int err;

  /* sanity checks */
  assert(initialized == 1);
  --initialized;

  /* close the environment */
  if((err = trackdb_env->close(trackdb_env, 0)))
    fatal(0, "trackdb_env->close: %s", db_strerror(err));

  if(rescan_pid != -1) {
    /* shut down the rescanner */
    if(kill(rescan_pid, SIGTERM) < 0)
      fatal(errno, "error killing rescanner");
    /* wait for the rescanner to finish */
    while(waitpid(rescan_pid, &err, 0) == -1 && errno == EINTR)
      ;
  }

  /* TODO kill any stats subprocesses */

  /* finally terminate the deadlock manager */
  if(db_deadlock_pid != -1 && kill(db_deadlock_pid, SIGTERM) < 0)
    fatal(errno, "error killing deadlock manager");
  db_deadlock_pid = -1;

  D(("deinitialized database environment"));
}

/* open a specific database */
static DB *open_db(const char *path,
                   u_int32_t dbflags,
                   DBTYPE dbtype,
                   u_int32_t openflags,
                   int mode) {
  int err;
  DB *db;

  D(("open %s", path));
  path = config_get_file(path);
  if((err = db_create(&db, trackdb_env, 0)))
    fatal(0, "db_create %s: %s", path, db_strerror(err));
  if(dbflags)
    if((err = db->set_flags(db, dbflags)))
      fatal(0, "db->set_flags %s: %s", path, db_strerror(err));
  if(dbtype == DB_BTREE)
    if((err = db->set_bt_compare(db, compare)))
      fatal(0, "db->set_bt_compare %s: %s", path, db_strerror(err));
  if((err = db->open(db, 0, path, 0, dbtype,
                     openflags | DB_AUTO_COMMIT, mode))) {
    if((openflags & DB_CREATE) || errno != ENOENT)
      fatal(0, "db->open %s: %s", path, db_strerror(err));
    db->close(db, 0);
    db = 0;
  }
  return db;
}

/** @brief Open track databases
 * @param flags Flags flags word
 *
 * @p flags should have one of:
 * - @p TRACKDB_NO_UPGRADE, if no upgrade should be attempted
 * - @p TRACKDB_CAN_UPGRADE, if an upgrade may be attempted
 * - @p TRACKDB_OPEN_FOR_UPGRADE, if this is disorder-dbupgrade
 * Also it may have:
 * - @p TRACKDB_READ_ONLY, read only access
 */
void trackdb_open(int flags) {
  int err;
  pid_t pid;
  uint32_t dbflags = flags & TRACKDB_READ_ONLY ? DB_RDONLY : DB_CREATE;

  /* sanity checks */
  assert(opened == 0);
  ++opened;
  /* check the database version first */
  trackdb_globaldb = open_db("global.db", 0, DB_HASH, DB_RDONLY, 0666);
  if(trackdb_globaldb) {
    /* This is an existing database */
    const char *s;
    long oldversion;

    s = trackdb_get_global("_dbversion");
    /* Close the database again,  we'll open it property below */
    if((err = trackdb_globaldb->close(trackdb_globaldb, 0)))
      fatal(0, "error closing global.db: %s", db_strerror(err));
    trackdb_globaldb = 0;
    /* Convert version string to an integer */
    oldversion = s ? atol(s) : 1;
    if(oldversion > config->dbversion) {
      /* Database is from the future; we never allow this. */
      fatal(0, "this version of DisOrder is too old for database version %ld",
            oldversion);
    }
    if(oldversion < config->dbversion) {
      /* Database version is out of date */
      switch(flags & TRACKDB_UPGRADE_MASK) {
      case TRACKDB_NO_UPGRADE:
        /* This database needs upgrading but this is not permitted */
        fatal(0, "database needs upgrading from %ld to %ld",
              oldversion, config->dbversion);
      case TRACKDB_CAN_UPGRADE:
        /* This database needs upgrading */
        info("invoking disorder-dbupgrade to upgrade from %ld to %ld",
             oldversion, config->dbversion);
        pid = subprogram(0, -1, "disorder-dbupgrade", (char *)0);
        while(waitpid(pid, &err, 0) == -1 && errno == EINTR)
          ;
        if(err)
          fatal(0, "disorder-dbupgrade %s", wstat(err));
        info("disorder-dbupgrade succeeded");
        break;
      case TRACKDB_OPEN_FOR_UPGRADE:
        break;
      default:
        abort();
      }
    }
    if(oldversion == config->dbversion && (flags & TRACKDB_OPEN_FOR_UPGRADE)) {
      /* This doesn't make any sense */
      fatal(0, "database is already at current version");
    }
    trackdb_existing_database = 1;
  } else {
    if(flags & TRACKDB_OPEN_FOR_UPGRADE) {
      /* Cannot upgrade a new database */
      fatal(0, "cannot upgrade a database that does not exist");
    }
    /* This is a brand new database */
    trackdb_existing_database = 0;
  }
  /* open the databases */
  if(!(trackdb_usersdb = open_db("users.db",
                                 0, DB_HASH, dbflags, 0600)))
    fatal(0, "cannot open users.db");
  trackdb_tracksdb = open_db("tracks.db",
                             DB_RECNUM, DB_BTREE, dbflags, 0666);
  trackdb_searchdb = open_db("search.db",
                             DB_DUP|DB_DUPSORT, DB_HASH, dbflags, 0666);
  trackdb_tagsdb = open_db("tags.db",
                           DB_DUP|DB_DUPSORT, DB_HASH, dbflags, 0666);
  trackdb_prefsdb = open_db("prefs.db", 0, DB_HASH, dbflags, 0666);
  trackdb_globaldb = open_db("global.db", 0, DB_HASH, dbflags, 0666);
  trackdb_noticeddb = open_db("noticed.db",
                             DB_DUPSORT, DB_BTREE, dbflags, 0666);
  trackdb_scheduledb = open_db("schedule.db", 0, DB_HASH, dbflags, 0666);
  trackdb_playlistsdb = open_db("playlists.db", 0, DB_HASH, dbflags, 0666);
  if(!trackdb_existing_database) {
    /* Stash the database version */
    char buf[32];

    assert(!(flags & TRACKDB_OPEN_FOR_UPGRADE));
    snprintf(buf, sizeof buf, "%ld", config->dbversion);
    trackdb_set_global("_dbversion", buf, 0);
  }
  D(("opened databases"));
}

/* close track databases */
void trackdb_close(void) {
  int err;

  /* sanity checks */
  assert(opened == 1);
  --opened;
  if((err = trackdb_tracksdb->close(trackdb_tracksdb, 0)))
    fatal(0, "error closing tracks.db: %s", db_strerror(err));
  if((err = trackdb_searchdb->close(trackdb_searchdb, 0)))
    fatal(0, "error closing search.db: %s", db_strerror(err));
  if((err = trackdb_tagsdb->close(trackdb_tagsdb, 0)))
    fatal(0, "error closing tags.db: %s", db_strerror(err));
  if((err = trackdb_prefsdb->close(trackdb_prefsdb, 0)))
    fatal(0, "error closing prefs.db: %s", db_strerror(err));
  if((err = trackdb_globaldb->close(trackdb_globaldb, 0)))
    fatal(0, "error closing global.db: %s", db_strerror(err));
  if((err = trackdb_noticeddb->close(trackdb_noticeddb, 0)))
    fatal(0, "error closing noticed.db: %s", db_strerror(err));
  if((err = trackdb_scheduledb->close(trackdb_scheduledb, 0)))
    fatal(0, "error closing schedule.db: %s", db_strerror(err));
  if((err = trackdb_usersdb->close(trackdb_usersdb, 0)))
    fatal(0, "error closing users.db: %s", db_strerror(err));
  if((err = trackdb_playlistsdb->close(trackdb_playlistsdb, 0)))
    fatal(0, "error closing playlists.db: %s", db_strerror(err));
  trackdb_tracksdb = trackdb_searchdb = trackdb_prefsdb = 0;
  trackdb_tagsdb = trackdb_globaldb = 0;
  D(("closed databases"));
}

/* generic db routines *******************************************************/

/** @brief Fetch and decode a database entry
 * @param db Database
 * @param track Track name
 * @param kp Where to put decoded list (or NULL if you don't care)
 * @param tid Owning transaction
 * @return 0, @c DB_NOTFOUND or @c DB_LOCK_DEADLOCK
 */
int trackdb_getdata(DB *db,
                    const char *track,
                    struct kvp **kp,
                    DB_TXN *tid) {
  int err;
  DBT key, data;

  switch(err = db->get(db, tid, make_key(&key, track),
                       prepare_data(&data), 0)) {
  case 0:
    if(kp)
      *kp = kvp_urldecode(data.data, data.size);
    return 0;
  case DB_NOTFOUND:
    if(kp)
      *kp = 0;
    return err;
  case DB_LOCK_DEADLOCK:
    error(0, "error querying database: %s", db_strerror(err));
    return err;
  default:
    fatal(0, "error querying database: %s", db_strerror(err));
  }
}

/* encode and store a database entry.  Returns 0, DB_KEYEXIST or
 * DB_LOCK_DEADLOCK. */
int trackdb_putdata(DB *db,
                    const char *track,
                    const struct kvp *k,
                    DB_TXN *tid,
                    u_int32_t flags) {
  int err;
  DBT key, data;

  switch(err = db->put(db, tid, make_key(&key, track),
                       encode_data(&data, k), flags)) {
  case 0:
  case DB_KEYEXIST:
    return err;
  case DB_LOCK_DEADLOCK:
    error(0, "error updating database: %s", db_strerror(err));
    return err;
  default:
    fatal(0, "error updating database: %s", db_strerror(err));
  }
}

/** @brief Delete a database entry
 * @param db Database
 * @param track Key to delete
 * @param tid Transaction ID
 * @return 0, DB_NOTFOUND or DB_LOCK_DEADLOCK
 */
int trackdb_delkey(DB *db,
                   const char *track,
                   DB_TXN *tid) {
  int err;

  DBT key;
  switch(err = db->del(db, tid, make_key(&key, track), 0)) {
  case 0:
  case DB_NOTFOUND:
    return 0;
  case DB_LOCK_DEADLOCK:
    error(0, "error updating database: %s", db_strerror(err));
    return err;
  default:
    fatal(0, "error updating database: %s", db_strerror(err));
  }
}

/* open a database cursor */
DBC *trackdb_opencursor(DB *db, DB_TXN *tid) {
  int err;
  DBC *c;

  switch(err = db->cursor(db, tid, &c, 0)) {
  case 0: break;
  default: fatal(0, "error creating cursor: %s", db_strerror(err));
  }
  return c;
}

/* close a database cursor; returns 0 or DB_LOCK_DEADLOCK */
int trackdb_closecursor(DBC *c) {
  int err;

  if(!c) return 0;
  switch(err = c->c_close(c)) {
  case 0:
    return err;
  case DB_LOCK_DEADLOCK:
    error(0, "error closing cursor: %s", db_strerror(err));
    return err;
  default:
    fatal(0, "error closing cursor: %s", db_strerror(err));
  }
}

/* delete a (key,data) pair.  Returns 0, DB_NOTFOUND or DB_LOCK_DEADLOCK. */
int trackdb_delkeydata(DB *db,
                       const char *word,
                       const char *track,
                       DB_TXN *tid) {
  int err;
  DBC *c;
  DBT key, data;

  c = trackdb_opencursor(db, tid);
  switch(err = c->c_get(c, make_key(&key, word),
                        make_key(&data, track), DB_GET_BOTH)) {
  case 0:
    switch(err = c->c_del(c, 0)) {
    case 0:
      break;
    case DB_KEYEMPTY:
      err = 0;
      break;
    case DB_LOCK_DEADLOCK:
      error(0, "error updating database: %s", db_strerror(err));
      break;
    default:
      fatal(0, "c->c_del: %s", db_strerror(err));
    }
    break;
  case DB_NOTFOUND:
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error updating database: %s", db_strerror(err));
    break;
  default:
    fatal(0, "c->c_get: %s", db_strerror(err));
  }
  if(trackdb_closecursor(c)) err = DB_LOCK_DEADLOCK;
  return err;
}

/* start a transaction */
DB_TXN *trackdb_begin_transaction(void) {
  DB_TXN *tid;
  int err;

  if((err = trackdb_env->txn_begin(trackdb_env, 0, &tid, 0)))
    fatal(0, "trackdb_env->txn_begin: %s", db_strerror(err));
  return tid;
}

/* abort transaction */
void trackdb_abort_transaction(DB_TXN *tid) {
  int err;

  if(tid)
    if((err = tid->abort(tid)))
      fatal(0, "tid->abort: %s", db_strerror(err));
}

/* commit transaction */
void trackdb_commit_transaction(DB_TXN *tid) {
  int err;

  if((err = tid->commit(tid, 0)))
    fatal(0, "tid->commit: %s", db_strerror(err));
}

/* search/tags shared code ***************************************************/

/* comparison function used by dedupe() */
static int wordcmp(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

/* sort and de-dupe VEC */
static char **dedupe(char **vec, int nvec) {
  int m, n;

  qsort(vec, nvec, sizeof (char *), wordcmp);
  m = n = 0;
  if(nvec) {
    vec[m++] = vec[0];
    for(n = 1; n < nvec; ++n)
      if(strcmp(vec[n], vec[m - 1]))
	vec[m++] = vec[n];
  }
  vec[m] = 0;
  return vec;
}

/* update a key/track database.  Returns 0 or DB_DEADLOCK. */
static int register_word(DB *db, const char *what,
                         const char *track, const char *word,
                         DB_TXN *tid) {
  int err;
  DBT key, data;

  switch(err = db->put(db, tid, make_key(&key, word),
                       make_key(&data, track), DB_NODUPDATA)) {
  case 0:
  case DB_KEYEXIST:
    return 0;
  case DB_LOCK_DEADLOCK:
    error(0, "error updating %s.db: %s", what, db_strerror(err));
    return err;
  default:
    fatal(0, "error updating %s.db: %s", what,  db_strerror(err));
  }
}

/* search primitives *********************************************************/

/* return true iff NAME is a trackname_display_ pref */
static int is_display_pref(const char *name) {
  static const char prefix[] = "trackname_display_";
  return !strncmp(name, prefix, (sizeof prefix) - 1);
}

/** @brief Word_Break property tailor that treats underscores as spaces */
static int tailor_underscore_Word_Break_Other(uint32_t c) {
  switch(c) {
  default:
    return -1;
  case 0x005F: /* LOW LINE (SPACING UNDERSCORE) */
    return unicode_Word_Break_Other;
  }
}

/** @brief Remove all combining characters in-place
 * @param s Pointer to start of string
 * @param ns Length of string
 * @return New, possiblby reduced, length
 */
static size_t remove_combining_chars(uint32_t *s, size_t ns) {
  uint32_t *start = s, *t = s, *end = s + ns;

  while(s < end) {
    const uint32_t c = *s++;
    if(!utf32_combining_class(c))
      *t++ = c;
  }
  return t - start;
}

/** @brief Normalize and split a string using a given tailoring */
static void word_split(struct vector *v,
                       const char *s,
                       unicode_property_tailor *pt) {
  size_t nw, nt32, i;
  uint32_t *t32, **w32;

  /* Convert to UTF-32 */
  if(!(t32 = utf8_to_utf32(s, strlen(s), &nt32)))
    return;
  /* Erase case distinctions */
  if(!(t32 = utf32_casefold_compat(t32, nt32, &nt32)))
    return;
  /* Drop combining characters */
  nt32 = remove_combining_chars(t32, nt32);
  /* Split into words, treating _ as a space */
  w32 = utf32_word_split(t32, nt32, &nw, pt);
  /* Convert words back to UTF-8 and append to result */
  for(i = 0; i < nw; ++i)
    vector_append(v, utf32_to_utf8(w32[i], utf32_len(w32[i]), 0));
}

/** @brief Normalize a tag
 * @param s Tag
 * @param ns Length of tag
 * @return Normalized string or NULL on error
 *
 * The return value will be:
 * - case-folded
 * - have no leading or trailing space
 * - have no combining characters
 * - all spacing between words will be a single U+0020 SPACE
 */
static char *normalize_tag(const char *s, size_t ns) {
  uint32_t *s32, **w32;
  size_t ns32, nw32, i;
  struct dynstr d[1];

  if(!(s32 = utf8_to_utf32(s, ns, &ns32)))
    return 0;
  if(!(s32 = utf32_casefold_compat(s32, ns32, &ns32))) /* ->NFKD */
    return 0;
  ns32 = remove_combining_chars(s32, ns32);
  /* Split into words, no Word_Break tailoring */
  w32 = utf32_word_split(s32, ns32, &nw32, 0);
  /* Compose back into a string */
  dynstr_init(d);
  for(i = 0; i < nw32; ++i) {
    if(i)
      dynstr_append(d, ' ');
    dynstr_append_string(d, utf32_to_utf8(w32[i], utf32_len(w32[i]), 0));
  }
  dynstr_terminate(d);
  return d->vec;
}

/* compute the words of a track name */
static char **track_to_words(const char *track,
                             const struct kvp *p) {
  struct vector v;
  const char *rootless = track_rootless(track);

  if(!rootless)
    rootless = track;                   /* bodge */
  vector_init(&v);
  rootless = strip_extension(rootless);
  word_split(&v, strip_extension(rootless), tailor_underscore_Word_Break_Other);
  for(; p; p = p->next)
    if(is_display_pref(p->name))
      word_split(&v, p->value, 0);
  vector_terminate(&v);
  return dedupe(v.vec, v.nvec);
}

/* return nonzero iff WORD is a stopword */
static int stopword(const char *word) {
  int n;

  for(n = 0; n < config->stopword.n
        && strcmp(word, config->stopword.s[n]); ++n)
    ;
  return n < config->stopword.n;
}

/* record that WORD appears in TRACK.  Returns 0 or DB_LOCK_DEADLOCK. */
static int register_search_word(const char *track, const char *word,
                                DB_TXN *tid) {
  if(stopword(word)) return 0;
  return register_word(trackdb_searchdb, "search", track, word, tid);
}

/* Tags **********************************************************************/

/* Return nonzero if C is a valid tag character */
static int tagchar(int c) {
  switch(c) {
  case ',':
    return 0;
  default:
    return c >= ' ';
  }
}

/* Parse and de-dupe a tag list.  If S=0 then assumes "". */
char **parsetags(const char *s) {
  const char *t;
  struct vector v;

  vector_init(&v);
  if(s) {
    /* skip initial separators */
    while(*s && (!tagchar(*s) || *s == ' '))
      ++s;
    while(*s) {
      /* find the extent of the tag */
      t = s;
      while(*s && tagchar(*s))
        ++s;
      /* strip trailing spaces */
      while(s > t && s[-1] == ' ')
        --s;
      /* add tag to list */
      vector_append(&v, normalize_tag(t, (size_t)(s - t)));
      /* skip intermediate and trailing separators */
      while(*s && (!tagchar(*s) || *s == ' '))
        ++s;
    }
  }
  vector_terminate(&v);
  return dedupe(v.vec, v.nvec);
}

/* Record that TRACK has TAG.  Returns 0 or DB_LOCK_DEADLOCK. */
static int register_tag(const char *track, const char *tag, DB_TXN *tid) {
  return register_word(trackdb_tagsdb, "tags", track, tag, tid);
}

/* aliases *******************************************************************/

/* compute the alias and store at aliasp.  Returns 0 or DB_LOCK_DEADLOCK.  If
 * there is no alias sets *aliasp to 0. */
static int compute_alias(char **aliasp,
                         const char *track,
                         const struct kvp *p,
                         DB_TXN *tid) {
  struct dynstr d;
  const char *s = config->alias, *t, *expansion, *part;
  int c, used_db = 0, slash_prefix, err;
  struct kvp *at;
  const char *const root = find_track_root(track);

  if(!root) {
    /* Bodge for tracks with no root */
    *aliasp = 0;
    return 0;
  }
  dynstr_init(&d);
  dynstr_append_string(&d, root);
  while((c = (unsigned char)*s++)) {
    if(c != '{') {
      dynstr_append(&d, c);
      continue;
    }
    if((slash_prefix = (*s == '/')))
      s++;
    t = strchr(s, '}');
    assert(t != 0);			/* validated at startup */
    part = xstrndup(s, t - s);
    expansion = getpart(track, "display", part, p, &used_db);
    if(*expansion) {
      if(slash_prefix) dynstr_append(&d, '/');
      dynstr_append_string(&d, expansion);
    }
    s = t + 1;				/* skip {part} */
  }
  /* only admit to the alias if we used the db... */
  if(!used_db) {
    *aliasp = 0;
    return 0;
  }
  dynstr_terminate(&d);
  /* ...and the answer differs from the original... */
  if(!strcmp(track, d.vec)) {
    *aliasp = 0;
    return 0;
  }
  /* ...and there isn't already a different track with that name (including as
   * an alias) */
  switch(err = trackdb_getdata(trackdb_tracksdb, d.vec, &at, tid)) {
  case 0:
    if((s = kvp_get(at, "_alias_for"))
       && !strcmp(s, track)) {
    case DB_NOTFOUND:
      *aliasp = d.vec;
    } else {
      *aliasp = 0;
    }
    return 0;
  default:
    return err;
  }
}

/* get track and prefs data (if tp/pp not null pointers).  Returns 0 on
 * success, DB_NOTFOUND if the track does not exist or DB_LOCK_DEADLOCK.
 * Always sets the return values, even if only to null pointers. */
static int gettrackdata(const char *track,
                        struct kvp **tp,
                        struct kvp **pp,
                        const char **actualp,
                        unsigned flags,
#define GTD_NOALIAS 0x0001
                        DB_TXN *tid) {
  int err;
  const char *actual = track;
  struct kvp *t = 0, *p = 0;

  if((err = trackdb_getdata(trackdb_tracksdb, track, &t, tid))) goto done;
  if((actual = kvp_get(t, "_alias_for"))) {
    if(flags & GTD_NOALIAS) {
      error(0, "alias passed to gettrackdata where real path required");
      abort();
    }
    if((err = trackdb_getdata(trackdb_tracksdb, actual, &t, tid))) goto done;
  } else
    actual = track;
  assert(actual != 0);
  if(pp) {
    if((err = trackdb_getdata(trackdb_prefsdb, actual, &p, tid)) == DB_LOCK_DEADLOCK)
      goto done;
  }
  err = 0;
done:
  if(actualp) *actualp = actual;
  if(tp) *tp = t;
  if(pp) *pp = p;
  return err;
}

/* trackdb_notice() **********************************************************/

/** @brief notice a possibly new track
 * @return @c DB_NOTFOUND if new, 0 if already known
 */
int trackdb_notice(const char *track,
                   const char *path) {
  int err;
  DB_TXN *tid;

  for(;;) {
    tid = trackdb_begin_transaction();
    err = trackdb_notice_tid(track, path, tid);
    if((err == DB_LOCK_DEADLOCK)) goto fail;
    break;
  fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return err;
}

/** @brief notice a possibly new track
 * @param track NFC UTF-8 track name
 * @param path Raw path name
 * @param tid Transaction ID
 * @return @c DB_NOTFOUND if new, 0 if already known, @c DB_LOCK_DEADLOCK also
 */
int trackdb_notice_tid(const char *track,
                       const char *path,
                       DB_TXN *tid) {
  int err, n;
  struct kvp *t, *a, *p;
  int t_changed, ret;
  char *alias, **w, *noticed;
  time_t now;

  /* notice whether the tracks.db entry changes */
  t_changed = 0;
  /* get any existing tracks entry */
  if((err = gettrackdata(track, &t, &p, 0, 0, tid)) == DB_LOCK_DEADLOCK)
    return err;
  ret = err;                            /* 0 or DB_NOTFOUND */
  /* this is a real track */
  t_changed += kvp_set(&t, "_alias_for", 0);
  t_changed += kvp_set(&t, "_path", path);
  time(&now);
  if(ret == DB_NOTFOUND) {
    /* It's a new track; record the time */
    byte_xasprintf(&noticed, "%lld", (long long)now);
    t_changed += kvp_set(&t, "_noticed", noticed);
  }
  /* if we have an alias record it in the database */
  if((err = compute_alias(&alias, track, p, tid))) return err;
  if(alias) {
    /* won't overwrite someone else's alias as compute_alias() checks */
    D(("%s: alias %s", track, alias));
    a = 0;
    kvp_set(&a, "_alias_for", track);
    if((err = trackdb_putdata(trackdb_tracksdb, alias, a, tid, 0))) return err;
  }
  /* update search.db */
  w = track_to_words(track, p);
  for(n = 0; w[n]; ++n)
    if((err = register_search_word(track, w[n], tid)))
      return err;
  /* update tags.db */
  w = parsetags(kvp_get(p, "tags"));
  for(n = 0; w[n]; ++n)
    if((err = register_tag(track, w[n], tid)))
      return err;
  /* only store the tracks.db entry if it has changed */
  if(t_changed && (err = trackdb_putdata(trackdb_tracksdb, track, t, tid, 0)))
    return err;
  if(ret == DB_NOTFOUND) {
    uint32_t timestamp[2];
    DBT key, data;

    timestamp[0] = htonl((uint64_t)now >> 32);
    timestamp[1] = htonl((uint32_t)now);
    memset(&key, 0, sizeof key);
    key.data = timestamp;
    key.size = sizeof timestamp;
    switch(err = trackdb_noticeddb->put(trackdb_noticeddb, tid, &key,
                                        make_key(&data, track), 0)) {
    case 0: break;
    case DB_LOCK_DEADLOCK: return err;
    default: fatal(0, "error updating noticed.db: %s", db_strerror(err));
    }
  }
  return ret;
}

/* trackdb_obsolete() ********************************************************/

/* obsolete a track */
int trackdb_obsolete(const char *track, DB_TXN *tid) {
  int err, n;
  struct kvp *p;
  char *alias, **w;

  if((err = gettrackdata(track, 0, &p, 0,
                         GTD_NOALIAS, tid)) == DB_LOCK_DEADLOCK)
    return err;
  else if(err == DB_NOTFOUND) return 0;
  /* compute the alias, if any, and delete it */
  if((err = compute_alias(&alias, track, p, tid))) return err;
  if(alias) {
    /* if the alias points to some other track then compute_alias won't
     * return it */
    if((err = trackdb_delkey(trackdb_tracksdb, alias, tid))
       && err != DB_NOTFOUND)
      return err;
  }
  /* update search.db */
  w = track_to_words(track, p);
  for(n = 0; w[n]; ++n)
    if(trackdb_delkeydata(trackdb_searchdb,
                          w[n], track, tid) == DB_LOCK_DEADLOCK)
      return err;
  /* update tags.db */
  w = parsetags(kvp_get(p, "tags"));
  for(n = 0; w[n]; ++n)
    if(trackdb_delkeydata(trackdb_tagsdb,
                          w[n], track, tid) == DB_LOCK_DEADLOCK)
      return err;
  /* update tracks.db */
  if(trackdb_delkey(trackdb_tracksdb, track, tid) == DB_LOCK_DEADLOCK)
    return err;
  /* We don't delete the prefs, so they survive temporary outages of the
   * (possibly virtual) track filesystem */
  return 0;
}

/* trackdb_stats() ***********************************************************/

#define H(name) { #name, offsetof(DB_HASH_STAT, name) }
#define B(name) { #name, offsetof(DB_BTREE_STAT, name) }

static const struct statinfo {
  const char *name;
  size_t offset;
} statinfo_hash[] = {
  H(hash_magic),
  H(hash_version),
  H(hash_nkeys),
  H(hash_ndata),
  H(hash_pagesize),
  H(hash_ffactor),
  H(hash_buckets),
  H(hash_free),
  H(hash_bfree),
  H(hash_bigpages),
  H(hash_big_bfree),
  H(hash_overflows),
  H(hash_ovfl_free),
  H(hash_dup),
  H(hash_dup_free),
}, statinfo_btree[] = {
  B(bt_magic),
  B(bt_version),
  B(bt_nkeys),
  B(bt_ndata),
  B(bt_pagesize),
  B(bt_minkey),
  B(bt_re_len),
  B(bt_re_pad),
  B(bt_levels),
  B(bt_int_pg),
  B(bt_leaf_pg),
  B(bt_dup_pg),
  B(bt_over_pg),
  B(bt_free),
  B(bt_int_pgfree),
  B(bt_leaf_pgfree),
  B(bt_dup_pgfree),
  B(bt_over_pgfree),
};

/* look up stats for DB */
static int get_stats(struct vector *v,
                     DB *database,
                     const struct statinfo *si,
                     size_t nsi,
                     DB_TXN *tid) {
  void *sp;
  size_t n;
  char *str;
  int err;

  if(database) {
    switch(err = database->stat(database, tid, &sp, 0)) {
    case 0:
      break;
    case DB_LOCK_DEADLOCK:
      error(0, "error querying database: %s", db_strerror(err));
      return err;
    default:
      fatal(0, "error querying database: %s", db_strerror(err));
    }
    for(n = 0; n < nsi; ++n) {
      byte_xasprintf(&str, "%s=%"PRIuMAX, si[n].name,
		     (uintmax_t)*(u_int32_t *)((char *)sp + si[n].offset));
      vector_append(v, str);
    }
  }
  return 0;
}

/** @brief One entry in the search league */
struct search_entry {
  char *word;
  int n;
};

/** @brief Add a word to the search league
 * @param se Pointer to search league
 * @param count Maximum size for search league
 * @param nse Current size of search league
 * @param word New word, or NULL
 * @param n How often @p word appears
 * @return New size of search league
 */
static int register_search_entry(struct search_entry *se,
                                 int count,
                                 int nse,
                                 char *word,
                                 int n) {
  int i;

  if(word && (nse < count || n > se[nse - 1].n)) {
    /* Find the starting point */
    if(nse == count)
      i = nse - 1;
    else
      i = nse++;
    /* Find the insertion point */
    while(i > 0 && n > se[i - 1].n)
      --i;
    memmove(&se[i + 1], &se[i], (nse - i - 1) * sizeof *se);
    se[i].word = word;
    se[i].n = n;
  }
  return nse;
}

/* find the top COUNT words in the search database */
static int search_league(struct vector *v, int count, DB_TXN *tid) {
  struct search_entry *se;
  DBT k, d;
  DBC *cursor;
  int err, n = 0, nse = 0, i;
  char *word = 0;
  size_t wl = 0;
  char *str;

  cursor = trackdb_opencursor(trackdb_searchdb, tid);
  se = xmalloc(count * sizeof *se);
  /* Walk across the whole database counting up the number of times each
   * word appears. */
  while(!(err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                              DB_NEXT))) {
    if(word && wl == k.size && !strncmp(word, k.data, wl))
      ++n;                              /* same word again */
    else {
      nse = register_search_entry(se, count, nse, word, n);
      word = xstrndup(k.data, wl = k.size);
      n = 1;
    }
  }
  switch(err) {
  case DB_NOTFOUND:
    err = 0;
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error querying search database: %s", db_strerror(err));
    break;
  default:
    fatal(0, "error querying search database: %s", db_strerror(err));
  }
  if(trackdb_closecursor(cursor)) err = DB_LOCK_DEADLOCK;
  if(err) return err;
  nse = register_search_entry(se, count, nse, word, n);
  byte_xasprintf(&str, "Top %d search words:", nse);
  vector_append(v, str);
  for(i = 0; i < nse; ++i) {
    byte_xasprintf(&str, "%4d: %5d %s", i + 1, se[i].n, se[i].word);
    vector_append(v, str);
  }
  return 0;
}

#define SI(what) statinfo_##what, \
                 sizeof statinfo_##what / sizeof (struct statinfo)

/* return a list of database stats */
char **trackdb_stats(int *nstatsp) {
  DB_TXN *tid;
  struct vector v;

  vector_init(&v);
  for(;;) {
    tid = trackdb_begin_transaction();
    v.nvec = 0;
    vector_append(&v, (char *)"Tracks database stats:");
    if(get_stats(&v, trackdb_tracksdb, SI(btree), tid)) goto fail;
    vector_append(&v, (char *)"");
    vector_append(&v, (char *)"Search database stats:");
    if(get_stats(&v, trackdb_searchdb, SI(hash), tid)) goto fail;
    vector_append(&v, (char *)"");
    vector_append(&v, (char *)"Prefs database stats:");
    if(get_stats(&v, trackdb_prefsdb, SI(hash), tid)) goto fail;
    vector_append(&v, (char *)"");
    if(search_league(&v, 10, tid)) goto fail;
    vector_terminate(&v);
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  if(nstatsp) *nstatsp = v.nvec;
  return v.vec;
}

struct stats_details {
  void (*done)(char *data, void *u);
  void *u;
  int exited;                           /* subprocess exited */
  int closed;                           /* pipe close */
  int wstat;                            /* wait status from subprocess */
  struct dynstr data[1];                /* data read from pipe */
};

static void stats_complete(struct stats_details *d) {
  char *s;

  if(!(d->exited && d->closed))
    return;
  byte_xasprintf(&s, "\n"
                 "Server stats:\n"
                 "track lookup cache hits: %lu\n"
                 "track lookup cache misses: %lu\n",
                 cache_files_hits,
                 cache_files_misses);
  dynstr_append_string(d->data, s);
  dynstr_terminate(d->data);
  d->done(d->data->vec, d->u);
}

static int stats_finished(ev_source attribute((unused)) *ev,
                          pid_t attribute((unused)) pid,
                          int status,
                          const struct rusage attribute((unused)) *rusage,
                          void *u) {
  struct stats_details *const d = u;

  d->exited = 1;
  if(status)
    error(0, "disorder-stats %s", wstat(status));
  stats_complete(d);
  return 0;
}

static int stats_read(ev_source attribute((unused)) *ev,
                      ev_reader *reader,
                      void *ptr,
                      size_t bytes,
                      int eof,
                      void *u) {
  struct stats_details *const d = u;

  dynstr_append_bytes(d->data, ptr, bytes);
  ev_reader_consume(reader, bytes);
  if(eof)
    d->closed = 1;
  stats_complete(d);
  return 0;
}

static int stats_error(ev_source attribute((unused)) *ev,
                       int errno_value,
                       void *u) {
  struct stats_details *const d = u;

  error(errno_value, "error reading from pipe to disorder-stats");
  d->closed = 1;
  stats_complete(d);
  return 0;
}

void trackdb_stats_subprocess(ev_source *ev,
                              void (*done)(char *data, void *u),
                              void *u) {
  int p[2];
  pid_t pid;
  struct stats_details *d = xmalloc(sizeof *d);

  dynstr_init(d->data);
  d->done = done;
  d->u = u;
  xpipe(p);
  pid = subprogram(ev, p[1], "disorder-stats", (char *)0);
  xclose(p[1]);
  ev_child(ev, pid, 0, stats_finished, d);
  ev_reader_new(ev, p[0], stats_read, stats_error, d, "disorder-stats reader");
}

/** @brief Parse a track name part preference
 * @param name Preference name
 * @param partp Where to store part name
 * @param contextp Where to store context name
 * @return 0 on success, non-0 if parse fails
 */
static int trackdb__parse_namepref(const char *name,
                                   char **partp,
                                   char **contextp) {
  char *c;
  static const char prefix[] = "trackname_";
  
  if(strncmp(name, prefix, strlen(prefix)))
    return -1;                          /* not trackname_* at all */
  name += strlen(prefix);
  /* There had better be a _ between context and part */
  c = strchr(name, '_');
  if(!c)
    return -1;
  /* Context is first in the pref name even though most APIs have the part
   * first.  Confusing; sorry. */
  *contextp = xstrndup(name, c - name);
  ++c;
  /* There had better NOT be a second _ */
  if(strchr(c, '_'))
    return -1;
  *partp = xstrdup(c);
  return 0;
}

/** @brief Compute the default value for a track preference
 * @param track Track name
 * @param name Preference name
 * @return Default value or 0 if none/not known
 */
static const char *trackdb__default(const char *track, const char *name) {
  char *context, *part;
  
  if(!trackdb__parse_namepref(name, &part, &context)) {
    /* We can work out the default for a trackname_ pref */
    return trackname_part(track, context, part);
  } else if(!strcmp(name, "weight")) {
    /* We know the default weight */
    return "90000";
  } else if(!strcmp(name, "pick_at_random")) {
    /* By default everything is eligible for picking at random */
    return "1";
  } else if(!strcmp(name, "tags")) {
    /* By default everything no track has any tags */
    return "";
  }
  return 0;
}

/* set a pref (remove if value=0) */
int trackdb_set(const char *track,
                const char *name,
                const char *value) {
  struct kvp *t, *p, *a;
  DB_TXN *tid;
  int err, cmp;
  char *oldalias, *newalias, **oldtags = 0, **newtags;
  const char *def;

  /* If the value matches the default then unset instead, to keep the database
   * tidy.  Older versions did not have this feature so your database may yet
   * have some default values stored in it. */
  if(value) {
    def = trackdb__default(track, name);
    if(def && !strcmp(value, def))
      value = 0;
  }

  for(;;) {
    tid = trackdb_begin_transaction();
    if((err = gettrackdata(track, &t, &p, 0,
                           0, tid)) == DB_LOCK_DEADLOCK)
      goto fail;
    if(err == DB_NOTFOUND) break;
    if(name[0] == '_') {
      if(kvp_set(&t, name, value))
        if(trackdb_putdata(trackdb_tracksdb, track, t, tid, 0))
          goto fail;
    } else {
      /* get the old alias name */
      if(compute_alias(&oldalias, track, p, tid)) goto fail;
      /* get the old tags */
      if(!strcmp(name, "tags"))
        oldtags = parsetags(kvp_get(p, "tags"));
      /* set the value */
      if(kvp_set(&p, name, value))
        if(trackdb_putdata(trackdb_prefsdb, track, p, tid, 0))
          goto fail;
      /* compute the new alias name */
      if((err = compute_alias(&newalias, track, p, tid))) goto fail;
      /* check whether alias has changed */
      if(!(oldalias == newalias
           || (oldalias && newalias && !strcmp(oldalias, newalias)))) {
        /* adjust alias records to fit change */
        if(oldalias
           && trackdb_delkey(trackdb_tracksdb, oldalias, tid) == DB_LOCK_DEADLOCK)
          goto fail;
        if(newalias) {
          a = 0;
          kvp_set(&a, "_alias_for", track);
          if(trackdb_putdata(trackdb_tracksdb, newalias, a, tid, 0)) goto fail;
        }
      }
      /* check whether tags have changed */
      if(!strcmp(name, "tags")) {
        newtags = parsetags(value);
        while(*oldtags || *newtags) {
          if(*oldtags && *newtags) {
            cmp = strcmp(*oldtags, *newtags);
            if(!cmp) {
              /* keeping this tag */
              ++oldtags;
              ++newtags;
            } else if(cmp < 0)
              /* old tag fits into a gap in the new list, so delete old */
              goto delete_old;
            else
              /* new tag fits into a gap in the old list, so insert new */
              goto insert_new;
          } else if(*oldtags) {
            /* we've run out of new tags, so remaining old ones are to be
             * deleted */
          delete_old:
            if(trackdb_delkeydata(trackdb_tagsdb,
                                  *oldtags, track, tid) == DB_LOCK_DEADLOCK)
              goto fail;
            ++oldtags;
          } else {
            /* we've run out of old tags, so remainig new ones are to be
             * inserted */
          insert_new:
            if(register_tag(track, *newtags, tid)) goto fail;
            ++newtags;
          }
        }
      }
    }
    err = 0;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return err == 0 ? 0 : -1;
}

/* get a pref */
const char *trackdb_get(const char *track,
                        const char *name) {
  return kvp_get(trackdb_get_all(track), name);
}

/* get all prefs as a 0-terminated array */
struct kvp *trackdb_get_all(const char *track) {
  struct kvp *t, *p, **pp;
  DB_TXN *tid;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(gettrackdata(track, &t, &p, 0, 0, tid) == DB_LOCK_DEADLOCK)
      goto fail;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  for(pp = &p; *pp; pp = &(*pp)->next)
    ;
  *pp = t;
  return p;
}

/* resolve alias */
const char *trackdb_resolve(const char *track) {
  DB_TXN *tid;
  const char *actual;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(gettrackdata(track, 0, 0, &actual, 0, tid) == DB_LOCK_DEADLOCK)
      goto fail;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return actual;
}

int trackdb_isalias(const char *track) {
  const char *actual = trackdb_resolve(track);

  return strcmp(actual, track);
}

/* test whether a track exists (perhaps an alias) */
int trackdb_exists(const char *track) {
  DB_TXN *tid;
  int err;

  for(;;) {
    tid = trackdb_begin_transaction();
    /* unusually, here we want the return value */
    if((err = gettrackdata(track, 0, 0, 0, 0, tid)) == DB_LOCK_DEADLOCK)
      goto fail;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return (err == 0);
}

/* return the list of tags */
char **trackdb_alltags(void) {
  int e;
  struct vector v[1];

  vector_init(v);
  WITH_TRANSACTION(trackdb_listkeys(trackdb_tagsdb, v, tid));
  return v->vec;
}

/** @brief List all the keys in @p db
 * @param db Database
 * @param v Vector to store keys in
 * @param tid Transaction ID
 * @return 0 or DB_LOCK_DEADLOCK
 */
int trackdb_listkeys(DB *db, struct vector *v, DB_TXN *tid) {
  int e;
  DBT k, d;
  DBC *const c = trackdb_opencursor(db, tid);

  v->nvec = 0;
  memset(&k, 0, sizeof k);
  while(!(e = c->c_get(c, &k, prepare_data(&d), DB_NEXT_NODUP)))
    vector_append(v, xstrndup(k.data, k.size));
  switch(e) {
  case DB_NOTFOUND:
    break;
  case DB_LOCK_DEADLOCK:
    return e;
  default:
    fatal(0, "c->c_get: %s", db_strerror(e));
  }
  if((e = trackdb_closecursor(c)))
    return e;
  vector_terminate(v);
  return 0;
}

/* return 1 iff sorted tag lists A and B have at least one member in common */
int tag_intersection(char **a, char **b) {
  int cmp;

  /* Same sort of logic as trackdb_set() above */
  while(*a && *b) {
    if(!(cmp = strcmp(*a, *b))) return 1;
    else if(cmp < 0) ++a;
    else ++b;
  }
  return 0;
}

static pid_t choose_pid = -1;
static int choose_fd;
static random_callback *choose_callback;
static struct dynstr choose_output;
static unsigned choose_complete;
static int choose_status;
#define CHOOSE_RUNNING 1
#define CHOOSE_READING 2

static void choose_finished(ev_source *ev, unsigned which) {
  choose_complete |= which;
  if(choose_complete != (CHOOSE_RUNNING|CHOOSE_READING))
    return;
  choose_pid = -1;
  if(choose_status == 0 && choose_output.nvec > 0) {
    dynstr_terminate(&choose_output);
    choose_callback(ev, xstrdup(choose_output.vec));
  } else
    choose_callback(ev, 0);
}

/** @brief Called when @c disorder-choose terminates */
static int choose_exited(ev_source *ev,
                         pid_t attribute((unused)) pid,
                         int status,
                         const struct rusage attribute((unused)) *rusage,
                         void attribute((unused)) *u) {
  if(status)
    error(0, "disorder-choose %s", wstat(status));
  choose_status = status;
  choose_finished(ev, CHOOSE_RUNNING);
  return 0;
}

/** @brief Called with data from @c disorder-choose pipe */
static int choose_readable(ev_source *ev,
                           ev_reader *reader,
                           void *ptr,
                           size_t bytes,
                           int eof,
                           void attribute((unused)) *u) {
  dynstr_append_bytes(&choose_output, ptr, bytes);
  ev_reader_consume(reader, bytes);
  if(eof)
    choose_finished(ev, CHOOSE_READING);
  return 0;
}

static int choose_read_error(ev_source *ev,
                             int errno_value,
                             void attribute((unused)) *u) {
  error(errno_value, "error reading disorder-choose pipe");
  choose_finished(ev, CHOOSE_READING);
  return 0;
}

/** @brief Request a random track
 * @param ev Event source
 * @param callback Called with random track or NULL
 * @return 0 if a request was initiated, else -1
 *
 * Initiates a random track choice.  @p callback will later be called back with
 * the choice (or NULL on error).  If a choice is already underway then -1 is
 * returned and there will be no additional callback.
 *
 * The caller shouldn't assume that the track returned actually exists (it
 * might be removed between the choice and the callback, or between being added
 * to the queue and being played).
 */
int trackdb_request_random(ev_source *ev,
                           random_callback *callback) {
  int p[2];
  
  if(choose_pid != -1)
    return -1;                          /* don't run concurrent chooses */
  xpipe(p);
  cloexec(p[0]);
  choose_pid = subprogram(ev, p[1], "disorder-choose", (char *)0);
  choose_fd = p[0];
  xclose(p[1]);
  choose_callback = callback;
  choose_output.nvec = 0;
  choose_complete = 0;
  ev_reader_new(ev, p[0], choose_readable, choose_read_error, 0,
                "disorder-choose reader"); /* owns p[0] */
  ev_child(ev, choose_pid, 0, choose_exited, 0); /* owns the subprocess */
  return 0;
}

/* get a track name given the prefs.  Set *used_db to 1 if we got the answer
 * from the prefs. */
static const char *getpart(const char *track,
                           const char *context,
                           const char *part,
                           const struct kvp *p,
                           int *used_db) {
  const char *result;
  char *pref;

  byte_xasprintf(&pref, "trackname_%s_%s", context, part);
  if((result = kvp_get(p, pref)))
    *used_db = 1;
  else
    result = trackname_part(track, context, part);
  assert(result != 0);
  return result;
}

/* get a track name part, like trackname_part(), but taking the database into
 * account. */
const char *trackdb_getpart(const char *track,
                            const char *context,
                            const char *part) {
  struct kvp *p;
  DB_TXN *tid;
  char *pref;
  const char *actual;
  int used_db, err;

  /* construct the full pref */
  byte_xasprintf(&pref, "trackname_%s_%s", context, part);
  for(;;) {
    tid = trackdb_begin_transaction();
    if((err = gettrackdata(track, 0, &p, &actual, 0, tid)) == DB_LOCK_DEADLOCK)
      goto fail;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return getpart(actual, context, part, p, &used_db);
}

/* get the raw path name for @track@ (might be an alias) */
const char *trackdb_rawpath(const char *track) {
  DB_TXN *tid;
  struct kvp *t;
  const char *path;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(gettrackdata(track, &t, 0, 0, 0, tid) == DB_LOCK_DEADLOCK)
      goto fail;
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  if(!(path = kvp_get(t, "_path"))) path = track;
  return path;
}

/* trackdb_list **************************************************************/

/* this is incredibly ugly, sorry, perhaps it will be rewritten to be actually
 * readable at some point */

/* return true if the basename of TRACK[0..TL-1], as defined by DL, matches RE.
 * If RE is a null pointer then it matches everything. */
static int track_matches(size_t dl, const char *track, size_t tl,
			 const pcre *re) {
  int ovec[3], rc;

  if(!re)
    return 1;
  track += dl + 1;
  tl -= (dl + 1);
  switch(rc = pcre_exec(re, 0, track, tl, 0, 0, ovec, 3)) {
  case PCRE_ERROR_NOMATCH: return 0;
  default:
    if(rc < 0) {
      error(0, "pcre_exec returned %d, subject '%s'", rc, track);
      return 0;
    }
    return 1;
  }
}

static int do_list(struct vector *v, const char *dir,
                   enum trackdb_listable what, const pcre *re, DB_TXN *tid) {
  DBC *cursor;
  DBT k, d;
  size_t dl;
  char *ptr;
  int err;
  size_t l, last_dir_len = 0;
  char *last_dir = 0, *track;
  struct kvp *p;

  dl = strlen(dir);
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  make_key(&k, dir);
  prepare_data(&d);
  /* find the first key >= dir */
  err = cursor->c_get(cursor, &k, &d, DB_SET_RANGE);
  /* keep going while we're dealing with <dir/anything> */
  while(err == 0
	&& k.size > dl
	&& ((char *)k.data)[dl] == '/'
	&& !memcmp(k.data, dir, dl)) {
    ptr = memchr((char *)k.data + dl + 1, '/', k.size - (dl + 1));
    if(ptr) {
      /* we have <dir/component/anything>, so <dir/component> is a directory */
      l = ptr - (char *)k.data;
      if(what & trackdb_directories)
	if(!(last_dir
	     && l == last_dir_len
	     && !memcmp(last_dir, k.data, l))) {
	  last_dir = xstrndup(k.data, last_dir_len = l);
	  if(track_matches(dl, k.data, l, re))
	    vector_append(v, last_dir);
	}
    } else {
      /* found a plain file */
      if((what & trackdb_files)) {
	track = xstrndup(k.data, k.size);
        if((err = trackdb_getdata(trackdb_prefsdb,
                                  track, &p, tid)) == DB_LOCK_DEADLOCK)
          goto deadlocked;
        /* There's an awkward question here...
         *
         * If a track shares a directory with its alias then we could
         * do one of three things:
         * - report both.  Looks ridiculuous in most UIs.
         * - report just the alias.  Remarkably inconvenient to write
         *   UI code for!
         * - report just the real name.  Ugly if the UI doesn't prettify
         *   names via the name parts.
         */
#if 1
        /* If this file is an alias for a track in the same directory then we
         * skip it */
        struct kvp *t = kvp_urldecode(d.data, d.size);
        const char *alias_target = kvp_get(t, "_alias_for");
        if(!(alias_target
             && !strcmp(d_dirname(alias_target),
                        d_dirname(track))))
	  if(track_matches(dl, k.data, k.size, re))
	    vector_append(v, track);
#else
	/* if this file has an alias in the same directory then we skip it */
           char *alias;
        if((err = compute_alias(&alias, track, p, tid)))
          goto deadlocked;
        if(!(alias && !strcmp(d_dirname(alias), d_dirname(track))))
	  if(track_matches(dl, k.data, k.size, re))
	    vector_append(v, track);
#endif
      }
    }
    err = cursor->c_get(cursor, &k, &d, DB_NEXT);
  }
  switch(err) {
  case 0:
    break;
  case DB_NOTFOUND:
    err = 0;
    break;
  case DB_LOCK_DEADLOCK:
    error(0, "error querying database: %s", db_strerror(err));
    break;
  default:
    fatal(0, "error querying database: %s", db_strerror(err));
  }
deadlocked:
  if(trackdb_closecursor(cursor)) err = DB_LOCK_DEADLOCK;
  return err;
}

/* return the directories or files below @dir@ */
char **trackdb_list(const char *dir, int *np, enum trackdb_listable what,
                    const pcre *re) {
  DB_TXN *tid;
  int n;
  struct vector v;

  vector_init(&v);
  for(;;) {
    tid = trackdb_begin_transaction();
    v.nvec = 0;
    if(dir) {
      if(do_list(&v, dir, what, re, tid))
        goto fail;
    } else {
      for(n = 0; n < config->collection.n; ++n)
        if(do_list(&v, config->collection.s[n].root, what, re, tid))
          goto fail;
    }
    break;
fail:
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  vector_terminate(&v);
  if(np)
    *np = v.nvec;
  return v.vec;
}

/* If S is tag:something, return something.  Else return 0. */
static const char *checktag(const char *s) {
  if(!strncmp(s, "tag:", 4))
    return s + 4;
  else
    return 0;
}

/* return a list of tracks containing all of the words given.  If you
 * ask for only stopwords you get no tracks. */
char **trackdb_search(char **wordlist, int nwordlist, int *ntracks) {
  const char **w, *best = 0, *tag;
  char **twords, **tags;
  char *istag;
  int i, j, n, err, what;
  DBC *cursor = 0;
  DBT k, d;
  struct vector u, v;
  DB_TXN *tid;
  struct kvp *p;
  int ntags = 0;
  DB *db;
  const char *dbname;

  *ntracks = 0;				/* for early returns */
  /* normalize all the words */
  w = xmalloc(nwordlist * sizeof (char *));
  istag = xmalloc_noptr(nwordlist);
  for(n = 0; n < nwordlist; ++n) {
    uint32_t *w32;
    size_t nw32;

    w[n] = utf8_casefold_compat(wordlist[n], strlen(wordlist[n]), 0);
    if(checktag(w[n])) {
      ++ntags;         /* count up tags */
      /* Normalize the tag */
      w[n] = normalize_tag(w[n] + 4, strlen(w[n] + 4));
      istag[n] = 1;
    } else {
      /* Normalize the search term by removing combining characters */
      if(!(w32 = utf8_to_utf32(w[n], strlen(w[n]), &nw32)))
        return 0;
      nw32 = remove_combining_chars(w32, nw32);
      if(!(w[n] = utf32_to_utf8(w32, nw32, 0)))
        return 0;
      istag[n] = 0;
    }
  }
  /* find the longest non-stopword */
  for(n = 0; n < nwordlist; ++n)
    if(!istag[n] && !stopword(w[n]))
      if(!best || strlen(w[n]) > strlen(best))
	best = w[n];
  /* TODO: we should at least in principal be able to identify the word or tag
   * with the least matches in log time, and choose that as our primary search
   * term. */
  if(ntags && !best) {
    /* Only tags are listed.  We limit to the first and narrow down with the
     * rest. */
    best = istag[0] ? w[0] : 0;
    db = trackdb_tagsdb;
    dbname = "tags";
  } else if(best) {
    /* We can limit to some word. */
    db = trackdb_searchdb;
    dbname = "search";
  } else {
    /* Only stopwords */
    return 0;
  }
  vector_init(&u);
  vector_init(&v);
  for(;;) {
    tid = trackdb_begin_transaction();
    /* find all the tracks that have that word */
    make_key(&k, best);
    prepare_data(&d);
    what = DB_SET;
    v.nvec = 0;
    cursor = trackdb_opencursor(db, tid);
    while(!(err = cursor->c_get(cursor, &k, &d, what))) {
      vector_append(&v, xstrndup(d.data, d.size));
      what = DB_NEXT_DUP;
    }
    switch(err) {
    case DB_NOTFOUND:
      err = 0;
      break;
    case DB_LOCK_DEADLOCK:
      error(0, "error querying %s database: %s", dbname, db_strerror(err));
      break;
    default:
      fatal(0, "error querying %s database: %s", dbname, db_strerror(err));
    }
    if(trackdb_closecursor(cursor)) err = DB_LOCK_DEADLOCK;
    cursor = 0;
    /* do a naive search over that (hopefuly fairly small) list of tracks */
    u.nvec = 0;
    for(n = 0; n < v.nvec; ++n) {
      if((err = gettrackdata(v.vec[n], 0, &p, 0, 0, tid) == DB_LOCK_DEADLOCK))
        goto fail;
      else if(err) {
        error(0, "track %s unexpected error: %s", v.vec[n], db_strerror(err));
        continue;
      }
      twords = track_to_words(v.vec[n], p);
      tags = parsetags(kvp_get(p, "tags"));
      for(i = 0; i < nwordlist; ++i) {
        if(istag[i]) {
          tag = w[i];
          /* Track must have this tag */
          for(j = 0; tags[j]; ++j)
            if(!strcmp(tag, tags[j])) break; /* tag found */
          if(!tags[j]) break;           /* tag not found */
        } else {
          /* Track must contain this word */
          for(j = 0; twords[j]; ++j)
            if(!strcmp(w[i], twords[j])) break; /* word found */
          if(!twords[j]) break;		/* word not found */
        }
      }
      if(i >= nwordlist)                /* all words found */
        vector_append(&u, v.vec[n]);
    }
    break;
  fail:
    trackdb_closecursor(cursor);
    cursor = 0;
    trackdb_abort_transaction(tid);
    info("retrying search");
  }
  trackdb_commit_transaction(tid);
  vector_terminate(&u);
  if(ntracks)
    *ntracks = u.nvec;
  return u.vec;
}

/* trackdb_scan **************************************************************/

int trackdb_scan(const char *root,
                 int (*callback)(const char *track,
                                 struct kvp *data,
                                 struct kvp *prefs,
                                 void *u,
                                 DB_TXN *tid),
                 void *u,
                 DB_TXN *tid) {
  DBC *cursor;
  DBT k, d, pd;
  const size_t root_len = root ? strlen(root) : 0;
  int err, cberr;
  struct kvp *data, *prefs;
  const char *track;

  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if(root)
    err = cursor->c_get(cursor, make_key(&k, root), prepare_data(&d),
                        DB_SET_RANGE);
  else {
    memset(&k, 0, sizeof k);
    err = cursor->c_get(cursor, &k, prepare_data(&d),
                        DB_FIRST);
  }
  while(!err) {
    if(!root
       || (k.size > root_len
           && !strncmp(k.data, root, root_len)
           && ((char *)k.data)[root_len] == '/')) {
      data = kvp_urldecode(d.data, d.size);
      if(kvp_get(data, "_path")) {
        track = xstrndup(k.data, k.size);
        /* TODO: trackdb_prefsdb is currently a DB_HASH.  This means we have to
         * do a lookup for every single track.  In fact this is quite quick:
         * with around 10,000 tracks a complete scan is around 0.3s on my
         * 2.2GHz Athlon.  However, if it were a DB_BTREE, we could do the same
         * linear walk as we already do over trackdb_tracksdb, and probably get
         * even higher performance.  That would require upgrade logic to
         * translate old databases though.
         */
        switch(err = trackdb_prefsdb->get(trackdb_prefsdb, tid, &k,
                                          prepare_data(&pd), 0)) {
        case 0:
          prefs = kvp_urldecode(pd.data, pd.size);
          break;
        case DB_NOTFOUND:
          prefs = 0;
          break;
        case DB_LOCK_DEADLOCK:
          error(0, "getting prefs: %s", db_strerror(err));
          trackdb_closecursor(cursor);
          return err;
        default:
          fatal(0, "getting prefs: %s", db_strerror(err));
        }
        /* Advance to the next track before the callback so that the callback
         * may safely delete the track */
        err = cursor->c_get(cursor, &k, &d, DB_NEXT);
        if((cberr = callback(track, data, prefs, u, tid))) {
          err = cberr;
          break;
        }
      } else
        err = cursor->c_get(cursor, &k, &d, DB_NEXT);
    } else
      break;
  }
  trackdb_closecursor(cursor);
  switch(err) {
  case EINTR:
    return err;
  case 0:
  case DB_NOTFOUND:
    return 0;
  case DB_LOCK_DEADLOCK:
    error(0, "c->c_get: %s", db_strerror(err));
    return err;
  default:
    fatal(0, "c->c_get: %s", db_strerror(err));
  }
}

/* trackdb_rescan ************************************************************/

/** @brief Node in the list of rescan-complete callbacks */
struct rescanned_node {
  struct rescanned_node *next;
  void (*rescanned)(void *ru);
  void *ru;
};

/** @brief List of rescan-complete callbacks */
static struct rescanned_node *rescanned_list;

/** @brief Add a rescan completion callback */
void trackdb_add_rescanned(void (*rescanned)(void *ru),
                           void *ru) {
  if(rescanned) {
    struct rescanned_node *n = xmalloc(sizeof *n);
    n->next = rescanned_list;
    n->rescanned = rescanned;
    n->ru = ru;
    rescanned_list = n;
  }
}

/* called when the rescanner terminates */
static int reap_rescan(ev_source attribute((unused)) *ev,
                       pid_t pid,
                       int status,
                       const struct rusage attribute((unused)) *rusage,
                       void attribute((unused)) *u) {
  if(pid == rescan_pid) rescan_pid = -1;
  if(status)
    error(0, RESCAN": %s", wstat(status));
  else
    D((RESCAN" terminated: %s", wstat(status)));
  /* Our cache of file lookups is out of date now */
  cache_clean(&cache_files_type);
  eventlog("rescanned", (char *)0);
  /* Call rescanned callbacks */
  while(rescanned_list) {
    void (*rescanned)(void *u_) = rescanned_list->rescanned;
    void *ru = rescanned_list->ru;

    rescanned_list = rescanned_list->next;
    rescanned(ru);
  }
  return 0;
}

/** @brief Initiate a rescan
 * @param ev Event loop or 0 to block
 * @param recheck 1 to recheck lengths, 0 to suppress check
 * @param rescanned Called on completion (if not NULL)
 * @param u Passed to @p rescanned
 */
void trackdb_rescan(ev_source *ev, int recheck,
                    void (*rescanned)(void *ru),
                    void *ru) {
  int w;

  if(rescan_pid != -1) {
    trackdb_add_rescanned(rescanned, ru);
    error(0, "rescan already underway");
    return;
  }
  rescan_pid = subprogram(ev, -1, RESCAN,
                          recheck ? "--check" : "--no-check",
                          (char *)0);
  trackdb_add_rescanned(rescanned, ru);
  if(ev) {
    ev_child(ev, rescan_pid, 0, reap_rescan, 0);
    D(("started rescanner"));
  } else {
    /* This is the first rescan, we block until it is complete */
    while(waitpid(rescan_pid, &w, 0) < 0 && errno == EINTR)
      ;
    reap_rescan(0, rescan_pid, w, 0, 0);
  }
}

int trackdb_rescan_cancel(void) {
  if(rescan_pid == -1) return 0;
  if(kill(rescan_pid, SIGTERM) < 0)
    fatal(errno, "error killing rescanner");
  rescan_pid = -1;
  return 1;
}

/** @brief Return true if a rescan is underway */
int trackdb_rescan_underway(void) {
  return rescan_pid != -1;
}

/* global prefs **************************************************************/

void trackdb_set_global(const char *name,
                        const char *value,
                        const char *who) {
  DB_TXN *tid;
  int err;
  int state;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(!(err = trackdb_set_global_tid(name, value, tid)))
      break;
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  /* log important state changes */
  if(!strcmp(name, "playing")) {
    state = !value || !strcmp(value, "yes");
    info("playing %s by %s",
         state ? "enabled" : "disabled",
         who ? who : "-");
    eventlog("state", state ? "enable_play" : "disable_play", (char *)0);
  }
  if(!strcmp(name, "random-play")) {
    state = !value || !strcmp(value, "yes");
    info("random play %s by %s",
         state ? "enabled" : "disabled",
         who ? who : "-");
    eventlog("state", state ? "enable_random" : "disable_random", (char *)0);
  }
}

int trackdb_set_global_tid(const char *name,
                           const char *value,
                           DB_TXN *tid) {
  DBT k, d;
  int err;

  memset(&k, 0, sizeof k);
  memset(&d, 0, sizeof d);
  k.data = (void *)name;
  k.size = strlen(name);
  if(value) {
    d.data = (void *)value;
    d.size = strlen(value);
  }
  if(value)
    err = trackdb_globaldb->put(trackdb_globaldb, tid, &k, &d, 0);
  else
    err = trackdb_globaldb->del(trackdb_globaldb, tid, &k, 0);
  if(err == DB_LOCK_DEADLOCK) return err;
  if(err)
    fatal(0, "error updating database: %s", db_strerror(err));
  return 0;
}

const char *trackdb_get_global(const char *name) {
  DB_TXN *tid;
  int err;
  const char *r;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(!(err = trackdb_get_global_tid(name, tid, &r)))
      break;
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return r;
}

int trackdb_get_global_tid(const char *name,
                           DB_TXN *tid,
                           const char **rp) {
  DBT k, d;
  int err;

  memset(&k, 0, sizeof k);
  k.data = (void *)name;
  k.size = strlen(name);
  switch(err = trackdb_globaldb->get(trackdb_globaldb, tid, &k,
                                     prepare_data(&d), 0)) {
  case 0:
    *rp = xstrndup(d.data, d.size);
    return 0;
  case DB_NOTFOUND:
    *rp = 0;
    return 0;
  case DB_LOCK_DEADLOCK:
    return err;
  default:
    fatal(0, "error reading database: %s", db_strerror(err));
  }
}

/** @brief Retrieve the most recently added tracks
 * @param ntracksp Where to put count, or 0
 * @param maxtracks Maximum number of tracks to retrieve
 * @return null-terminated array of track names
 *
 * The most recently added track is first in the array.
 */
char **trackdb_new(int *ntracksp,
                   int maxtracks) {
  DB_TXN *tid;
  char **tracks;

  for(;;) {
    tid = trackdb_begin_transaction();
    tracks = trackdb_new_tid(ntracksp, maxtracks, tid);
    if(tracks)
      break;
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  return tracks;
}

/** @brief Retrieve the most recently added tracks
 * @param ntracksp Where to put count, or 0
 * @param maxtracks Maximum number of tracks to retrieve, or 0 for all
 * @param tid Transaction ID
 * @return null-terminated array of track names, or NULL on deadlock
 *
 * The most recently added track is first in the array.
 */
static char **trackdb_new_tid(int *ntracksp,
                              int maxtracks,
                              DB_TXN *tid) {
  DBC *c;
  DBT k, d;
  int err = 0;
  struct vector tracks[1];
  hash *h = hash_new(1);

  vector_init(tracks);
  c = trackdb_opencursor(trackdb_noticeddb, tid);
  while((maxtracks <= 0 || tracks->nvec < maxtracks)
        && !(err = c->c_get(c, prepare_data(&k), prepare_data(&d), DB_PREV))) {
    char *const track = xstrndup(d.data, d.size);
    /* Don't add any track more than once */
    if(hash_add(h, track, "", HASH_INSERT))
      continue;
    /* See if the track still exists */
    err = trackdb_getdata(trackdb_tracksdb, track, NULL/*kp*/, tid);
    if(err == DB_NOTFOUND)
      continue;                         /* It doesn't, skip it */
    if(err == DB_LOCK_DEADLOCK)
      break;                            /* Doh */
    vector_append(tracks, track);
  }
  switch(err) {
  case 0:                               /* hit maxtracks */
  case DB_NOTFOUND:                     /* ran out of tracks */
    break;
  case DB_LOCK_DEADLOCK:
    trackdb_closecursor(c);
    return 0;
  default:
    fatal(0, "error reading noticed.db: %s", db_strerror(err));
  }
  if((err = trackdb_closecursor(c)))
    return 0;                           /* deadlock */
  vector_terminate(tracks);
  if(ntracksp)
    *ntracksp = tracks->nvec;
  return tracks->vec;
}

/** @brief Expire noticed.db
 * @param earliest Earliest timestamp to keep
 */
void trackdb_expire_noticed(time_t earliest) {
  DB_TXN *tid;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(!trackdb_expire_noticed_tid(earliest, tid))
      break;
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
}

/** @brief Expire noticed.db
 * @param earliest Earliest timestamp to keep
 * @param tid Transaction ID
 * @return 0 or DB_LOCK_DEADLOCK
 */
static int trackdb_expire_noticed_tid(time_t earliest, DB_TXN *tid) {
  DBC *c;
  DBT k, d;
  int err = 0, ret;
  time_t when;
  uint32_t *kk;
  int count = 0;

  c = trackdb_opencursor(trackdb_noticeddb, tid);
  while(!(err = c->c_get(c, prepare_data(&k), prepare_data(&d), DB_NEXT))) {
    kk = k.data;
    when = (time_t)(((uint64_t)ntohl(kk[0]) << 32) + ntohl(kk[1]));
    if(when >= earliest)
      break;
    if((err = c->c_del(c, 0))) {
      if(err != DB_LOCK_DEADLOCK)
        fatal(0, "error deleting expired noticed.db entry: %s",
              db_strerror(err));
      break;
    }
    ++count;
  }
  if(err == DB_NOTFOUND)
    err = 0;
  if(err && err != DB_LOCK_DEADLOCK)
    fatal(0, "error expiring noticed.db: %s", db_strerror(err));
  ret = err;
  if((err = trackdb_closecursor(c))) {
    if(err != DB_LOCK_DEADLOCK)
      fatal(0, "error closing cursor: %s", db_strerror(err));
    ret = err;
  }
  if(!ret && count)
    info("expired %d tracks from noticed.db", count);
  return ret;
}

/* tidying up ****************************************************************/

void trackdb_gc(void) {
  int err;
  char **logfiles;

  if((err = trackdb_env->txn_checkpoint(trackdb_env,
                                        config->checkpoint_kbyte,
                                        config->checkpoint_min,
                                        0)))
    fatal(0, "trackdb_env->txn_checkpoint: %s", db_strerror(err));
  if((err = trackdb_env->log_archive(trackdb_env, &logfiles, DB_ARCH_REMOVE)))
    fatal(0, "trackdb_env->log_archive: %s", db_strerror(err));
  /* This makes catastrophic recovery impossible.  However, the user can still
   * preserve the important data by using disorder-dump to snapshot their
   * prefs, and later to restore it.  This is likely to have much small
   * long-term storage requirements than record the db logfiles. */
}

/* user database *************************************************************/

/** @brief Return true if @p user is trusted */
static int trusted(const char *user) {
  int n;

  for(n = 0; (n < config->trust.n
	      && strcmp(config->trust.s[n], user)); ++n)
    ;
  return n < config->trust.n;
}

/** @brief Return non-zero for a valid username
 *
 * Currently we only allow the letters and digits in ASCII.  We could be more
 * liberal than this but it is a nice simple test.  It is critical that
 * semicolons are never allowed.
 *
 * NB also used by playlist_parse_name() to validate playlist names!
 */
int valid_username(const char *user) {
  if(!*user)
    return 0;
  while(*user) {
    const uint8_t c = *user++;
    /* For now we are very strict */
    if((c >= 'a' && c <= 'z')
       || (c >= 'A' && c <= 'Z')
       || (c >= '0' && c <= '9'))
      /* ok */;
    else
      return 0;
  }
  return 1;
}

/** @brief Add a user */
static int create_user(const char *user,
                       const char *password,
                       const char *rights,
                       const char *email,
                       const char *confirmation,
                       DB_TXN *tid,
                       uint32_t flags) {
  struct kvp *k = 0;
  char s[64];

  /* sanity check user */
  if(!valid_username(user)) {
    error(0, "invalid username '%s'", user);
    return -1;
  }
  if(parse_rights(rights, 0, 1)) {
    error(0, "invalid rights string");
    return -1;
  }
  /* data for this user */
  if(password)
    kvp_set(&k, "password", password);
  kvp_set(&k, "rights", rights);
  if(email)
    kvp_set(&k, "email", email);
  if(confirmation)
    kvp_set(&k, "confirmation", confirmation);
  snprintf(s, sizeof s, "%jd", (intmax_t)time(0));
  kvp_set(&k, "created", s);
  return trackdb_putdata(trackdb_usersdb, user, k, tid, flags);
}

/** @brief Add one pre-existing user */
static int one_old_user(const char *user, const char *password,
                        DB_TXN *tid) {
  const char *rights;

  /* www-data doesn't get added */
  if(!strcmp(user, "www-data")) {
    info("not adding www-data to user database");
    return 0;
  }
  /* pick rights */
  if(!strcmp(user, "root"))
    rights = "all";
  else if(trusted(user)) {
    rights_type r;

    parse_rights(config->default_rights, &r, 1);
    r &= ~(rights_type)(RIGHT_SCRATCH__MASK|RIGHT_MOVE__MASK|RIGHT_REMOVE__MASK);
    r |= (RIGHT_ADMIN|RIGHT_RESCAN
          |RIGHT_SCRATCH_ANY|RIGHT_MOVE_ANY|RIGHT_REMOVE_ANY);
    rights = rights_string(r);
  } else
    rights = config->default_rights;
  return create_user(user, password, rights, 0/*email*/, 0/*confirmation*/,
                     tid, DB_NOOVERWRITE);
}

static int trackdb_old_users_tid(DB_TXN *tid) {
  int n;

  for(n = 0; n < config->allow.n; ++n) {
    switch(one_old_user(config->allow.s[n].s[0], config->allow.s[n].s[1],
                        tid)) {
    case 0:
      info("created user %s from 'allow' directive", config->allow.s[n].s[0]);
      break;
    case DB_KEYEXIST:
      error(0, "user %s already exists, delete 'allow' directive",
            config->allow.s[n].s[0]);
          /* This won't ever become fatal - eventually 'allow' will be
           * disabled. */
      break;
    case DB_LOCK_DEADLOCK:
      return DB_LOCK_DEADLOCK;
    }
  }
  return 0;
}

/** @brief Read old 'allow' directives and copy them to the users database */
void trackdb_old_users(void) {
  int e;

  if(config->allow.n)
    WITH_TRANSACTION(trackdb_old_users_tid(tid));
}

/** @brief Create a root user in the user database if there is none */
void trackdb_create_root(void) {
  int e;
  uint8_t pwbin[12];
  char *pw;

  /* Choose a new root password */
  gcry_randomize(pwbin, sizeof pwbin, GCRY_STRONG_RANDOM);
  pw = mime_to_base64(pwbin, sizeof pwbin);
  /* Create the root user if it does not exist */
  WITH_TRANSACTION(create_user("root", pw, "all",
                               0/*email*/, 0/*confirmation*/,
                               tid, DB_NOOVERWRITE));
  if(e == 0)
    info("created root user");
}

/** @brief Find a user's password from the database
 * @param user Username
 * @return Password or NULL
 *
 * Only works if running as a user that can read the database!
 *
 * If the user exists but has no password, "" is returned.
 */
const char *trackdb_get_password(const char *user) {
  int e;
  struct kvp *k;
  const char *password;

  WITH_TRANSACTION(trackdb_getdata(trackdb_usersdb, user, &k, tid));
  if(e)
    return 0;
  password = kvp_get(k, "password");
  return password ? password : "";
}

/** @brief Add a new user
 * @param user Username
 * @param password Password or NULL
 * @param rights Initial rights
 * @param email Email address or NULL
 * @param confirmation Confirmation string or NULL
 * @return 0 on success, non-0 on error
 */
int trackdb_adduser(const char *user,
                    const char *password,
                    const char *rights,
                    const char *email,
                    const char *confirmation) {
  int e;

  WITH_TRANSACTION(create_user(user, password, rights, email, confirmation,
                               tid, DB_NOOVERWRITE));
  if(e) {
    error(0, "cannot create user '%s' because they already exist", user);
    return -1;
  } else {
    if(email)
      info("created user '%s' with rights '%s' and email address '%s'",
           user, rights, email);
    else
      info("created user '%s' with rights '%s'", user, rights);
    eventlog("user_add", user, (char *)0);
    return 0;
  }
}

/** @brief Delete a user
 * @param user User to delete
 * @return 0 on success, non-0 if the user didn't exist anyway
 */
int trackdb_deluser(const char *user) {
  int e;

  WITH_TRANSACTION(trackdb_delkey(trackdb_usersdb, user, tid));
  if(e) {
    error(0, "cannot delete user '%s' because they do not exist", user);
    return -1;
  }
  info("deleted user '%s'", user);
  eventlog("user_delete", user, (char *)0);
  return 0;
}

/** @brief Get user information
 * @param user User to query
 * @return Linked list of user information or NULL if user does not exist
 *
 * Every user has at least a @c rights entry so NULL can be used to mean no
 * such user safely.
 */
struct kvp *trackdb_getuserinfo(const char *user) {
  int e;
  struct kvp *k;

  WITH_TRANSACTION(trackdb_getdata(trackdb_usersdb, user, &k, tid));
  if(e)
    return 0;
  else
    return k;
}

/** @brief Edit user information
 * @param user User to edit
 * @param key Key to change
 * @param value Value to set, or NULL to remove
 * @param tid Transaction ID
 * @return 0, DB_LOCK_DEADLOCK or DB_NOTFOUND
 */
static int trackdb_edituserinfo_tid(const char *user, const char *key,
                                    const char *value, DB_TXN *tid) {
  struct kvp *k;
  int e;

  if((e = trackdb_getdata(trackdb_usersdb, user, &k, tid)))
    return e;
  if(!kvp_set(&k, key, value))
    return 0;                           /* no change */
  return trackdb_putdata(trackdb_usersdb, user, k, tid, 0);
}

/** @brief Edit user information
 * @param user User to edit
 * @param key Key to change
 * @param value Value to set, or NULL to remove
 * @return 0 on success, non-0 on error
 */
int trackdb_edituserinfo(const char *user,
                         const char *key, const char *value) {
  int e;

  if(!strcmp(key, "rights")) {
    if(!value) {
      error(0, "cannot remove 'rights' key from user '%s'", user);
      return -1;
    }
    if(parse_rights(value, 0, 1)) {
      error(0, "invalid rights string");
      return -1;
    }
  } else if(!strcmp(key, "email")) {
    if(*value) {
      if(!email_valid(value)) {
        error(0, "invalid email address '%s' for user '%s'", value, user);
        return -1;
      }
    } else
      value = 0;                        /* no email -> remove key */
  } else if(!strcmp(key, "created")) {
    error(0, "cannot change creation date for user '%s'", user);
    return -1;
  } else if(strcmp(key, "password")
            && !strcmp(key, "confirmation")) {
    error(0, "unknown user info key '%s' for user '%s'", key, user);
    return -1;
  }
  WITH_TRANSACTION(trackdb_edituserinfo_tid(user, key, value, tid));
  if(e) {
    error(0, "unknown user '%s'", user);
    return -1;
  } else {
    eventlog("user_edit", user, key, (char *)0);
    return 0;
  }
}

/** @brief List all users
 * @return NULL-terminated list of users
 */
char **trackdb_listusers(void) {
  int e;
  struct vector v[1];

  vector_init(v);
  WITH_TRANSACTION(trackdb_listkeys(trackdb_usersdb, v, tid));
  return v->vec;
}

/** @brief Confirm a user registration
 * @param user Username
 * @param confirmation Confirmation string
 * @param rightsp Where to put user rights
 * @param tid Transaction ID
 * @return 0 on success, non-0 on error
 */
static int trackdb_confirm_tid(const char *user, const char *confirmation,
                               rights_type *rightsp,
                               DB_TXN *tid) {
  const char *stored_confirmation;
  struct kvp *k;
  int e;
  const char *rights;
  
  if((e = trackdb_getdata(trackdb_usersdb, user, &k, tid)))
    return e;
  if(!(stored_confirmation = kvp_get(k, "confirmation"))) {
    error(0, "already confirmed user '%s'", user);
    /* DB claims -30,800 to -30,999 so -1 should be a safe bet */
    return -1;
  }
  if(!(rights = kvp_get(k, "rights"))) {
    error(0, "no rights for unconfirmed user '%s'", user);
    return -1;
  }
  if(parse_rights(rights, rightsp, 1))
    return -1;
  if(strcmp(confirmation, stored_confirmation)) {
    error(0, "wrong confirmation string for user '%s'", user);
    return -1;
  }
  /* 'sall good */
  kvp_set(&k, "confirmation", 0);
  return trackdb_putdata(trackdb_usersdb, user, k, tid, 0);
}

/** @brief Confirm a user registration
 * @param user Username
 * @param confirmation Confirmation string
 * @param rightsp Where to put user rights
 * @return 0 on success, non-0 on error
 */
int trackdb_confirm(const char *user, const char *confirmation,
                    rights_type *rightsp) {
  int e;

  WITH_TRANSACTION(trackdb_confirm_tid(user, confirmation, rightsp, tid));
  switch(e) {
  case 0:
    info("registration confirmed for user '%s'", user);
    eventlog("user_confirm", user, (char *)0);
    return 0;
  case DB_NOTFOUND:
    error(0, "confirmation for nonexistent user '%s'", user);
    return -1;
  default:                              /* already reported */
    return -1;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
