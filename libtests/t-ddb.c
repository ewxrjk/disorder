/*
 * This file is part of DisOrder.
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
#include "test.h"
#include "ddb.h"
#include "rights.h"

static void test_users(void) {
  char **users;
  int nusers;
  char *password, *email, *confirm;
  rights_type rights;

  /* Initial state should be empty */
  assert(ddb_list_users(&users, &nusers) == DDB_OK);
  assert(nusers == 0);
  assert(users[0] == NULL);

  /* Create a user */
  assert(ddb_create_user("fred", "fredpw", NULL, NULL, RIGHTS__MASK) == DDB_OK);
  assert(ddb_list_users(&users, &nusers) == DDB_OK);
  assert(nusers == 1);
  assert(!strcmp(users[0], "fred"));
  assert(users[1] == NULL);

  /* Cannot create duplicates */
  assert(ddb_create_user("fred", "fredpw", NULL, NULL, RIGHTS__MASK) == DDB_USER_EXISTS);

  /* Retrieve user */
  assert(ddb_get_user("fred", &password, &email, &confirm, &rights) == DDB_OK);
  assert(!strcmp(password, "fredpw"));
  assert(email == NULL);
  assert(confirm == NULL);
  assert(rights == RIGHTS__MASK);

  /* Retrieve nonexistent user */
  assert(ddb_get_user("bob", &password, &email, &confirm, &rights) == DDB_NO_SUCH_USER);

  /* Delete user */
  assert(ddb_delete_user("fred") == DDB_OK);

  /* Cannot delete nonexistent users */
  assert(ddb_delete_user("fred") == DDB_NO_SUCH_USER);
  assert(ddb_delete_user("bob") == DDB_NO_SUCH_USER);
}

static void test_ddb(void) {
  /* Set database path and make sure there isn't one there already */
  ddb_sqlite_path = "test.db";
  unlink(ddb_sqlite_path);

  ddb_open(DDB_READWRITE|DDB_CREATE);
  test_users();
  ddb_close();

  unlink(ddb_sqlite_path);
}

TEST(ddb);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
