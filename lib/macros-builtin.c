/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file lib/macros-builtin.c
 * @brief Built-in expansions
 *
 * This is a grab-bag of non-domain-specific expansions
 *
 * Documentation is generated from the comments at the head of each function.
 * The comment should have a '$' and the expansion name on the first line and
 * should have a blank line between each paragraph.
 *
 * To make a bulleted list, put a '-' at the start of each line.
 *
 * You can currently get away with troff markup but this is horribly ugly and
 * might be changed.
 */

#include "common.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "log.h"
#include "hash.h"
#include "mem.h"
#include "macros.h"
#include "sink.h"
#include "syscalls.h"
#include "wstat.h"
#include "kvp.h"
#include "split.h"
#include "printf.h"
#include "vector.h"
#include "filepart.h"

static struct vector include_path;

/** @brief Return 1 if @p s is 'true' else 0 */
int mx_str2bool(const char *s) {
  return !strcmp(s, "true");
}

/** @brief Return "true" if @p n is nonzero else "false" */
const char *mx_bool2str(int n) {
  return n ? "true" : "false";
}

/** @brief Write a boolean result */
int mx_bool_result(struct sink *output, int result) {
  if(sink_writes(output, mx_bool2str(result)) < 0)
    return -1;
  else
    return 0;
}

/** @brief Search the include path */
char *mx_find(const char *name, int report) {
  char *path;
  int n;
  
  if(name[0] == '/') {
    if(access(name, O_RDONLY) < 0) {
      if(report)
        disorder_error(errno, "cannot read %s", name);
      return 0;
    }
    path = xstrdup(name);
  } else {
    /* Search the include path */
    for(n = 0; n < include_path.nvec; ++n) {
      byte_xasprintf(&path, "%s/%s", include_path.vec[n], name);
      if(access(path, O_RDONLY) == 0)
        break;
    }
    if(n >= include_path.nvec) {
      if(report)
        disorder_error(0, "cannot find '%s' in search path", name);
      return 0;
    }
  }
  return path;
}

/*$ @include{TEMPLATE}
 *
 * Includes TEMPLATE.
 *
 * TEMPLATE can be an absolute filename starting with a '/'; only the file with
 * exactly this name will be included.
 *
 * Alternatively it can be a relative filename, not starting with a '/'.  In
 * this case the file will be searched for in the include path.  When searching
 * paths, unreadable files are treated as if they do not exist (rather than
 * matching then producing an error).
 *
 * If the name chosen ends ".tmpl" then the file will be expanded as a
 * template.  Anything else is included byte-for-byte without further
 * modification.
 *
 * Only regular files are allowed (no devices, sockets or name pipes).
 */
static int exp_include(int attribute((unused)) nargs,
		       char **args,
		       struct sink *output,
		       void *u) {
  const char *path;
  int fd, n;
  char buffer[4096];
  struct stat sb;

  if(!(path = mx_find(args[0], 1/*report*/))) {
    if(sink_printf(output, "[[cannot find '%s']]", args[0]) < 0)
      return 0;
    return 0;
  }
  /* If it's a template expand it */
  if(strlen(path) >= 5 && !strncmp(path + strlen(path) - 5, ".tmpl", 5))
    return mx_expand_file(path, output, u);
  /* Read the raw file.  As with mx_expand_file() we insist that the file is a
   * regular file. */
  if((fd = open(path, O_RDONLY)) < 0)
    disorder_fatal(errno, "error opening %s", path);
  if(fstat(fd, &sb) < 0)
    disorder_fatal(errno, "error statting %s", path);
  if(!S_ISREG(sb.st_mode))
    disorder_fatal(0, "%s: not a regular file", path);
  while((n = read(fd, buffer, sizeof buffer)) > 0) {
    if(sink_write(output, buffer, n) < 0) {
      xclose(fd);
      return -1;
    }
  }
  if(n < 0)
    disorder_fatal(errno, "error reading %s", path);
  xclose(fd);
  return 0;
}

/*$ @include{COMMAND}
 *
 * Executes COMMAND via the shell (using "sh \-c") and copies its
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
    disorder_fatal(errno, "error executing sh");
  }
  xclose(p[1]);
  while((n = read(p[0], buffer, sizeof buffer))) {
    if(n < 0) {
      if(errno == EINTR)
	continue;
      else
	disorder_fatal(errno, "error reading from pipe");
    }
    if(output->write(output, buffer, n) < 0)
      return -1;
  }
  xclose(p[0]);
  while((n = waitpid(pid, &w, 0)) < 0 && errno == EINTR)
    ;
  if(n < 0)
    disorder_fatal(errno, "error calling waitpid");
  if(w)
    disorder_error(0, "shell command '%s' %s", args[0], wstat(w));
  return 0;
}

/*$ @if{CONDITION}{IF-TRUE}{IF-FALSE}
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

  if((rc = mx_expandstr(args[0], &s, u, "argument #0 (CONDITION)")))
    return rc;
  if(mx_str2bool(s))
    return mx_expand(args[1], output, u);
  else if(nargs > 2)
    return mx_expand(args[2], output, u);
  else
    return 0;
}

/*$ @and{BRANCH}{BRANCH}...
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
  char *s, *argname;

  for(n = 0; n < nargs; ++n) {
    byte_xasprintf(&argname, "argument #%d", n);
    if((rc = mx_expandstr(args[n], &s, u, argname)))
      return rc;
    if(!mx_str2bool(s)) {
      result = 0;
      break;
    }
  }
  return mx_bool_result(output, result);
}

/*$ @or{BRANCH}{BRANCH}...
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
  char *s, *argname;

  for(n = 0; n < nargs; ++n) {
    byte_xasprintf(&argname, "argument #%d", n);
    if((rc = mx_expandstr(args[n], &s, u, argname)))
      return rc;
    if(mx_str2bool(s)) {
      result = 1;
      break;
    }
  }
  return mx_bool_result(output, result);
}

/*$ @not{CONDITION}
 *
 * Expands to "true" unless CONDITION is "true" in which case "false".
 */
