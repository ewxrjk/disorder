/*
 * This file is part of DisOrder
 * Copyright © 2011 Richard Kettlewell
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
/** @file lib/ddb-sqlite.c
 * @brief DisOrder track database - SQLite implementation
 */
#include "common.h"

/* TODO fully support DDB_DB_BUSY */

#include "ddb.h"
#include "ddb-db.h"
#include "defs.h"
#include "log.h"
#include "configuration.h"
#include "mem.h"
#include <sqlite3.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

static sqlite3 *dbhandle;

const char *ddb_sqlite_path;

#define DBTIMEOUT (10 * 1000)           /* milliseconds */

// Setup and teardown ---------------------------------------------------------

void ddb_open(unsigned ddbflags) {
  int sqliteflags = 0, initialcreation = 0;

  if(!ddb_sqlite_path)
    ddb_sqlite_path = config_get_file("s3.db");
  assert(!dbhandle);
  /* Translate flags */
  if(ddbflags & DDB_READONLY)
    sqliteflags |= SQLITE_OPEN_READONLY;
  if(ddbflags & DDB_READWRITE)
    sqliteflags |= SQLITE_OPEN_READWRITE;
  if(ddbflags & DDB_CREATE) {
    sqliteflags |= SQLITE_OPEN_CREATE;
    /* See if this is initial creation */
    if(access(ddb_sqlite_path, F_OK) == 0)
      initialcreation = 0;
    else if(errno == ENOENT)
      initialcreation = 1;
    else
      disorder_fatal(errno, "%s", ddb_sqlite_path);
  }
  int rc = sqlite3_open_v2(ddb_sqlite_path, &dbhandle, sqliteflags, NULL);
  if(rc)
    disorder_fatal(0, "sqlite3_open_v2 %s: error code %d", ddb_sqlite_path, rc);
  if(sqlite3_busy_timeout(dbhandle, DBTIMEOUT))
    disorder_fatal(0, "sqlite3_busy_timeout: %s", sqlite3_errmsg(dbhandle));
  if(initialcreation) {
    disorder_info("Initializing database schema");
    if(ddb_execute_sql("creating tables", ddb_createdb_sql) != DDB_OK) {
      unlink(ddb_sqlite_path);
      disorder_fatal(0, "failed to create database");
    }
  }
}

void ddb_close(void) {
  if(dbhandle) {
    int rc = sqlite3_close(dbhandle);
    if(rc)
      disorder_fatal(0, "sqlite3_close: error code %d", rc);
    dbhandle = NULL;
  }
}

// Utilities ------------------------------------------------------------------

static int translate_error(int sqlite_error) {
  switch(sqlite_error) {
  case SQLITE_OK:
    return DDB_OK;
  case SQLITE_BUSY:
    return DDB_DB_BUSY;
  default:
    return DDB_DB_ERROR;
  }
}

int ddb_create_statement(const char *context, void **stmtp, const char *sql) {
  sqlite3_stmt *stmt;
  int sqlite_error;

  D(("ddb_create_statement %s", sql));
  if((sqlite_error = sqlite3_prepare_v2(dbhandle,
                                        sql,
                                        -1,
                                        &stmt,
                                        NULL))) {
    disorder_error(0, "%s: sqlite3_prepare_v2: %s",
		   context, sqlite3_errmsg(dbhandle));
    return translate_error(sqlite_error);
  }
  *stmtp = stmt;
  return DDB_OK;
}

int ddb_destroy_statement(const char *context, void **stmtp) {
  int sqlite_error;

  if(*stmtp) {
    if((sqlite_error = sqlite3_finalize(*stmtp))) {
      disorder_error(0, "%s: sqlite3_finalize: %s",
		     context, sqlite3_errmsg(dbhandle));
      *stmtp = NULL;
      return translate_error(sqlite_error);
    }
    *stmtp = NULL;
  }
  return DDB_OK;
}

