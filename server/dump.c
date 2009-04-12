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
  { "trackdb", no_argument, 0, 't' },
  { "searchdb", no_argument, 0, 's' },
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

/* dump prefs to FP, return nonzero on error */
static void do_dump(FILE *fp, const char *tag,
		    int tracksdb, int searchdb) {
  DBC *cursor = 0;
  DB_TXN *tid;
  struct sink *s = sink_stdio(tag, fp);
  int err;
  DBT k, d;

  for(;;) {
    tid = trackdb_begin_transaction();
    if(fseek(fp, 0, SEEK_SET) < 0)
      fatal(errno, "error calling fseek");
    if(fflush(fp) < 0)
      fatal(errno, "error calling fflush");
    if(ftruncate(fileno(fp), 0) < 0)
      fatal(errno, "error calling ftruncate");
    if(fprintf(fp, "V%c\n", (tracksdb || searchdb) ? '1' : '0') < 0)
      fatal(errno, "error writing to %s", tag);
    /* dump the preferences */
    cursor = trackdb_opencursor(trackdb_prefsdb, tid);
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_FIRST);
    while(err == 0) {
      if(fputc('P', fp) < 0
         || urlencode(s, k.data, k.size)
         || fputc('\n', fp) < 0
         || urlencode(s, d.data, d.size)
         || fputc('\n', fp) < 0)
        fatal(errno, "error writing to %s", tag);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_NEXT);
    }
    if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
    cursor = 0;

    /* dump the global preferences */
    cursor = trackdb_opencursor(trackdb_globaldb, tid);
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_FIRST);
    while(err == 0) {
      if(fputc('G', fp) < 0
         || urlencode(s, k.data, k.size)
         || fputc('\n', fp) < 0
         || urlencode(s, d.data, d.size)
         || fputc('\n', fp) < 0)
        fatal(errno, "error writing to %s", tag);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_NEXT);
    }
    if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
    cursor = 0;
    
    /* dump the users */
    cursor = trackdb_opencursor(trackdb_usersdb, tid);
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_FIRST);
    while(err == 0) {
      if(fputc('U', fp) < 0
         || urlencode(s, k.data, k.size)
         || fputc('\n', fp) < 0
         || urlencode(s, d.data, d.size)
         || fputc('\n', fp) < 0)
        fatal(errno, "error writing to %s", tag);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_NEXT);
    }
    if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
    cursor = 0;

    /* dump the schedule */
    cursor = trackdb_opencursor(trackdb_scheduledb, tid);
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                        DB_FIRST);
    while(err == 0) {
      if(fputc('W', fp) < 0
         || urlencode(s, k.data, k.size)
         || fputc('\n', fp) < 0
         || urlencode(s, d.data, d.size)
         || fputc('\n', fp) < 0)
        fatal(errno, "error writing to %s", tag);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_NEXT);
    }
    if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
    cursor = 0;
    
    
    if(tracksdb) {
      cursor = trackdb_opencursor(trackdb_tracksdb, tid);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
			  DB_FIRST);
      while(err == 0) {
	if(fputc('T', fp) < 0
	   || urlencode(s, k.data, k.size)
	   || fputc('\n', fp) < 0
	   || urlencode(s, d.data, d.size)
	   || fputc('\n', fp) < 0)
	  fatal(errno, "error writing to %s", tag);
	err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
			    DB_NEXT);
      }
      if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
      cursor = 0;
    }

    if(searchdb) {
      cursor = trackdb_opencursor(trackdb_searchdb, tid);
      err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
			  DB_FIRST);
      while(err == 0) {
	if(fputc('S', fp) < 0
	   || urlencode(s, k.data, k.size)
	   || fputc('\n', fp) < 0
	   || urlencode(s, d.data, d.size)
	   || fputc('\n', fp) < 0)
	  fatal(errno, "error writing to %s", tag);
	err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
			    DB_NEXT);
      }
      if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }      cursor = 0;
    }

    if(fputs("E\n", fp) < 0) fatal(errno, "error writing to %s", tag);
    if(err == DB_LOCK_DEADLOCK) {
      error(0, "c->c_get: %s", db_strerror(err));
      goto fail;
    }
    if(err && err != DB_NOTFOUND)
      fatal(0, "cursor->c_get: %s", db_strerror(err));
    if(trackdb_closecursor(cursor)) { cursor = 0; goto fail; }
    break;
