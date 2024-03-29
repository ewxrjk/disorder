/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-9, 2013 Richard Kettlewell
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
/** @file lib/log.c @brief Errors and logging
 *
 * All messages are initially emitted by one of the four functions
 * below.  disorder_debug() is generally invoked via D() so that
 * mostly you just do a test rather than a complete subroutine call.
 *
 * Messages are dispatched via @ref log_default.  This defaults to @ref
 * log_stderr.  daemonize() will turn off @ref log_stderr and use @ref
 * log_syslog instead.
 *
 * disorder_fatal() will call exitfn() with a nonzero status.  The
 * default value is exit(), but it should be set to _exit() anywhere
 * but the 'main line' of the program, to guarantee that exit() gets
 * called at most once.
 */

#define NO_MEMORY_ALLOCATION
/* because the memory allocation functions report errors */

#include "common.h"

#include <errno.h>
#include <sys/types.h>
#if HAVE_SYSLOG_H
# include <syslog.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_NETDB_H
# include <netdb.h>
#endif
#include <time.h>

#include "log.h"
#include "disorder.h"
#include "printf.h"

/** @brief Definition of a log output */
struct log_output {
  /** @brief Function to call */
  void (*fn)(int pri, const char *msg, void *user);
  /** @brief User data */
  void *user;
};

/** @brief Function to call on a fatal error
 *
 * This is normally @c exit() but in the presence of @c fork() it
 * sometimes gets set to @c _exit(). */
void (*exitfn)(int) attribute((noreturn)) = exit;

/** @brief Debug flag */
int debugging;

/** @brief Program name */
const char *progname;

/** @brief Filename for debug messages */
const char *debug_filename;

/** @brief Set to include timestamps in log messages */
int logdate;

/** @brief Line number for debug messages */
int debug_lineno;

/** @brief Pointer to chosen log output structure */
struct log_output *log_default = &log_stderr;

/** @brief Filename to debug for */
static const char *debug_only;

/** @brief Construct log line, encoding special characters
 *
 * We might be receiving things in any old encoding, or binary rubbish
 * in no encoding at all, so escape anything we don't like the look
 * of.  We limit the log message to a kilobyte.
 */
static void format(char buffer[], size_t bufsize, const char *fmt, va_list ap) {
  char t[1024];
  const char *p;
  int ch;
  size_t n = 0;
  
  if(byte_vsnprintf(t, sizeof t, fmt, ap) < 0) {
    strcpy(t, "[byte_vsnprintf failed: ");
    strncat(t, fmt, sizeof t - strlen(t) - 1);
  }
  p = t;
  while((ch = (unsigned char)*p++)) {
    if(ch >= ' ' && ch <= 126) {
      if(n < bufsize) buffer[n++] = ch;
    } else {
      if(n < bufsize) buffer[n++] = '\\';
      if(n < bufsize) buffer[n++] = '0' + ((ch >> 6) & 7);
      if(n < bufsize) buffer[n++] = '0' + ((ch >> 3) & 7);
      if(n < bufsize) buffer[n++] = '0' + ((ch >> 0) & 7);
    }
  }
  if(n >= bufsize)
    n = bufsize - 1;
  buffer[n] = 0;
}

/** @brief Log to a file
 * @param pri Message priority (as per syslog)
 * @param msg Messagge to log
 * @param user The @c FILE @c * to log to or NULL for @c stderr
 */
static void logfp(int pri, const char *msg, void *user) {
  struct timeval tv;
  FILE *fp = user ? user : stderr;
  /* ...because stderr is not a constant so we can't initialize log_stderr
   * sanely */
  const char *p;
  
  if(logdate) {
    char timebuf[64];
    struct tm *tm;
    time_t t;
    gettimeofday(&tv, 0);
    t = tv.tv_sec;
    tm = localtime(&t);
    strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S %Z", tm);
    fprintf(fp, "%s: ", timebuf);
  } 
 if(progname)
    fprintf(fp, "%s: ", progname);
  if(pri <= LOG_ERR)
    fputs("ERROR: ", fp);
  else if(pri < LOG_DEBUG)
    fputs("INFO: ", fp);
  else {
    if(!debug_only) {
      if(!(debug_only = getenv("DISORDER_DEBUG_ONLY")))
	debug_only = "";
    }
    gettimeofday(&tv, 0);
    p = debug_filename;
    while(!strncmp(p, "../", 3)) p += 3;
    if(*debug_only && strcmp(p, debug_only))
      return;
    fprintf(fp, "%llu.%06lu: %s:%d: ",
	    (unsigned long long)tv.tv_sec, (unsigned long)tv.tv_usec,
	    p, debug_lineno);
  }
  fputs(msg, fp);
  fputc('\n', fp);
  fflush(fp);
}