int ddb_vbind_params(const char *context,
		     void *stmt,
		     va_list ap) {
  int type, param = 1, sqlite_error;

  while((type = va_arg(ap, int)) >= 0) {
    switch(type) {
    case P_INT: {
      int n = va_arg(ap, int);
      D(("ddb_vbind_params %d P_INT %d", param, n));
      if((sqlite_error = sqlite3_bind_int(stmt, param, n))) {
	disorder_error(0, "%s: sqlite3_bind_int: %s",
		       context, sqlite3_errmsg(dbhandle));
	goto error;
      }
      break;
    }
    case P_INT64: {
      int64_t n = va_arg(ap, int64_t);
      D(("ddb_vbind_params %d P_INT64 %"PRId64, param, n));
      if((sqlite_error = sqlite3_bind_int64(stmt, param, n))) {
	disorder_error(0, "%s: sqlite3_bind_int64: %s",
		       context, sqlite3_errmsg(dbhandle));
	goto error;
      }
      break;
    }
    case P_STRING: {
      const char *s = va_arg(ap, const char *);
      D(("ddb_vbind_params %d P_STRING %s", param, s));
      if((sqlite_error = sqlite3_bind_text(stmt, param,
                                           s, strlen(s),
                                           SQLITE_STATIC))) {
	disorder_error(0, "%s: sqlite3_bind_text: %s",
		       context, sqlite3_errmsg(dbhandle));
	goto error;
      }
      break;
    }
    default:
      if(type & P_NULL) {
        D(("ddb_vbind_params %d P_NULL", param));
	/* Optionally consume a parameter */
	switch(type) {
	case P_INT|P_NULL: va_arg(ap, int); break;
	case P_INT64|P_NULL: va_arg(ap, int64_t); break;
	case P_STRING|P_NULL: va_arg(ap, char *); break;
	}
	if((sqlite_error = sqlite3_bind_null(stmt, param))) {
	  disorder_error(0, "%s: sqlite3_bind_null: %s",
			 context, sqlite3_errmsg(dbhandle));
	  goto error;
	}
      } else
	abort();
      break;
    }
    ++param;
  }
  return DDB_OK;
 error:
  return translate_error(sqlite_error);
}

int ddb_retrieve_row(const char *context,
		     void *stmt) {
  D(("ddb_retrieve_row"));
  int sqlite_error = sqlite3_step(stmt);
  switch(sqlite_error) {
  case SQLITE_ROW:
    return DDB_OK;
  case SQLITE_DONE:
    return DDB_NO_ROW;
  default:
    disorder_error(0, "%s: sqlite3_step: %s",
		   context, sqlite3_errmsg(dbhandle));
    return translate_error(sqlite_error);
  }
}

int ddb_vretrieve_columns(const char attribute((unused)) *context,
			  void *stmt,
			  va_list ap) {
  int column = 0, type;
  while((type = va_arg(ap, int)) >= 0) {
    switch(type) {
    case P_INT: {
      int *resultp = va_arg(ap, int *);
      int result = sqlite3_column_int(stmt, column);
      D(("ddb_vretrieve_columns %d P_INT %d", column, result));
      if(resultp)
	*resultp = result;
      break;
    }
    case P_INT64: {
      int64_t *resultp = va_arg(ap, int64_t *);
      int64_t result = sqlite3_column_int64(stmt, column);
      D(("ddb_vretrieve_columns %d P_INT64 %"PRId64, column, result));
      if(resultp)
	*resultp = result;
      break;
    }
    case P_STRING: {
      char **resultp = va_arg(ap, char **);
      const unsigned char *result = sqlite3_column_text(stmt, column);
      D(("ddb_vretrieve_columns %d P_STRING %s",
         column, result ? (char *)result : "(null)"));
      if(resultp)
	*resultp = result ? xstrdup((char *)result) : NULL;
      break;
    }
    default:
      abort();
    }
    ++column;
  }
  /* TODO we can call sqlite3_errcode() to see what went wrong, but we
   * do not know if anything *did* go wrong. */
  return DDB_OK;
}

int ddb_begin_transaction(const char *context) {
  return ddb_execute_sql(context, "BEGIN TRANSACTION");
}

int ddb_commit_transaction(const char *context) {
  return ddb_execute_sql(context, "COMMIT TRANSACTION");
}

int ddb_rollback_transaction(const char *context) {
  return ddb_execute_sql(context, "ROLLBACK TRANSACTION");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