fail:
    trackdb_closecursor(cursor);
    cursor = 0;
    info("aborting transaction and retrying dump");
    trackdb_abort_transaction(tid);
  }
  trackdb_commit_transaction(tid);
  if(fflush(fp) < 0) fatal(errno, "error writing to %s", tag);
  /* caller might not be paranoid so we are paranoid on their behalf */
  if(fsync(fileno(fp)) < 0) fatal(errno, "error syncing %s", tag);
}

/* delete all aliases prefs, return 0 or DB_LOCK_DEADLOCK */
static int remove_aliases(DB_TXN *tid, int remove_pathless) {
  DBC *cursor;
  int err;
  DBT k, d;
  struct kvp *data;
  int alias, pathless;

  info("removing aliases");
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if((err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_FIRST)) == DB_LOCK_DEADLOCK) {
    error(0, "cursor->c_get: %s", db_strerror(err));
    goto done;
  }
  while(err == 0) {
    data = kvp_urldecode(d.data, d.size);
    alias = !!kvp_get(data, "_alias_for");
    pathless = !kvp_get(data, "_path");
    if(pathless && !remove_pathless)
      info("no _path for %s", utf82mb(xstrndup(k.data, k.size)));
    if(alias || (remove_pathless && pathless)) {
      switch(err = cursor->c_del(cursor, 0)) {
      case 0: break;
      case DB_LOCK_DEADLOCK:
        error(0, "cursor->c_get: %s", db_strerror(err));
        goto done;
      default:
        fatal(0, "cursor->c_del: %s", db_strerror(err));
      }
    }
    err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d), DB_NEXT);
  }
  if(err == DB_LOCK_DEADLOCK) {
    error(0, "cursor operation: %s", db_strerror(err));
    goto done;
  }
  if(err != DB_NOTFOUND) fatal(0, "cursor->c_get: %s", db_strerror(err));
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
    error(0, "db->truncate: %s", db_strerror(err));
    break;
  default:
    fatal(0, "db->truncate: %s", db_strerror(err));
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
    fatal(0, "invalid URL-encoded data in %s", tag);
  dbt->data = d.vec;
  dbt->size = d.nvec;
  return 0;
}

/* undump from FP, return 0 or DB_LOCK_DEADLOCK */
static int undump_from_fp(DB_TXN *tid, FILE *fp, const char *tag) {
  int err, c;
  DBT k, d;
  const char *which_name;
  DB *which_db;

  info("undumping");
  if(fseek(fp, 0, SEEK_SET) < 0)
    fatal(errno, "error calling fseek on %s", tag);
  if((err = truncdb(tid, trackdb_prefsdb))) return err;
  if((err = truncdb(tid, trackdb_globaldb))) return err;
  if((err = truncdb(tid, trackdb_searchdb))) return err;
  if((err = truncdb(tid, trackdb_tagsdb))) return err;
  if((err = truncdb(tid, trackdb_usersdb))) return err;
  if((err = truncdb(tid, trackdb_scheduledb))) return err;
  c = getc(fp);
  while(!ferror(fp) && !feof(fp)) {
    switch(c) {
    case 'V':
      c = getc(fp);
      if(c != '0')
        fatal(0, "unknown version '%c'", c);
      break;
    case 'E':
      return 0;
    case 'P':
    case 'G':
    case 'U':
    case 'W':
      switch(c) {
      case 'P':
	which_db = trackdb_prefsdb;
	which_name = "prefs.db";
	break;
      case 'G':
	which_db = trackdb_globaldb;
	which_name = "global.db";
	break;
      case 'U':
	which_db = trackdb_usersdb;
	which_name = "users.db";
	break;
      case 'W':				/* for 'when' */
	which_db = trackdb_scheduledb;
	which_name = "scheduledb.db";
	break;
      default:
	abort();
      }
      if(undump_dbt(fp, tag, prepare_data(&k))
         || undump_dbt(fp, tag, prepare_data(&d)))
        break;
      switch(err = which_db->put(which_db, tid, &k, &d, 0)) {
      case 0:
        break;
      case DB_LOCK_DEADLOCK:
        error(0, "error updating %s: %s", which_name, db_strerror(err));
        return err;
      default:
        fatal(0, "error updating %s: %s", which_name, db_strerror(err));
      }
      break;
    case 'T':
    case 'S':
      if(undump_dbt(fp, tag, prepare_data(&k))
         || undump_dbt(fp, tag, prepare_data(&d)))
        break;
      /* We don't restore the tracks.db or search.db entries, instead
       * we recompute them */
      break;
    case '\n':
      break;
    }
    c = getc(fp);
  }
  if(ferror(fp))
    fatal(errno, "error reading %s", tag);
  else
    fatal(0, "unexpected EOF reading %s", tag);
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

  info("recomputing aliases");
  cursor = trackdb_opencursor(trackdb_tracksdb, tid);
  if((err = cursor->c_get(cursor, prepare_data(&k), prepare_data(&d),
                          DB_FIRST)) == DB_LOCK_DEADLOCK) goto done;
  while(err == 0) {
    data = kvp_urldecode(d.data, d.size);
    track = xstrndup(k.data, k.size);
    if(!kvp_get(data, "_alias_for")) {
      if(!(path = kvp_get(data, "_path")))
	error(0, "%s is not an alias but has no path", utf82mb(track));
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
    fatal(0, "cursor->c_get: %s", db_strerror(err));
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
    info("aborting transaction and retrying undump");
    trackdb_abort_transaction(tid);
  }
  info("committing undump");
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
    info("aborting transaction and retrying recomputation");
    trackdb_abort_transaction(tid);
  }
  info("committing recomputed aliases");
  trackdb_commit_transaction(tid);
}

