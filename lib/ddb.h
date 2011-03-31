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
/** @file lib/ddb.h
 * @brief DisOrder track database
 *
 * (The new interface!)
 */
#ifndef DDB_H
#define DDB_H

#include "rights.h"

/** @brief Open database in read-write mode */
#define DDB_READWRITE 1

/** @brief Open database in read-only mode */
#define DDB_READONLY 2

/** @brief Create database if it does not exist */
#define DDB_CREATE 4

/** @brief Success */
#define DDB_OK 0

/** @brief Database-level error */
#define DDB_DB_ERROR 1

/** @brief Invalid user name */
#define DDB_INVALID_USERNAME 2

/** @brief User exists */
#define DDB_USER_EXISTS 3

/** @brief No such user */
#define DDB_NO_SUCH_USER 4

/** @brief Open the database
 * @param ddbflags Flags
 *
 * Possible bits in @p ddbflags:
 * - @ref DDB_READWRITE
 * - @ref DDB_READONLY
 * - @ref DDB_CREATE
 */
void ddb_open(unsigned ddbflags);

/** @brief Close the database */
void ddb_close(void);

/** @brief Create a user
 * @param name Username
 * @param password Initial password or NULL
 * @param email Email address or NULL
 * @param confirm Required confirmation string or NULL
 * @param rights Initial rights
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_USER_EXISTS
 * - @ref DDB_DB_ERROR
 */
int ddb_create_user(const char *name,
                    const char *password,
                    const char *email,
                    const char *confirm,
                    rights_type rights);

/** @brief Look up a user
 * @param name Username
 * @param passwordp Where to store password, or NULL
 * @param emailp Where to store email address, or NULL
 * @param confirmp Where to store confirmation string, or NULL
 * @param rightsp Where to store rights, or NULL
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_NO_SUCH_USER
 * - @ref DDB_DB_ERROR
 */
int ddb_get_user(const char *name,
                 char **passwordp,
                 char **emailp,
                 char **confirmp,
                 rights_type *rightsp);

/** @brief Delete a user
 * @param name Username
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_NO_SUCH_USER
 * - @ref DDB_DB_ERROR
 */
int ddb_delete_user(const char *name);

/** @brief List users
 * @param namesp Where to store list of users
 * @param nnamesp Where to store count of users or NULL
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 */
int ddb_list_users(char ***namesp,
                   int *nnamesp);

/** @brief Path to database file
 *
 * Set by first call to ddb_open() if not overridden by caller.
 */
extern const char *ddb_sqlite_path;

#endif /* DDB_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
