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
/** @file lib/ddb-db.h
 * @brief DisOrder database abstraction
 */
#ifndef DDB_DB_H
#define DDB_DB_H

/** @brief Bind an @c int */
#define P_INT 1

/** @brief Bind an @c int64_t */
#define P_INT64 2

/** @brief Bind a string */
#define P_STRING 3

/** @brief Bind a @c time_t (as an integer) */
#define P_TIME 4

/** @brief Bind a NULL
 *
 * Also use @c P_NULL|P_INT etc to consume an argument.
 */
#define P_NULL 128

/** @brief End of arguments lists */
#define P_END (-1)

/** @brief No row was retrieved */
#define DDB_NO_ROW 256

/** @brief Database is busy, try again */
#define DDB_DB_BUSY 257

/** @brief Invoke a call in a transaction, retrying as needed
 * @param CONTEXT Context string for errors
 * @param EXPR Expression to evaluate
 **/
#define TRANSACTION_WRAP(CONTEXT, EXPR) do {            \
  int rc, rct;                                          \
  for(;;) {                                             \
    if((rct = ddb_begin_transaction(CONTEXT))) {        \
      if(rct == DDB_DB_BUSY)                            \
        continue;                                       \
      return rct;                                       \
    }                                                   \
    rc = (EXPR);                                        \
    switch(rc) {                                        \
    case DDB_DB_BUSY:                                   \
      if((rct = ddb_rollback_transaction(CONTEXT)))     \
        return rct;                                     \
      if(rc == DDB_DB_BUSY)                             \
        continue;                                       \
      break;                                            \
    case DDB_OK:                                        \
      rct = ddb_commit_transaction(CONTEXT);            \
      if(rct == DDB_DB_BUSY)                            \
        continue;                                       \
      return rct;                                       \
    default:                                            \
      if((rct = ddb_rollback_transaction(CONTEXT)))     \
        return rct;                                     \
      return rc;                                        \
    }                                                   \
  }                                                     \
} while(0)

/** @brief Create a SQL statement handle
 * @param context Context string for errors
 * @param stmtp Where to store statement handle
 * @param sql SQL statement
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_create_statement(const char *context, void **stmtp, const char *sql);

/** @brief Create a SQL statement handle and bind parameters
 * @param context Context string for errors
 * @param stmtp Where to store statement handle
 * @param sql SQL statement
 * @param ... Parameters
 * @return Status code
 *
 * Equivalent to calling ddb_create_statement() followed by
 * ddb_bind_parameters().
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_create_bind(const char *context, void **stmtp, const char *sql,
                    ...);

/** @brief Destroy a SQL statement handle
 * @param context Context string for errors
 * @param stmtp Where to store statement handle
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_destroy_statement(const char *context, void **stmtp);

/** @brief Bind statement parameters
 * @param context Context string for errors
 * @param stmt Statement handle
 * @param ap Parameters
 * @return Status code
 *
 * The parameter list should be a series of P_... each values
 * followed by a value of the appropriate type, terminated by
 * P_END.
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_vbind_params(const char *context,
		     void *stmt,
		     va_list ap);

/** @brief Bind statement parameters
 * @param context Context string for errors
 * @param stmt Statement handle
 * @param ... Parameters
 * @return Status code
 *
 * The parameter list should be a series of P_... each values
 * followed by a value of the appropriate type, terminated by
 * P_END.
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_bind_params(const char *context,
		    void *stmt,
		    ...);

/** @brief Retrieve the next row
 * @param context Context string for errors
 * @param stmt Statement handle
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_NO_ROW
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_retrieve_row(const char *context,
		     void *stmt);

/** @brief Extract column values
 * @param context Context string for errors
 * @param stmt Statement handle
 * @param ap Column specification
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_vunpick_columns(const char *context,
                        void *stmt,
                        va_list ap);

/** @brief Extract column values
 * @param context Context string for errors
 * @param stmt Statement handle
 * @param ... Column specification
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_unpick_columns(const char *context,
                       void *stmt,
                       ...);

/** @brief Retrieve a row and unpick column values
 * @param context Context string for errors
 * @param stmt Statement handle
 * @param ... Column specification
 * @return Status code
 *
 * Equivalent to calling ddb_retrieve_row() followed by ddb_unpick_columns().
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 * - @ref DDB_NO_ROW
 */
int ddb_unpick_row(const char *context,
                   void *stmt,
                   ...);

/** @brief Execute a command with parameters bound
 * @param context Context string for errors
 * @param sql Template SQL
 * @param ... Parameters
 * @return Status code
 *
 * The parameter list should be a series of P_... each values
 * followed by a value of the appropriate type, terminated by
 * P_END.
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_bind_and_execute(const char *context,
			 const char *sql,
			 ...);

/** @brief Execute a fixed command
 * @param context Context string for errors
 * @param sql SQL
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_execute_sql(const char *context,
		    const char *sql);

/** @brief Open a database transaction
 * @param context Context string for errors
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_begin_transaction(const char *context);

/** @brief Commit a database transaction
 * @param context Context string for errors
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 * - @ref DDB_DB_BUSY
 */
int ddb_commit_transaction(const char *context);

/** @brief Roll back a database transaction
 * @param context Context string for errors
 * @return Status code
 *
 * Possible return values:
 * - @ref DDB_OK
 * - @ref DDB_DB_ERROR
 */
int ddb_rollback_transaction(const char *context);

extern const char ddb_createdb_sql[];
extern const char ddb_insert_user_sql[];
extern const char ddb_retrieve_user_sql[];
extern const char ddb_delete_user_sql[];
extern const char ddb_list_users_sql[];
extern const char ddb_set_user_sql[];
extern const char ddb_track_get_sql[];
extern const char ddb_track_update_availability_sql[];
extern const char ddb_track_new_sql[];

#endif /* DDB_DB_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
