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
/** @file lib/ddb-users.c
* @brief DisOrder track database - user support
*/
#include "common.h"
#include "ddb.h"
#include "ddb-db.h"
#include "log.h"
#include "vector.h"
#include "validity.h"

static int ddb_do_get_user(const char *name,
			   char **passwordp,
			   char **emailp,
			   char **confirmp,
			   rights_type *rightsp) {
  static const char context[] = "retrieving user";
  int rc;
  void *stmt = NULL;
  int64_t rights64;

  if((rc = ddb_create_bind(context, &stmt, ddb_retrieve_user_sql,
                           P_STRING, name,
			   P_END)))
    goto error;
  switch(rc = ddb_unpick_row(context, stmt,
                             P_STRING, passwordp,
                             P_STRING, emailp,
                             P_STRING, confirmp,
                             P_INT64, &rights64,
                             P_END)) {
  case DDB_NO_ROW:
    rc = DDB_NO_SUCH_USER;
    goto error;
  default:
    goto error;
  case DDB_OK:
    if(rightsp)
      *rightsp = rights64;
    return ddb_destroy_statement(context, &stmt);
  }
 error:
  ddb_destroy_statement(context, &stmt);
  return rc;
}

int ddb_get_user(const char *name,
		 char **passwordp,
		 char **emailp,
		 char **confirmp,
		 rights_type *rightsp) {
  TRANSACTION_WRAP("retrieving user",
                   ddb_do_get_user(name, passwordp, emailp, confirmp, rightsp));
}

static int ddb_do_create_user(const char *name,
			      const char *password,
			      const char *email,
			      const char *confirm,
			      rights_type rights) {
  int rc;
  static const char context[] = "creating user";

  if((rc = ddb_do_get_user(name, NULL, NULL, NULL, NULL)) != DDB_NO_SUCH_USER) {
    if(rc == DDB_OK) {
      disorder_error(0, "user %s already exists", name);
      return DDB_USER_EXISTS;
    }
    return rc;
  }
  return ddb_bind_and_execute(context,
                              ddb_insert_user_sql,
                              P_STRING, name,
                              password ? P_STRING : P_NULL|P_STRING, password,
                              email ? P_STRING : P_NULL|P_STRING, email,
                              confirm ? P_STRING : P_NULL|P_STRING, confirm,
                              P_INT64, (int64_t)rights,
                              P_END);
}

int ddb_create_user(const char *name,
                    const char *password,
                    const char *email,
                    const char *confirm,
                    rights_type rights) {
  if(!valid_username(name)) {
    disorder_error(0, "invalid user name '%s'", name);
    return DDB_INVALID_USERNAME;
  }
  TRANSACTION_WRAP("creating user",
                   ddb_do_create_user(name, password, email, confirm, rights));
}

static int ddb_do_delete_user(const char *name) {
  int rc;
  static const char context[] = "deleting user";

  if((rc = ddb_do_get_user(name, NULL, NULL, NULL, NULL)) != DDB_OK) {
    if(rc == DDB_NO_SUCH_USER)
      disorder_error(0, "user %s does not exist", name);
    return rc;
  }
  return ddb_bind_and_execute(context,
                              ddb_delete_user_sql,
                              P_STRING, name,
                              P_END);
}

int ddb_delete_user(const char *name) {
  TRANSACTION_WRAP("deleting user",
                   ddb_do_delete_user(name));
}

static int ddb_do_list_users(char ***namesp,
			     int *nnamesp) {
  static const char context[] = "listing users";
  int rc;
  struct vector v[1];
  void *stmt = NULL;
  char *name;

  vector_init(v);
  if((rc = ddb_create_statement(context, &stmt, ddb_list_users_sql)))
    return rc;
  while((rc = ddb_retrieve_row(context, stmt)) == DDB_OK) {
    if((rc = ddb_unpick_columns(context, stmt,
                                P_STRING, &name,
                                P_END)))
      goto error;
    vector_append(v, xstrdup(name));
  }
  if(rc != DDB_NO_ROW)
    goto error;
  vector_terminate(v);
  if(namesp)
    *namesp = v->vec;
  if(nnamesp)
    *nnamesp = v->nvec;
  return ddb_destroy_statement(context, &stmt);
 error:
  ddb_destroy_statement(context, &stmt);
  return rc;
}

int ddb_list_users(char ***namesp,
                   int *nnamesp) {
  TRANSACTION_WRAP("listing users",
                   ddb_do_list_users(namesp, nnamesp));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