#if HAVE_SYSLOG_H
/** @brief Log to syslog */
static void logsyslog(int pri, const char *msg,
		      void attribute((unused)) *user) {
  if(pri < LOG_DEBUG)
    syslog(pri, "%s", msg);
  else
    syslog(pri, "%s:%d: %s", debug_filename, debug_lineno, msg);
}
#endif

/** @brief Log output that writes to @c stderr */
struct log_output log_stderr = { logfp, 0 };

#if HAVE_SYSLOG_H
/** @brief Log output that sends to syslog */
struct log_output log_syslog = { logsyslog, 0 };
#endif

/** @brief Format and log a message */
static void vlogger(int pri, const char *fmt, va_list ap) {
  char buffer[1024];

  format(buffer, sizeof buffer, fmt, ap);
  log_default->fn(pri, buffer, log_default->user);
}

/** @brief Format and log a message */
static void logger(int pri, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vlogger(pri, fmt, ap);
  va_end(ap);
}

/** @brief Format and log a message
 * @param pri Message priority (as per syslog)
 * @param ec Error class
 * @param fmt Format string
 * @param errno_value Errno value to include as a string, or 0
 * @param ap Argument list
 */
void elog(int pri, enum error_class ec, int errno_value, const char *fmt, va_list ap) {
  char buffer[1024];
  char errbuf[1024];

  if(errno_value == 0)
    vlogger(pri, fmt, ap);
  else {
    memset(buffer, 0, sizeof buffer);
    byte_vsnprintf(buffer, sizeof buffer, fmt, ap);
    buffer[sizeof buffer - 1] = 0;
    logger(pri, "%s: %s", buffer,
           format_error(ec, errno_value, errbuf, sizeof errbuf));
  }
}

/** @brief Log an error and quit
 *
 * If @c ${DISORDER_FATAL_ABORT} is defined (as anything) then the process
 * is aborted, so you can get a backtrace.
 */
void disorder_fatal(int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_CRIT, ec_errno, errno_value, msg, ap);
  va_end(ap);
  if(getenv("DISORDER_FATAL_ABORT")) abort();
  exitfn(EXIT_FAILURE);
}

/** @brief Log an error and quit
 *
 * If @c ${DISORDER_FATAL_ABORT} is defined (as anything) then the process
 * is aborted, so you can get a backtrace.
 */
void disorder_fatal_ec(enum error_class ec, int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_CRIT, ec, errno_value, msg, ap);
  va_end(ap);
  if(getenv("DISORDER_FATAL_ABORT")) abort();
  exitfn(EXIT_FAILURE);
}

/** @brief Log an error */
void disorder_error(int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_ERR, ec_errno, errno_value, msg, ap);
  va_end(ap);
}

/** @brief Log an error */
void disorder_error_ec(enum error_class ec, int errno_value, const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_ERR, ec, errno_value, msg, ap);
  va_end(ap);
}

/** @brief Log an informational message */
void disorder_info(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  elog(LOG_INFO, ec_none, 0, msg, ap);
  va_end(ap);
}

/** @brief Log a debug message */
void disorder_debug(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  vlogger(LOG_DEBUG, msg, ap);
  va_end(ap);
}

/** @brief Set the program name from @c argc */
void set_progname(char **argv) {
  if((progname = strrchr(argv[0], '/')))
    ++progname;
  else
    progname = argv[0];
}

/** @brief Format an error string
 * @param ec Error class
 * @param err Error code (interpretation defined by @p ec)
 * @param buffer Output buffer
 * @param bufsize Size of output buffer
 * @return Pointer to error string
 *
 * The return value may or may not be @p buffer.
 */
#if !_WIN32
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
const char *format_error(enum error_class ec, int err, char buffer[], size_t bufsize) {
#if _WIN32
  size_t n;
  switch(ec) {
  default:
    if(!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       err,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buffer,
                       bufsize,
                       NULL))
      disorder_fatal(0, "FormatMessage failed");
    n = strlen(buffer);
    while(n > 0 && isspace((unsigned char)buffer[n-1]))
      --n;
    buffer[n] = 0;
    return buffer;
  case ec_errno:
    strerror_s(buffer, bufsize, err);
    return buffer;
  case ec_none:
    return "(none)";
  }
#else
  switch(ec) {
  default:
    return strerror(err);
  case ec_getaddrinfo:
    return gai_strerror(err);
  case ec_none:
    return "(none)";
  }
#endif
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
