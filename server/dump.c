/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file server/dump.c
 * @brief Dump and restore database contents
 */
#include "disorder-server.h"

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "dump", no_argument, 0, 'd' },
  { "undump", no_argument, 0, 'u' },
  { "debug", no_argument, 0, 'D' },
  { "recover", no_argument, 0, 'r' },
  { "recover-fatal", no_argument, 0, 'R' },
  { "recompute-aliases", no_argument, 0, 'a' },
  { "remove-pathless", no_argument, 0, 'P' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-dump [OPTIONS] --dump|--undump PATH\n"
	  "  disorder-dump [OPTIONS] --recompute-aliases\n"
	  "Options:\n"
	  "  --help, -h               Display usage message\n"
	  "  --version, -V            Display version number\n"
	  "  --config PATH, -c PATH   Set configuration file\n"
	  "  --dump, -d               Dump state to PATH\n"
	  "  --undump, -u             Restore state from PATH\n"
	  "  --recover, -r            Run database recovery\n"
	  "  --recompute-aliases, -a  Recompute aliases\n"
	  "  --remove-pathless, -P    Remove pathless tracks\n"
	  "  --debug                  Debug mode\n");
  xfclose(stdout);
  exit(0);
}

/** @brief Dump one record
 * @param s Output stream
 * @param tag Tag for error messages
 * @param letter Prefix leter for dumped record
 * @param dbname Database name
 * @param db Database handle
 * @param tid Transaction handle
 * @return 0 or @c DB_LOCK_DEADLOCK
 */
static int dump_one(struct sink *s,
                    const char *tag,
                    int letter,
                    const char *dbname,
                    DB *db,
                    DB_TXN *tid) {
  int err;
  DBC *cursor;
  DBT k, d;

  /* dump the preferences */
  cursor = trackdb_opencursor(db, tid);
  err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                      DB_FIRST);
  while(err == 0) {
    if(sink_writec(s, letter) < 0
       || urlencode(s, k.data, k.size)
       || sink_writec(s, '\n') < 0
       || urlencode(s, d.data, d.size)
       || sink_writec(s, '\n') < 0)
      disorder_fatal(errno, "error writing to %s", tag);
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_NEXT);
  }
  switch(err) {
  case DB_LOCK_DEADLOCK:
    trackdb_closecursor(cursor);
    return err;
  case DB_NOTFOUND:
    return trackdb_closecursor(cursor);
  case 0:
    assert(!"cannot happen");
  default:
    disorder_fatal(0, "error reading %s: %s", dbname, db_strerror(err));
  }
}

static struct {
  int letter;
  const char *dbname;
  DB **db;
} dbtable[] = {
  { 'P', "prefs.db",     &trackdb_prefsdb },
  { 'G', "global.db",    &trackdb_globaldb },
  { 'U', "users.db",     &trackdb_usersdb },
  { 'W', "schedule.db",  &trackdb_scheduledb },
  { 'L', "playlists.db", &trackdb_playlistsdb },
  /* avoid 'T' and 'S' for now */
};
#define NDBTABLE (sizeof dbtable / sizeof *dbtable)

/* dump prefs to FP, return nonzero on error */
static void do_dump(FILE *fp, const char *tag) {
  DB_TXN *tid;
  struct sink *s = sink_stdio(tag, fp);

  for(;;) {
    tid = trackdb_begin_transaction();
    if(fseek(fp, 0, SEEK_SET) < 0)
      disorder_fatal(errno, "error calling fseek");
    if(fflush(fp) < 0)
      disorder_fatal(errno, "error calling fflush");
    if(ftruncate(fileno(fp), 0) < 0)
      disorder_fatal(errno, "error calling ftruncate");
    if(fprintf(fp, "V0") < 0)
      disorder_fatal(errno, "error writing to %s", tag);
    for(size_t n = 0; n < NDBTABLE; ++n)
      if(dump_one(s, tag,
                  dbtable[n].letter, dbtable[n].dbname, *dbtable[n].db,
                  tid))
        goto fail;
    
    if(fputs("E\n", fp) < 0)
      disorder_fatal(errno, "error writing to %s", tag);
    break;
fail:
    disorder_info("aborting transaction and retrying dump");
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  if(fflush(fp) < 0) disorder_fatal(errno, "error writing to %s", tag);
  /* caller might not be paranoid so we are paranoid on their behalf */
  if(fsync(fileno(fp)) < 0) disorder_fatal(errno, "error syncing %s", tag);
}

/* delete all aliases prefs, return 0 or DB_LOCK_DEADLOCK */
static int remove_aliases(DB_TXN *tid, int remove_pathless) {
  DBC *cursor;
  int err;
  DBT k, d;
  struct kvp *data;
  int alias, pathless;

  disorder_info("removing aliases");
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if((err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_FIRST)) == DB_LOCK_DEADLOCK) {
    disorder_error(0, "cursor->c_get: %s", db_strerror(err));
    goto done;
  }
  while(err == 0) {
    data = kvp_urldecode(d.data, d.size);
    alias = !!kvp_get(data, "_alias_for");
    pathless = !kvp_get(data, "_path");
    if(pathless && !remove_pathless)
      disorder_info("no _path for %s", utf82mb(xstrndup(k.data, k.size)));
    if(alias || (remove_pathless && pathless)) {
      switch(err = cursor->c_del(cursor, 0)) {
      case 0: break;
      case DB_LOCK_DEADLOCK:
        disorder_error(0, "cursor->c_get: %s", db_strerror(err));
        goto done;
      default:
        disorder_fatal(0, "cursor->c_del: %s", db_strerror(err));
      }
    }
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d), DB_NEXT);
  }
  if(err == DB_LOCK_DEADLOCK) {
    disorder_error(0, "cursor operation: %s", db_strerror(err));
    goto done;
  }
  if(err != DB_NOTFOUND)
    disorder_fatal(0, "cursor->c_get: %s", db_strerror(err));
  err = 0;