int main(int argc, char **argv) {
  int n, dump = 0, undump = 0, recover = TRACKDB_NO_RECOVER, recompute = 0;
  int tracksdb = 0, searchdb = 0, remove_pathless = 0, fd;
  const char *path;
  char *tmp;
  FILE *fp;

  mem_init();
  while((n = getopt_long(argc, argv, "hVc:dDutsrRaP", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-dump");
    case 'c': configfile = optarg; break;
    case 'd': dump = 1; break;
    case 'u': undump = 1; break;
    case 'D': debugging = 1; break;
    case 't': tracksdb = 1; break;
    case 's': searchdb = 1; break;
    case 'r': recover = TRACKDB_NORMAL_RECOVER;
    case 'R': recover = TRACKDB_FATAL_RECOVER;
    case 'a': recompute = 1; break;
    case 'P': remove_pathless = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  if(dump + undump + recompute != 1)
    fatal(0, "choose exactly one of --dump, --undump or --recompute-aliases");
  if((undump || recompute) && (tracksdb || searchdb))
    fatal(0, "--trackdb and --searchdb with --undump or --recompute-aliases");
  if(recompute) {
    if(optind != argc)
      fatal(0, "--recompute-aliases does not take a filename");
    path = 0;
  } else {
    if(optind >= argc)
      fatal(0, "missing dump file name");
    if(optind + 1 < argc)
      fatal(0, "specify only a dump file name");
    path = argv[optind];
  }
  if(config_read(0, NULL)) fatal(0, "cannot read configuration");
  trackdb_init(recover|TRACKDB_MAY_CREATE);
  trackdb_open(TRACKDB_NO_UPGRADE);
  if(dump) {
    /* We write to a temporary file and rename into place.  We make
     * sure the permissions are tight from the start. */
    byte_xasprintf(&tmp, "%s.%lx.tmp", path, (unsigned long)getpid());
    if((fd = open(tmp, O_CREAT|O_TRUNC|O_WRONLY, 0600)) < 0)
      fatal(errno, "error opening %s", tmp);
    if(!(fp = fdopen(fd, "w")))
      fatal(errno, "fdopen on %s", tmp);
    do_dump(fp, tmp, tracksdb, searchdb);
    if(fclose(fp) < 0) fatal(errno, "error closing %s", tmp);
    if(rename(tmp, path) < 0)
      fatal(errno, "error renaming %s to %s", tmp, path);
  } else if(undump) {
    /* the databases or logfiles might end up with wrong permissions
     * if new ones are created */
    if(getuid() == 0) info("you might need to chown database files");
    if(!(fp = fopen(path, "r"))) fatal(errno, "error opening %s", path);
    do_undump(fp, path, remove_pathless);
    xfclose(fp);
  } else if(recompute) {
    do_recompute(remove_pathless);
  }
  trackdb_close();
  trackdb_deinit();
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
