/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/log-impl.h @brief Errors and logging */

/** @brief Log an error and quit
 *
 * If @c ${DISORDER_FATAL_ABORT} is defined (as anything) then the process
 * is aborted, so you can get a backtrace.
 */
void disorder_fatal(int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_CRIT, errno_value, msg, ap);
  va_end(ap);
  if(getenv("DISORDER_FATAL_ABORT")) abort();
  exitfn(EXIT_FAILURE);
}

/** @brief Log an error */
void disorder_error(int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_ERR, errno_value, msg, ap);
  va_end(ap);
}

/** @brief Log an informational message */
void disorder_info(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_INFO, 0, msg, ap);
  va_end(ap);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