done:
  if(trackdb_closecursor(cursor) && !err) err = DB_LOCK_DEADLOCK;
  return err;
}

/* truncate (i.e. empty) a database, return 0 or DB_LOCK_DEADLOCK */
static int truncdb(DB_TXN *tid, DB *db) {
  int err;
  u_int32_t count;

  switch(err = db->truncate(db, tid, &count, 0)) {
  case 0: break;
  case DB_LOCK_DEADLOCK:
    disorder_error(0, "db->truncate: %s", db_strerror(err));
    break;
  default:
    disorder_fatal(0, "db->truncate: %s", db_strerror(err));
  }
  return err;
}

/* read a DBT from FP, return 0 on success or -1 on input error */
static int undump_dbt(FILE *fp, const char *tag, DBT *dbt) {
  char *s;
  struct dynstr d;

  if(inputline(tag, fp, &s, '\n')) return -1;
  dynstr_init(&d);
  if(urldecode(sink_dynstr(&d), s, strlen(s)))
    disorder_fatal(0, "invalid URL-encoded data in %s", tag);
  dbt->data = d.vec;
  dbt->size = d.nvec;
  return 0;
}

/* undump from FP, return 0 or DB_LOCK_DEADLOCK */
static int undump_from_fp(DB_TXN *tid, FILE *fp, const char *tag) {
  int err, c;

  disorder_info("undumping");
  if(fseek(fp, 0, SEEK_SET) < 0)
    disorder_fatal(errno, "error calling fseek on %s", tag);
  if((err = truncdb(tid, trackdb_prefsdb))) return err;
  if((err = truncdb(tid, trackdb_globaldb))) return err;
  if((err = truncdb(tid, trackdb_searchdb))) return err;
  if((err = truncdb(tid, trackdb_tagsdb))) return err;
  if((err = truncdb(tid, trackdb_usersdb))) return err;
  if((err = truncdb(tid, trackdb_scheduledb))) return err;
  c = getc(fp);
  while(!ferror(fp) && !feof(fp)) {
    for(size_t n = 0; n < NDBTABLE; ++n) {
      if(dbtable[n].letter == c) {
	DB *db = *dbtable[n].db;
	const char *dbname = dbtable[n].dbname;
        DBT k, d;

        if(undump_dbt(fp, tag, prepare_data(&k))
           || undump_dbt(fp, tag, prepare_data(&d)))
          break;
        switch(err = db->put(db, tid, &k, &d, 0)) {
        case 0:
          break;
        case DB_LOCK_DEADLOCK:
          disorder_error(0, "error updating %s: %s", dbname, db_strerror(err));
          return err;
        default:
          disorder_fatal(0, "error updating %s: %s", dbname, db_strerror(err));
        }
        goto next;
      }
    }
    
    switch(c) {
    case 'V':
      c = getc(fp);
      if(c != '0')
        disorder_fatal(0, "unknown version '%c'", c);
      break;
    case 'E':
      return 0;
    case '\n':
      break;
    default:
      if(c >= 32 && c <= 126)
        disorder_fatal(0, "unexpected character '%c'", c);
      else
        disorder_fatal(0, "unexpected character 0x%02X", c);
    }
  next:
    c = getc(fp);
  }
  if(ferror(fp))
    disorder_fatal(errno, "error reading %s", tag);
  else
    disorder_fatal(0, "unexpected EOF reading %s", tag);
  return 0;
}

/* recompute aliases and search database from prefs, return 0 or
 * DB_LOCK_DEADLOCK */
