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

/** @file lib/macros.c
 * @brief Macro expansion
 */

#include <config.h>
#include "types.h"

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "macros.h"
#include "mem.h"
#include "vector.h"
#include "log.h"
#include "hash.h"
#include "sink.h"
#include "syscalls.h"

VECTOR_TYPE(mx_node_vector, const struct mx_node *, xrealloc);

/** @brief Definition of an expansion */
struct expansion {
  /** @brief Minimum permitted arguments */
  int min;

  /** @brief Maximum permitted arguments */
  int max;

  /** @brief Flags */
  unsigned flags;

  /** @brief Callback (cast to appropriate type) */
  void (*callback)();
};

/** @brief Expansion takes parsed templates, not strings */
#define EXP_MAGIC 0x0001

static hash *expansions;

/* Parsing ------------------------------------------------------------------ */

/** @brief Parse a template
 * @param filename Input filename (for diagnostics)
 * @param line Line number (use 1 on initial call)
 * @param input Start of text to parse
 * @param end End of text to parse or NULL
 * @return Pointer to parse tree root node
 *
 * Parses the text in [start, end) and returns an (immutable) parse
 * tree representing it.
 *
 * If @p end is NULL then the whole string is parsed.
 *
 * Note that the @p filename value stored in the parse tree is @p filename,
 * i.e. it is not copied.
 */
const struct mx_node *mx_parse(const char *filename,
			       int line,
			       const char *input,
			       const char *end) {
  int braces, expansion_start_line, argument_start_line;
  const char *argument_start, *argument_end, *p;
  struct mx_node_vector v[1];
  struct dynstr d[1];
  struct mx_node *head = 0, **tailp = &head, *e;
  int omitted_terminator;

  if(!end)
    end = input + strlen(input);
  while(input < end) {
    if(*input != '@') {
      expansion_start_line = line;
      dynstr_init(d);
      /* Gather up text without any expansions in. */
      while(input < end && *input != '@') {
	if(*input == '\n')
	  ++line;
	dynstr_append(d, *input++);
      }
      dynstr_terminate(d);
      e = xmalloc(sizeof *e);
      e->next = 0;
      e->filename = filename;
      e->line = expansion_start_line;
      e->type = MX_TEXT;
      e->text = d->vec;
      *tailp = e;
      tailp = &e->next;
      continue;
    }
    mx_node_vector_init(v);
    braces = 0;
    p = input;
    ++input;
    expansion_start_line = line;
    omitted_terminator = 0;
    while(!omitted_terminator && input < end && *input != '@') {
      /* Skip whitespace */
      if(isspace((unsigned char)*input)) {
	if(*input == '\n')
	  ++line;
	++input;
	continue;
      }
      if(*input == '{') {
	/* This is a bracketed argument.  We'll walk over it counting
	 * braces to figure out where the end is. */
	++input;
	argument_start = input;
	argument_start_line = line;
	while(input < end && (*input != '}' || braces > 0)) {
	  switch(*input++) {
	  case '{': ++braces; break;
	  case '}': --braces; break;
	  case '\n': ++line; break;
	  }
	}
        /* If we run out of input without seeing a '}' that's an error */
	if(input >= end)
	  fatal(0, "%s:%d: unterminated expansion '%.*s'",
		filename, argument_start_line,
		(int)(input - argument_start), argument_start);
        /* Consistency check */
	assert(*input == '}');
        /* Record the end of the argument */
        argument_end = input;
        /* Step over the '}' */
	++input;
	if(input < end && isspace((unsigned char)*input)) {
	  /* There is at least some whitespace after the '}'.  Look
	   * ahead and see what is after all the whitespace. */
	  for(p = input; p < end && isspace((unsigned char)*p); ++p)
	    ;
	  /* Now we are looking after the whitespace.  If it's
	   * anything other than '{', including the end of the input,
	   * then we infer that this expansion finished at the '}' we
	   * just saw.  (NB that we don't move input forward to p -
	   * the whitespace is NOT part of the expansion.) */
	  if(p == end || *p != '{')
	    omitted_terminator = 1;
	}
      } else {
	/* We are looking at an unbracketed argument.  (A common example would
	 * be the expansion or macro name.)  This is terminated by an '@'
	 * (indicating the end of the expansion), a ':' (allowing a subsequent
	 * unbracketed argument) or a '{' (allowing a bracketed argument).  The
	 * end of the input will also do. */
	argument_start = input;
	argument_start_line = line;
	while(input < end
	      && *input != '@' && *input != '{' && *input != ':') {
	  if(*input == '\n') ++line;
	  ++input;
	}
        argument_end = input;
        /* Trailing whitespace is not significant in unquoted arguments (and
         * leading whitespace is eliminated by the whitespace skip above). */
        while(argument_end > argument_start
              && isspace((unsigned char)argument_end[-1]))
          --argument_end;
        /* Step over the ':' if that's what we see */
	if(input < end && *input == ':')
	  ++input;
      }
      /* Now we have an argument in [argument_start, argument_end), and we know
       * its filename and initial line number.  This is sufficient to parse
       * it. */
      mx_node_vector_append(v, mx_parse(filename, argument_start_line,
                                        argument_start, argument_end));
    }
    /* We're at the end of an expansion.  We might have hit the end of the
     * input, we might have hit an '@' or we might have matched the
     * omitted_terminator criteria. */
    if(input < end) {
      if(!omitted_terminator) {
        assert(*input == '@');
        ++input;
      }
    }
    /* @@ terminates this file */
    if(v->nvec == 0)
      break;
    /* Currently we require that the first element, the expansion name, is
     * always plain text.  Removing this restriction would raise some
     * interesting possibilities but for the time being it is considered an
     * error. */
    if(v->vec[0]->type != MX_TEXT)
      fatal(0, "%s:%d: expansion names may not themselves contain expansions",
            v->vec[0]->filename, v->vec[0]->line);
    /* Guarantee a NULL terminator (for the case where there's more than one
     * argument) */
    mx_node_vector_terminate(v);
    e = xmalloc(sizeof *e);
    e->next = 0;
    e->filename = filename;
    e->line = expansion_start_line;
    e->type = MX_EXPANSION;
    e->name = v->vec[0]->text;
    e->nargs = v->nvec - 1;
    e->args = v->nvec > 1 ? &v->vec[1] : 0;
    *tailp = e;
    tailp = &e->next;
  }
  return head;
}

