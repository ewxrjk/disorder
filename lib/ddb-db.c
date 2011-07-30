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
#include "common.h"
#include "ddb.h"
#include "ddb-db.h"
#include <stdarg.h>

int ddb_bind_params(const char *context,
		    void *stmt,
		    ...) {
  va_list ap;
  int rc;

  va_start(ap, stmt);
  rc = ddb_vbind_params(context, stmt, ap);
  va_end(ap);
  return rc;
}

int ddb_unpick_columns(const char *context,
                       void *stmt,
                       ...) {
  va_list ap;
  int rc;

  va_start(ap, stmt);
  rc = ddb_vunpick_columns(context, stmt, ap);
  va_end(ap);
  return rc;
}

int ddb_unpick_row(const char *context,
                   void *stmt,
                   ...) {
  va_list ap;
  int rc;

  if((rc = ddb_retrieve_row(context, stmt)))
    return rc;
  va_start(ap, stmt);
  rc = ddb_vunpick_columns(context, stmt, ap);
  va_end(ap);
  return rc;
}

int ddb_bind_and_execute(const char *context,
			 const char *sql,
			 ...) {
  int rc;
  void *stmt = NULL;
  va_list ap;

  if(ddb_create_statement(context, &stmt, sql))
    return DDB_DB_ERROR;
  va_start(ap, sql);
  rc = ddb_vbind_params(context, stmt, ap);
  va_end(ap);
  if(rc)
    goto error;
  switch(rc = ddb_retrieve_row(context, stmt)) {
  case DDB_OK:
  case DDB_NO_ROW:
    break;
  default:
    goto error;
  }
  if(ddb_destroy_statement(context, &stmt))
    return DDB_DB_ERROR;
  return DDB_OK;
 error:
  ddb_destroy_statement(context, &stmt);
  return rc;
}

int ddb_execute_sql(const char *context,
		    const char *sql) {
  return ddb_bind_and_execute(context, sql, P_END);
}

int ddb_create_bind(const char *context, void **stmtp, const char *sql,
                    ...) {
  int rc;
  va_list ap;
  void *stmt;

  if((rc = ddb_create_statement(context, &stmt, sql)))
    return rc;
  va_start(ap, sql);
  rc = ddb_vbind_params(context, stmt, ap);
  va_end(ap);
  if(rc)
    ddb_destroy_statement(context, stmt);
  else
    *stmtp = stmt;
  return rc;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