static int recompute_aliases(DB_TXN *tid) {
  DBC *cursor;
  DBT k, d;
  int err;
  struct kvp *data;
  const char *path, *track;

  disorder_info("recomputing aliases");
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if((err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_FIRST)) == DB_LOCK_DEADLOCK) goto done;
  while(err == 0) {
    data = kvp_urldecode(d.data, d.size);
    track = xstrndup(k.data, k.size);
    if(!kvp_get(data, "_alias_for")) {
      if(!(path = kvp_get(data, "_path")))
	disorder_error(0, "%s is not an alias but has no path", utf82mb(track));
      else
	if((err = trackdb_notice_tid(track, path, tid)) == DB_LOCK_DEADLOCK)
	  goto done;
    }
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_NEXT);
  }
  switch(err) {
  case 0:
    break;
  case DB_NOTFOUND:
    err = 0;
    break;
  case DB_LOCK_DEADLOCK:
    break;
  default:
    disorder_fatal(0, "cursor->c_get: %s", db_strerror(err));
  }
done:
  if(trackdb_closecursor(cursor) && !err) err = DB_LOCK_DEADLOCK;
  return err;
}

/* restore prefs from FP */
static void do_undump(FILE *fp, const char *tag, int remove_pathless) {
  DB_TXN *tid;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(remove_aliases(tid, remove_pathless)
       || undump_from_fp(tid, fp, tag)
       || recompute_aliases(tid)) goto fail;
    break;
fail:
    disorder_info("aborting transaction and retrying undump");
    trackdb_abort_transaction(tid);
  }
  disorder_info("committing undump");
  trackdb_commit_transaction(tid);
}

/* just recompute alisaes */
static void do_recompute(int remove_pathless) {
  DB_TXN *tid;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(remove_aliases(tid, remove_pathless)
       || recompute_aliases(tid)) goto fail;
    break;
fail:
    disorder_info("aborting transaction and retrying recomputation");
    trackdb_abort_transaction(tid);
  }
  disorder_info("committing recomputed aliases");
  trackdb_commit_transaction(tid);
}

int main(int argc, char **argv) {
  int n, dump = 0, undump = 0, recover = TRACKDB_NO_RECOVER, recompute = 0;
  int remove_pathless = 0, fd;
  const char *path;
  char *tmp;
  FILE *fp;

  mem_init();
  while((n = getopt_long(argc, argv, "hVc:dDurRaP", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-dump");
    case 'c': configfile = optarg; break;
    case 'd': dump = 1; break;
    case 'u': undump = 1; break;
    case 'D': debugging = 1; break;
    case 'r': recover = TRACKDB_NORMAL_RECOVER;
    case 'R': recover = TRACKDB_FATAL_RECOVER;
    case 'a': recompute = 1; break;
    case 'P': remove_pathless = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(dump + undump + recompute != 1)
    disorder_fatal(0, "choose exactly one of --dump, --undump or --recompute-aliases");
  if(recompute) {
    if(optind != argc)
      disorder_fatal(0, "--recompute-aliases does not take a filename");
    path = 0;
  } else {
    if(optind >= argc)
      disorder_fatal(0, "missing dump file name");
    if(optind + 1 < argc)
      disorder_fatal(0, "specify only a dump file name");
    path = argv[optind];
  }
  if(config_read(0, NULL))
    disorder_fatal(0, "cannot read configuration");
  trackdb_init(recover|TRACKDB_MAY_CREATE);
  trackdb_open(TRACKDB_NO_UPGRADE);
  if(dump) {
    /* We write to a temporary file and rename into place.  We make
     * sure the permissions are tight from the start. */
    byte_xasprintf(&tmp, "%s.%lx.tmp", path, (unsigned long)getpid());
    if((fd = open(tmp, O_CREAT|O_TRUNC|O_WRONLY, 0600)) < 0)
      disorder_fatal(errno, "error opening %s", tmp);
    if(!(fp = fdopen(fd, "w")))
      disorder_fatal(errno, "fdopen on %s", tmp);
    do_dump(fp, tmp);
    if(fclose(fp) < 0) disorder_fatal(errno, "error closing %s", tmp);
    if(rename(tmp, path) < 0)
      disorder_fatal(errno, "error renaming %s to %s", tmp, path);
  } else if(undump) {
    /* the databases or logfiles might end up with wrong permissions
     * if new ones are created */
    if(getuid() == 0)
      disorder_info("you might need to chown database files");
    if(!(fp = fopen(path, "r")))
      disorder_fatal(errno, "error opening %s", path);
    do_undump(fp, path, remove_pathless);
    xfclose(fp);
  } else if(recompute) {
    do_recompute(remove_pathless);
  }
  trackdb_close();
  trackdb_deinit(NULL);
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
