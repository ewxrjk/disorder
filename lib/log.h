/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef LOG_H
#define LOG_H

/* All messages are initially emitted by one of the four functions below.
 * debug() is generally invoked via D() so that mostly you just do a test
 * rather than a complete subroutine call.
 *
 * Messages are dispatched via log_default.  This defaults to log_stderr.
 * daemonize() will turn off log_stderr and use log_syslog instead.
 *
 * fatal() will call exitfn() with a nonzero status.  The default value is
 * exit(), but it should be set to _exit() anywhere but the 'main line' of the
 * program, to guarantee that exit() gets called at most once.
 */

#include <stdarg.h>

struct log_output;

void set_progname(char **argv);
/* set progname from argv[0] */

void elog(int pri, int errno_value, const char *fmt, va_list ap);
/* internals of fatal/error/info/debug */

void fatal(int errno_value, const char *msg, ...) attribute((noreturn))
  attribute((format (printf, 2, 3)));
void error(int errno_value, const char *msg, ...)
  attribute((format (printf, 2, 3)));
void info(const char *msg, ...)
  attribute((format (printf, 1, 2)));
void debug(const char *msg, ...)
  attribute((format (printf, 1, 2)));
/* report a message of the given class.  @errno_value@ if present an
 * non-zero is included.  @fatal@ terminates the process. */

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
/* arch-tag:6350679c7069ec3b2709aa51004a804a */
