/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#define NO_MEMORY_ALLOCATION
/* because the memory allocation functions report errors */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>

#include "log.h"
#include "disorder.h"
#include "printf.h"

struct log_output {
  void (*fn)(int pri, const char *msg, void *user);
  void *user;
};

void (*exitfn)(int) attribute((noreturn)) = exit;
int debugging;
const char *progname;
const char *debug_filename;
int debug_lineno;
struct log_output *log_default = &log_stderr;

static const char *debug_only;

/* we might be receiving things in any old encoding, or binary rubbish in no
 * encoding at all, so escape anything we don't like the look of */
static void format(char buffer[], size_t bufsize, const char *fmt, va_list ap) {
  char t[1024];
  const char *p;
  int ch;
  size_t n = 0;
  
  if(byte_vsnprintf(t, sizeof t, fmt, ap) < 0)
    strcpy(t, "[byte_vsnprintf failed]");
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

/* log to a file */
static void logfp(int pri, const char *msg, void *user) {
  struct timeval tv;
  FILE *fp = user ? user : stderr;
  /* ...because stderr is not a constant so we can't initialize log_stderr
   * sanely */
  const char *p;
  
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
}

/* log to syslog */
static void logsyslog(int pri, const char *msg,
		      void attribute((unused)) *user) {
  if(pri < LOG_DEBUG)
    syslog(pri, "%s", msg);
  else
    syslog(pri, "%s:%d: %s", debug_filename, debug_lineno, msg);
}

struct log_output log_stderr = { logfp, 0 };
struct log_output log_syslog = { logsyslog, 0 };

/* log to all log outputs */
static void vlogger(int pri, const char *fmt, va_list ap) {
  char buffer[1024];

  format(buffer, sizeof buffer, fmt, ap);
  log_default->fn(pri, buffer, log_default->user);
}

/* wrapper for vlogger */
static void logger(int pri, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vlogger(pri, fmt, ap);
  va_end(ap);
}

/* internals of fatal/error/info */
void elog(int pri, int errno_value, const char *fmt, va_list ap) {
  char buffer[1024];

  if(errno_value == 0)
    vlogger(pri, fmt, ap);
  else {
    memset(buffer, 0, sizeof buffer);
    byte_vsnprintf(buffer, sizeof buffer, fmt, ap);
    buffer[sizeof buffer - 1] = 0;
    logger(pri, "%s: %s", buffer, strerror(errno_value));
  }
}

#define disorder_fatal fatal
#define disorder_error error
#define disorder_info info

/* shared implementation of vararg functions */
#include "log-impl.h"

void debug(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  vlogger(LOG_DEBUG, msg, ap);
  va_end(ap);
}

void set_progname(char **argv) {
  if((progname = strrchr(argv[0], '/')))
    ++progname;
  else
    progname = argv[0];
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:78385d5240eab4439cb7eca7dad5154d */