static void mx__dump(struct dynstr *d, const struct mx_node *m) {
  int n;
  
  if(!m)
    return;
  switch(m->type) {
  case MX_TEXT:
    dynstr_append_string(d, m->text);
    break;
  case MX_EXPANSION:
    dynstr_append(d, '@');
    dynstr_append_string(d, m->name);
    for(n = 0; n < m->nargs; ++n) {
      dynstr_append(d, '{');
      mx__dump(d, m->args[n]);
      dynstr_append(d, '}');
    }
    dynstr_append(d, '@');
    break;
  default:
    assert(!"invalid m->type");
  }
  mx__dump(d, m->next);
}

/** @brief Dump a parse macro expansion to a string */
char *mx_dump(const struct mx_node *m) {
  struct dynstr d[1];

  dynstr_init(d);
  mx__dump(d, m);
  dynstr_terminate(d);
  return d->vec;
}

/* Expansion registration --------------------------------------------------- */

static void mx__register(unsigned flags,
                         const char *name,
                         int min,
                         int max,
                         void (*callback)()) {
  struct expansion e[1];

  if(!expansions)
    expansions = hash_new(sizeof(struct expansion));
  e->min = min;
  e->max = max;
  e->flags = flags;
  e->callback = callback;
  hash_add(expansions, name, &e, HASH_INSERT_OR_REPLACE);
}

/** @brief Register a simple expansion rule
 * @param name Name
 * @param min Minimum number of arguments
 * @param max Maximum number of arguments
 * @param callback Callback to write output
 */
void mx_register(const char *name,
                 int min,
                 int max,
                 mx_simple_callback *callback) {
  mx__register(0,  name, min, max, (void (*)())callback);
}

/** @brief Register a magic expansion rule
 * @param name Name
 * @param min Minimum number of arguments
 * @param max Maximum number of arguments
 * @param callback Callback to write output
 */
