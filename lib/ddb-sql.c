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
/** @file lib/ddb-sql.c
 * @brief DisOrder track database - SQL statements
 */
#include "common.h"

#include "ddb.h"
#include "ddb-db.h"

const char ddb_createdb_sql[] =
  "CREATE TABLE Users (\n"
  "  user VARCHAR(32) NOT NULL PRIMARY KEY,\n"
  "  password VARCHAR(32),\n"
  "  email VARCHAR(128),\n"
  "  confirm VARCHAR(128),\n"
  "  rights INTEGER\n"
  ")\n"
;

const char ddb_insert_user_sql[] =
  "INSERT INTO Users (user,password,email,confirm,rights) VALUES(?,?,?,?,?)"
;

const char ddb_retrieve_user_sql[] =
  "SELECT password,email,confirm,rights FROM Users WHERE user = ?";
;

const char ddb_delete_user_sql[] =
  "DELETE FROM Users WHERE user = ?"
;

const char ddb_list_users_sql[] =
  "SELECT user FROM Users ORDER BY user"
;

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