static int exp_not(int attribute((unused)) nargs,
		   char **args,
		   struct sink *output,
		   void attribute((unused)) *u) {
  return mx_bool_result(output, !mx_str2bool(args[0]));
}

/*$ @#{...}
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

/*$ @urlquote{STRING}
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

/*$ @eq{S1}{S2}...
 *
 * Expands to "true" if all the arguments are identical, otherwise to "false"
 * (i.e. if any pair of arguments differs).
 *
 * If there are no arguments then expands to "true".  Evaluates all arguments
 * (with their side effects) even if that's not strictly necessary to discover
 * the result.
 */
static int exp_eq(int nargs,
		  char **args,
		  struct sink *output,
		  void attribute((unused)) *u) {
  int n, result = 1;
  
  for(n = 1; n < nargs; ++n) {
    if(strcmp(args[n], args[0])) {
      result = 0;
      break;
    }
  }
  return mx_bool_result(output, result);
}

/*$ @ne{S1}{S2}...
 *
 * Expands to "true" if all of the arguments differ from one another, otherwise
 * to "false" (i.e. if any value appears more than once).
 *
 * If there are no arguments then expands to "true".  Evaluates all arguments
 * (with their side effects) even if that's not strictly necessary to discover
 * the result.
 */
static int exp_ne(int nargs,
		  char **args,
		  struct sink *output,
		  void  attribute((unused))*u) {
  hash *h = hash_new(sizeof (char *));
  int n, result = 1;

  for(n = 0; n < nargs; ++n)
    if(hash_add(h, args[n], "", HASH_INSERT)) {
      result = 0;
      break;
    }
  return mx_bool_result(output, result);
}

/*$ @discard{...}
 *
 * Expands to nothing.  Unlike the comment expansion @#{...}, side effects of
 * arguments are not suppressed.  So this can be used to surround a collection
 * of macro definitions with whitespace, free text commentary, etc.
 */
static int exp_discard(int attribute((unused)) nargs,
                       char attribute((unused)) **args,
                       struct sink attribute((unused)) *output,
                       void attribute((unused)) *u) {
  return 0;
}

/*$ @define{NAME}{ARG1 ARG2...}{DEFINITION}
 *
 * Define a macro.  The macro will be called NAME and will act like an
 * expansion.  When it is expanded, the expansion is replaced by DEFINITION,
 * with each occurence of @ARG1@ etc replaced by the parameters to the
 * expansion.
 */
static int exp_define(int attribute((unused)) nargs,
                      const struct mx_node **args,
                      struct sink attribute((unused)) *output,
                      void attribute((unused)) *u) {
  char **as, *name, *argnames;
  int rc, nas;
  
  if((rc = mx_expandstr(args[0], &name, u, "argument #0 (NAME)")))
    return rc;
  if((rc = mx_expandstr(args[1], &argnames, u, "argument #1 (ARGS)")))
    return rc;
  as = split(argnames, &nas, 0, 0, 0);
  mx_register_macro(name, nas, as, args[2]);
  return 0;
}

/*$ @basename{PATH}
 *
 * Expands to the UNQUOTED basename of PATH.
 */
static int exp_basename(int attribute((unused)) nargs,
                        char **args,
                        struct sink attribute((unused)) *output,
                        void attribute((unused)) *u) {
  return sink_writes(output, d_basename(args[0])) < 0 ? -1 : 0;
}

/*$ @dirname{PATH}
 *
 * Expands to the UNQUOTED directory name of PATH.
 */
static int exp_dirname(int attribute((unused)) nargs,
                       char **args,
                       struct sink attribute((unused)) *output,
                       void attribute((unused)) *u) {
  return sink_writes(output, d_dirname(args[0])) < 0 ? -1 : 0;
}

/*$ @q{STRING}
 *
 * Expands to STRING.
 */
static int exp_q(int attribute((unused)) nargs,
                 char **args,
                 struct sink attribute((unused)) *output,
                 void attribute((unused)) *u) {
  return sink_writes(output, args[0]) < 0 ? -1 : 0;
}

/** @brief Register built-in expansions */
void mx_register_builtin(void) {
  mx_register("basename", 1, 1, exp_basename);
  mx_register("dirname", 1, 1, exp_dirname);
  mx_register("discard", 0, INT_MAX, exp_discard);
  mx_register("eq", 0, INT_MAX, exp_eq);
  mx_register("include", 1, 1, exp_include);
  mx_register("ne", 0, INT_MAX, exp_ne);
  mx_register("not", 1, 1, exp_not);
  mx_register("shell", 1, 1, exp_shell);
  mx_register("urlquote", 1, 1, exp_urlquote);
  mx_register("q", 1, 1, exp_q);
  mx_register_magic("#", 0, INT_MAX, exp_comment);
  mx_register_magic("and", 0, INT_MAX, exp_and);
  mx_register_magic("define", 3, 3, exp_define);
  mx_register_magic("if", 2, 3, exp_if);
  mx_register_magic("or", 0, INT_MAX, exp_or);
}

/** @brief Add a directory to the search path
 * @param s Directory to add
 */
void mx_search_path(const char *s) {
  vector_append(&include_path, xstrdup(s));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