void mx_magic_register(const char *name,
                       int min,
                       int max,
                       mx_magic_callback *callback) {
  mx__register(EXP_MAGIC, name, min, max, (void (*)())callback);
}

/* Expansion ---------------------------------------------------------------- */

/** @brief Expand a template
 * @param m Where to start
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-0 on error∑
 *
 * If a sink write fails then -1 is returned.  If any callback returns non-zero
 * then that value is returned.  It is suggested that callbacks adopt this
 * policy too and use positive values to mean other kinds of fatal error.
 */
int mx_expand(const struct mx_node *m,
              struct sink *output,
              void *u) {
  const struct expansion *e;
  int rc;

  if(!m)
    return 0;
  switch(m->type) {
  case MX_TEXT:
    if(sink_writes(output, m->text) < 0)
      return -1;
    break;
  case MX_EXPANSION:
    if(!(e = hash_find(expansions, m->name))) {
      error(0, "%s:%d: unknown expansion name '%s'",
            m->filename, m->line, m->name);
      if(sink_printf(output, "[[%s unknown]]", m->name) < 0)
        return -1;
    } else if(m->nargs < e->min) {
      error(0, "%s:%d: expansion '%s' requires %d args, only %d given",
            m->filename, m->line, m->name, e->min, m->nargs);
      if(sink_printf(output, "[[%s too few args]]", m->name) < 0)
        return -1;
    } else if(m->nargs > e->max) {
      error(0, "%s:%d: expansion '%s' takes at most %d args, but %d given",
            m->filename, m->line, m->name, e->max, m->nargs);
      if(sink_printf(output, "[[%s too many args]]", m->name) < 0)
        return -1;
    } else if(e->flags & EXP_MAGIC) {
      /* Magic callbacks we can call directly */
      if((rc = ((mx_magic_callback *)e->callback)(m->nargs,
                                                  m->args,
                                                  output,
                                                  u)))
        return rc;
    } else {
      /* For simple callbacks we expand their arguments for them */
      char **args = xcalloc(1 + m->nargs, sizeof (char *));
      int n;

      for(n = 0; n < m->nargs; ++n)
        if((rc = mx_expandstr(m->args[n], &args[n], u)))
          return rc;
      args[n] = NULL;
      if((rc = ((mx_simple_callback *)e->callback)(m->nargs,
                                                   args,
                                                   output,
                                                   u)))
        return rc;
    }
    break;
  default:
    assert(!"invalid m->type");
  }
  return mx_expand(m, output, u);
}

/** @brief Expand a template storing the result in a string
 * @param m Where to start
 * @param sp Where to store string
 * @param u User data
 * @return 0 on success, non-0 on error
 *
 * Same return conventions as mx_expand().  This wrapper is slightly more
 * convenient to use from 'magic' expansions.
 */
int mx_expandstr(const struct mx_node *m,
                 char **sp,
                 void *u) {
  struct dynstr d[1];
  int rc;

  if(!(rc = mx_expand(m, sink_dynstr(d), u))) {
    dynstr_terminate(d);
    *sp = d->vec;
  }
  return rc;
}

/** @brief Expand a template file
 * @param path Filename
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-0 on error
 *
 * Same return conventions as mx_expand().
 */
int mx_expand_file(const char *path,
                   struct sink *output,
                   void *u) {
  int fd, n;
  struct stat sb;
  char *b;
  off_t sofar;

  if((fd = open(path, O_RDONLY)) < 0)
    fatal(errno, "error opening %s", path);
  if(fstat(fd, &sb) < 0)
    fatal(errno, "error statting %s", path);
  if(!S_ISREG(sb.st_mode))
    fatal(0, "%s: not a regular file", path);
  sofar = 0;
  b = xmalloc_noptr(sb.st_size);
  while(sofar < sb.st_size) {
    n = read(fd, b + sofar, sb.st_size - sofar);
    if(n > 0)
      sofar += n;
    else if(n == 0)
      fatal(0, "unexpected EOF reading %s", path);
    else if(errno != EINTR)
      fatal(errno, "error reading %s", path);
  }
  xclose(fd);
  return mx_expand(mx_parse(path, 1, b, b + sb.st_size),
                   output,
                   u);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
