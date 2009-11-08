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
/** @file lib/log.h
 * @brief Errors and logging
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

struct log_output;

void set_progname(char **argv);

void elog(int pri, int errno_value, const char *fmt, va_list ap);

void disorder_fatal(int errno_value, const char *msg, ...) attribute((noreturn))
  attribute((format (printf, 2, 3)));
void disorder_error(int errno_value, const char *msg, ...)
  attribute((format (printf, 2, 3)));
void disorder_info(const char *msg, ...)
  attribute((format (printf, 1, 2)));
void disorder_debug(const char *msg, ...)
  attribute((format (printf, 1, 2)));
/* report a message of the given class.  @errno_value@ if present an
 * non-zero is included.  @fatal@ terminates the process. */

/** @brief Backward-compatibility alias for disorder_fatal() */
#define fatal disorder_fatal

/** @brief Backward-compatibility alias for disorder_error() */
#define error disorder_error

/** @brief Backward-compatibility alias for disorder_info() */
#define info disorder_info

/** @brief Backward-compatibility alias for disorder_debug() */
#define debug disorder_debug

extern int debugging;
/* set when debugging enabled */

extern void (*exitfn)(int) attribute((noreturn));
/* how to exit the program (for fatal) */
  
extern const char *progname;
/* program name */

extern struct log_output log_stderr, log_syslog, *log_default;
/* some typical outputs */

extern const char *debug_filename;
extern int debug_lineno;
extern int logdate;

/** @brief Issue a debug message if debugging is turned on
 * @param x Parenthesized debug arguments
 *
 * Use in the format: D(("format string", arg, arg, ...));
 */
#define D(x) do {				\
  if(debugging) {				\
    debug_filename=__FILE__;			\
    debug_lineno=__LINE__;			\
    debug x;					\
  }						\
} while(0)

#endif /* LOG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
