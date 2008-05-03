/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file lib/macros-builtin.c
 * @brief Built-in expansions
 *
 * This is a grab-bag of non-domain-specific expansions.  Note that
 * documentation will be generated from the comments at the head of
 * each function.
 */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "macros.h"
#include "sink.h"
#include "syscalls.h"
#include "log.h"
#include "wstat.h"
#include "kvp.h"

/** @brief Return 1 if @p s is 'true' else 0 */
int mx_str2bool(const char *s) {
  return !strcmp(s, "true");
}

/** @brief Return "true" if @p n is nonzero else "false" */
const char *mx_bool2str(int n) {
  return n ? "true" : "false";
}

/* @include{TEMPLATE}@
 *
 * Includes TEMPLATE as if its text were substituted for the @include
 * expansion.   TODO
 */
static int exp_include(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       struct sink attribute((unused)) *output,
		       void attribute((unused)) *u) {
  assert(!"TODO implement search path");
}

/* @include{COMMAND}
 *
 * Executes COMMAND via the shell (using "sh -c") and copies its
 * standard output to the template output.  The shell command output
 * is not expanded or modified in any other way.
 *
 * The shell command's standard error is copied to the error log.
 *
 * If the shell exits nonzero then this is reported to the error log
 * but otherwise no special action is taken.
 */
static int exp_shell(int attribute((unused)) nargs,
		     char **args,
		     struct sink *output,
		     void attribute((unused)) *u) {
  int w, p[2], n;
  char buffer[4096];
  pid_t pid;
  
  xpipe(p);
  if(!(pid = xfork())) {
    exitfn = _exit;
    xclose(p[0]);
    xdup2(p[1], 1);
    xclose(p[1]);
    execlp("sh", "sh", "-c", args[0], (char *)0);
    fatal(errno, "error executing sh");
  }
  xclose(p[1]);
  while((n = read(p[0], buffer, sizeof buffer))) {
    if(n < 0) {
      if(errno == EINTR)
	continue;
      else
	fatal(errno, "error reading from pipe");
    }
    if(output->write(output, buffer, n) < 0)
      return -1;
  }
  xclose(p[0]);
  while((n = waitpid(pid, &w, 0)) < 0 && errno == EINTR)
    ;
  if(n < 0)
    fatal(errno, "error calling waitpid");
  if(w)
    error(0, "shell command '%s' %s", args[0], wstat(w));
  return 0;
}

/* @if{CONDITION}{IF-TRUE}{IF-FALSE}
 *
 * If CONDITION is "true" then evaluates to IF-TRUE.  Otherwise
 * evaluates to IF-FALSE.  The IF-FALSE part is optional.
 */
static int exp_if(int nargs,
		  const struct mx_node **args,
		  struct sink *output,
		  void *u) {
  char *s;
  int rc;

  if((rc = mx_expandstr(args[0], &s, u)))
    return rc;
  if(mx_str2bool(s))
    return mx_expand(args[1], output, u);
  else if(nargs > 2)
    return mx_expand(args[2], output, u);
  else
    return 0;
}

/* @and{BRANCH}{BRANCH}...
 *
 * Expands to "true" if all the branches are "true" otherwise to "false".  If
 * there are no brances then the result is "true".  Only as many branches as
 * necessary to compute the answer are evaluated (starting from the first one),
 * so if later branches have side effects they may not take place.
 */
static int exp_and(int nargs,
		   const struct mx_node **args,
		   struct sink *output,
		   void *u) {
  int n, result = 1, rc;
  char *s;

  for(n = 0; n < nargs; ++n) {
    if((rc = mx_expandstr(args[n], &s, u)))
      return rc;
    if(!mx_str2bool(s)) {
      result = 0;
      break;
    }
  }
  if(sink_writes(output, mx_bool2str(result)) < 0)
    return -1;
  else
    return 0;
}

/* @or{BRANCH}{BRANCH}...
 *
 * Expands to "true" if any of the branches are "true" otherwise to "false".
 * If there are no brances then the result is "false".  Only as many branches
 * as necessary to compute the answer are evaluated (starting from the first
 * one), so if later branches have side effects they may not take place.
 */
static int exp_or(int nargs,
		  const struct mx_node **args,
		  struct sink *output,
		  void *u) {
  int n, result = 0, rc;
  char *s;

  for(n = 0; n < nargs; ++n) {
    if((rc = mx_expandstr(args[n], &s, u)))
      return rc;
    if(mx_str2bool(s)) {
      result = 1;
      break;
    }
  }
  if(sink_writes(output, mx_bool2str(result)) < 0)
    return -1;
  else
    return 0;
}

/* @not{CONDITION}
 *
 * Expands to "true" unless CONDITION is "true" in which case "false".
 */
static int exp_not(int attribute((unused)) nargs,
		   char **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  if(sink_writes(output, mx_bool2str(!mx_str2bool(args[0]))) < 0)
    return -1;
  else
    return 0;
}

/* @#{...}
 *
 * Expands to nothing.  The argument(s) are not fully evaluated, and no side
 * effects occur.
 */
static int exp_comment(int attribute((unused)) nargs,
                       const struct mx_node attribute((unused)) **args,
                       struct sink attribute((unused)) *output,
                       void attribute((unused)) *u) {
  return 0;
}

/* @urlquote{STRING}
 *
 * URL-quotes a string, i.e. replaces any characters not safe to use unquoted
 * in a URL with %-encoded form.
 */
static int exp_urlquote(int attribute((unused)) nargs,
                        char **args,
                        struct sink *output,
                        void attribute((unused)) *u) {
  if(sink_writes(output, urlencodestring(args[0])) < 0)
    return -1;
  else
    return 0;
}

void mx_register_builtin(void) {
  mx_register("include", 1, 1, exp_include);
  mx_register("shell", 1, 1, exp_shell);
  mx_magic_register("if", 2, 3, exp_if);
  mx_magic_register("and", 0, INT_MAX, exp_and);
  mx_magic_register("or", 0, INT_MAX, exp_or);
  mx_register("not", 1, 1, exp_not);
  mx_magic_register("#", 0, INT_MAX, exp_comment);
  mx_register("urlquote", 1, 1, exp_urlquote);
  /* TODO: eq */
  /* TODO: ne */
  /* TODO: define */
  /* TODO: discard */
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
