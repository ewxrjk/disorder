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

#include "hash.h"
#include "macros.h"
#include "mem.h"
#include "vector.h"
#include "log.h"
#include "sink.h"
#include "syscalls.h"
#include "printf.h"

VECTOR_TYPE(mx_node_vector, const struct mx_node *, xrealloc);

/** @brief Definition of an expansion */
struct expansion {
  /** @brief Minimum permitted arguments */
  int min;

  /** @brief Maximum permitted arguments */
  int max;

  /** @brief Flags
   *
   * See:
   * - @ref EXP_SIMPLE
   * - @ref EXP_MAGIC
   * - @ref EXP_MACRO
   * - @ref EXP_TYPE_MASK
   */
  unsigned flags;

  /** @brief Macro argument names */
  char **args;

  /** @brief Callback (cast to appropriate type)
   *
   * Cast to @ref mx_simple_callback or @ref mx_magic_callback as required. */
  void (*callback)();

  /** @brief Macro definition
   *
   * Only for @ref EXP_MACRO expansions. */
  const struct mx_node *definition;
};

/** @brief Expansion takes pre-expanded strings
 *
 * @p callback is cast to @ref mx_simple_callback. */
#define EXP_SIMPLE 0x0000

/** @brief Expansion takes parsed templates, not strings
 *
 * @p callback is cast to @ref mx_magic_callback.  The callback must do its own
 * expansion e.g. via mx_expandstr() where necessary. */
#define EXP_MAGIC 0x0001

/** @brief Expansion is a macro */
#define EXP_MACRO 0x0002

/** @brief Mask of types */
#define EXP_TYPE_MASK 0x0003

/** @brief Hash of all expansions
 *
 * Created by mx_register(), mx_register_macro() or mx_register_magic().
 */
static hash *expansions;

static int mx__expand_macro(const struct expansion *e,
                            const struct mx_node *m,
                            struct sink *output,
                            void *u);

/* Parsing ------------------------------------------------------------------ */

static int next_non_whitespace(const char *input,
                               const char *end) {
  while(input < end && isspace((unsigned char)*input))
    ++input;
  return input < end ? *input : -1;
}

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
  int braces, argument_start_line, obracket, cbracket;
  const char *argument_start, *argument_end;
  struct mx_node_vector v[1];
  struct dynstr d[1];
  struct mx_node *head = 0, **tailp = &head, *e;

  if(!end)
    end = input + strlen(input);
  while(input < end) {
    if(*input != '@') {
      e = xmalloc(sizeof *e);
      e->next = 0;
      e->filename = filename;
      e->line = line;
      e->type = MX_TEXT;
      dynstr_init(d);
      /* Gather up text without any expansions in. */
      while(input < end && *input != '@') {
	if(*input == '\n')
	  ++line;
	dynstr_append(d, *input++);
      }
      dynstr_terminate(d);
      e->text = d->vec;
      *tailp = e;
      tailp = &e->next;
      continue;
    }
    if(input + 1 < end)
      switch(input[1]) {
      case '@':
        /* '@@' expands to '@' */
        e = xmalloc(sizeof *e);
        e->next = 0;
        e->filename = filename;
        e->line = line;
        e->type = MX_TEXT;
        e->text = "@";
        *tailp = e;
        tailp = &e->next;
        input += 2;
        continue;
      case '#':
        /* '@#' starts a (newline-eating comment), like dnl */
        input += 2;
        while(input < end && *input != '\n')
          ++input;
        if(*input == '\n') {
          ++line;
          ++input;
        }
        continue;
      case '_':
        /* '@_' expands to nothing.  It's there to allow dump to terminate
         * expansions without having to know what follows. */
        input += 2;
        continue;
      }
    /* It's a full expansion */
    ++input;
    e = xmalloc(sizeof *e);
    e->next = 0;
    e->filename = filename;
    e->line = line;
    e->type = MX_EXPANSION;
    /* Collect the expansion name.  Expansion names start with an alnum and
     * consist of alnums and '-'.  We don't permit whitespace between the '@'
     * and the name. */
    dynstr_init(d);
    if(input == end || !isalnum((unsigned char)*input))
      fatal(0, "%s:%d: invalid expansion", filename, e->line);
    while(input < end && (isalnum((unsigned char)*input) || *input == '-'))
      dynstr_append(d, *input++);
    dynstr_terminate(d);
    e->name = d->vec;
    /* See what the bracket character is */
    obracket = next_non_whitespace(input, end);
    switch(obracket) {
    case '(': cbracket = ')'; break;
    case '[': cbracket = ']'; break;
    case '{': cbracket = '}'; break;
    default: cbracket = obracket = -1; break;      /* no arguments */
    }
    mx_node_vector_init(v);
    if(obracket >= 0) {
      /* Gather up arguments */
      while(next_non_whitespace(input, end) == obracket) {
        while(isspace((unsigned char)*input)) {
          if(*input == '\n')
            ++line;
          ++input;
        }
        ++input;                        /* the bracket */
        braces = 0;
        /* Find the end of the argument */
        argument_start = input;
        argument_start_line = line;
        while(input < end && (*input != cbracket || braces > 0)) {
          const int c = *input++;

          if(c == obracket)
            ++braces;
          else if(c == cbracket)
            --braces;
          else if(c == '\n')
            ++line;
        }
        if(input >= end) {
          /* We ran out of input without encountering a balanced cbracket */
	  fatal(0, "%s:%d: unterminated expansion argument '%.*s'",
		filename, argument_start_line,
		(int)(input - argument_start), argument_start);
        }
        /* Consistency check */
        assert(*input == cbracket);
        /* Record the end of the argument */
        argument_end = input;
        /* Step over the cbracket */
	++input;
        /* Now we have an argument in [argument_start, argument_end), and we
         * know its filename and initial line number.  This is sufficient to
         * parse it. */
        mx_node_vector_append(v, mx_parse(filename, argument_start_line,
                                          argument_start, argument_end));
      }
    }
    /* Guarantee a NULL terminator (for the case where there's more than one
     * argument) */
    mx_node_vector_terminate(v);
    /* Fill in the remains of the node */
    e->nargs = v->nvec;
    e->args = v->vec;
    *tailp = e;
    tailp = &e->next;
  }
  return head;
}

static void mx__dump(struct dynstr *d, const struct mx_node *m) {
  int n;
  const struct mx_node *mm;

  if(!m)
    return;
  switch(m->type) {
  case MX_TEXT:
    if(m->text[0] == '@')
      dynstr_append(d, '@');
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
    /* If the next non-whitespace is '{', add @_ to stop it being
     * misinterpreted */
    mm = m->next;
    while(mm && mm->type == MX_TEXT) {
      switch(next_non_whitespace(mm->text, mm->text + strlen(mm->text))) {
      case -1:
        mm = mm->next;
        continue;
      case '{':
        dynstr_append_string(d, "@_");
        break;
      default:
        break;
      }
      break;
    }
    break;
  default:
    assert(!"invalid m->type");
  }
  mx__dump(d, m->next);
}

/** @brief Dump a parse macro expansion to a string
 *
 * Not of production quality!  Only intended for testing!
 */
char *mx_dump(const struct mx_node *m) {
  struct dynstr d[1];

  dynstr_init(d);
  mx__dump(d, m);
  dynstr_terminate(d);
  return d->vec;
}

/* Expansion registration --------------------------------------------------- */

static int mx__register(unsigned flags,
                        const char *name,
                        int min,
                        int max,
                        char **args,
                        void (*callback)(),
                        const struct mx_node *definition) {
  struct expansion e[1];

  if(!expansions)
    expansions = hash_new(sizeof(struct expansion));
  e->min = min;
  e->max = max;
  e->flags = flags;
  e->args = args;
  e->callback = callback;
  e->definition = definition;
  return hash_add(expansions, name, &e,
                  ((flags & EXP_TYPE_MASK) == EXP_MACRO)
                      ? HASH_INSERT : HASH_INSERT_OR_REPLACE);
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
  mx__register(EXP_SIMPLE,  name, min, max, 0, (void (*)())callback, 0);
}

/** @brief Register a magic expansion rule
 * @param name Name
 * @param min Minimum number of arguments
 * @param max Maximum number of arguments
 * @param callback Callback to write output
 */
void mx_register_magic(const char *name,
                       int min,
                       int max,
                       mx_magic_callback *callback) {
  mx__register(EXP_MAGIC, name, min, max, 0, (void (*)())callback, 0);
}

/** @brief Register a macro
 * @param name Name
 * @param nargs Number of arguments
 * @param args Argument names
 * @param definition Macro definition
 * @return 0 on success, negative on error
 */
int mx_register_macro(const char *name,
                      int nargs,
                      char **args,
                      const struct mx_node *definition) {
  if(mx__register(EXP_MACRO, name, nargs, nargs, args,  0/*callback*/,
                  definition)) {
    /* This locates the error to the definition, which may be a line or two
     * beyond the @define command itself.  The backtrace generated by
     * mx_expand() may help more. */
    error(0, "%s:%d: duplicate definition of '%s'",
          definition->filename, definition->line, name);
    return -2;
  }
  return 0;
}

/* Expansion ---------------------------------------------------------------- */

/** @brief Expand a template
 * @param m Where to start
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-0 on error
 *
 * Interpretation of return values:
 * - 0 means success
 * - -1 means an error writing to the sink.
 * - other negative values mean errors generated from with the macro
 *   expansion system
 * - positive values are reserved for the application
 *
 * If any callback returns non-zero then that value is returned, abandoning
 * further expansion.
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
    rc = 0;
    if(!(e = hash_find(expansions, m->name))) {
      error(0, "%s:%d: unknown expansion name '%s'",
            m->filename, m->line, m->name);
      if(sink_printf(output, "[['%s' unknown]]", m->name) < 0)
        return -1;
    } else if(m->nargs < e->min) {
      error(0, "%s:%d: expansion '%s' requires %d args, only %d given",
            m->filename, m->line, m->name, e->min, m->nargs);
      if(sink_printf(output, "[['%s' too few args]]", m->name) < 0)
        return -1;
    } else if(m->nargs > e->max) {
      error(0, "%s:%d: expansion '%s' takes at most %d args, but %d given",
            m->filename, m->line, m->name, e->max, m->nargs);
      if(sink_printf(output, "[['%s' too many args]]", m->name) < 0)
        return -1;
    } else switch(e->flags & EXP_TYPE_MASK) {
      case EXP_MAGIC: {
        /* Magic callbacks we can call directly */
        rc = ((mx_magic_callback *)e->callback)(m->nargs,
                                                m->args,
                                                output,
                                                u);
        break;
      }
      case EXP_SIMPLE: {
        /* For simple callbacks we expand their arguments for them. */
        char **args = xcalloc(1 + m->nargs, sizeof (char *)), *argname;
        int n;
        
        for(n = 0; n < m->nargs; ++n) {
          /* Argument numbers are at least clear from looking at the text;
           * adding names as well would be nice.  TODO */
          byte_xasprintf(&argname, "argument #%d", n);
          if((rc = mx_expandstr(m->args[n], &args[n], u, argname)))
            break;
        }
        if(!rc) {
          args[n] = NULL;
          rc = ((mx_simple_callback *)e->callback)(m->nargs,
                                                   args,
                                                   output,
                                                   u);
        }
        break;
      }
      case EXP_MACRO: {
        /* Macros we expand by rewriting their definition with argument values
         * substituted and then expanding that. */
        rc = mx__expand_macro(e, m, output, u);
        break;
      }
      default:
        assert(!"impossible EXP_TYPE_MASK value");
    }
    if(rc) {
      /* For non-IO errors we generate some backtrace */
      if(rc != -1)
        error(0,  "  ...in @%s at %s:%d",
              m->name, m->filename, m->line);
      return rc;
    }
    break;
  default:
    assert(!"invalid m->type");
  }
  return mx_expand(m->next, output, u);
}

/** @brief Expand a template storing the result in a string
 * @param m Where to start
 * @param sp Where to store string
 * @param u User data
 * @param what Token for backtrace, or NULL
 * @return 0 on success, non-0 on error
 *
 * Same return conventions as mx_expand().  This wrapper is slightly more
 * convenient to use from 'magic' expansions.
 */
int mx_expandstr(const struct mx_node *m,
                 char **sp,
                 void *u,
                 const char *what) {
  struct dynstr d[1];
  int rc;

  dynstr_init(d);
  if(!(rc = mx_expand(m, sink_dynstr(d), u))) {
    dynstr_terminate(d);
    *sp = d->vec;
  } else
    *sp = 0;
  if(rc && rc != -1 && what)
    error(0, "  ...in %s at %s:%d", what, m->filename, m->line);
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
  int fd, n, rc;
  struct stat sb;
  char *b;
  off_t sofar;
  const struct mx_node *m;

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
  m = mx_parse(path, 1, b, b + sb.st_size);
  rc = mx_expand(m, output, u);
  if(rc && rc != -1)
    /* Mention inclusion in backtrace */
    error(0, "  ...in inclusion of file '%s'", path);
  return rc;
}

/* Macros ------------------------------------------------------------------- */

/** @brief Rewrite a parse tree substituting sub-expansions
 * @param m Parse tree to rewrite (from macro definition)
 * @param ... Name/value pairs to rewrite
 * @return Rewritten parse tree
 *
 * The name/value pair list consists of pairs of strings and is terminated by
 * (char *)0.  Names and values are both copied so need not survive the call.
 */
const struct mx_node *mx_rewritel(const struct mx_node *m,
                                  ...) {
  va_list ap;
  hash *h = hash_new(sizeof (struct mx_node *));
  const char *n, *v;
  struct mx_node *e;

  va_start(ap, m);
  while((n = va_arg(ap, const char *))) {
    v = va_arg(ap, const char *);
    e = xmalloc(sizeof *e);
    e->next = 0;
    e->filename = m->filename;
    e->line = m->line;
    e->type = MX_TEXT;
    e->text = xstrdup(v);
    hash_add(h, n, &e, HASH_INSERT);
    /* hash_add() copies n */
  }
  return mx_rewrite(m, h);
}

/** @brief Rewrite a parse tree substituting in macro arguments
 * @param definition Parse tree to rewrite (from macro definition)
 * @param h Hash mapping argument names to argument values
 * @return Rewritten parse tree
 */
const struct mx_node *mx_rewrite(const struct mx_node *definition,
                                 hash *h) {
  const struct mx_node *head = 0, **tailp = &head, *argvalue, *m, *mm;
  struct mx_node *nm;
  int n;
  
  for(m = definition; m; m = m->next) {
    switch(m->type) {
    case MX_TEXT:
      nm = xmalloc(sizeof *nm);
      *nm = *m;                          /* Dumb copy of text node fields */
      nm->next = 0;                      /* Maintain list structure */
      *tailp = nm;
      tailp = (const struct mx_node **)&nm->next;
      break;
    case MX_EXPANSION:
      if(m->nargs == 0
         && (argvalue = *(const struct mx_node **)hash_find(h, m->name))) {
        /* This expansion has no arguments and its name matches one of the
         * macro arguments.  (Even if it's a valid expansion name we override
         * it.)  We insert its value at this point.  We do NOT recursively
         * rewrite the argument's value - it is outside the lexical scope of
         * the argument name.
         *
         * We need to recreate the list structure but a shallow copy will
         * suffice here.
         */
        for(mm = argvalue; mm; mm = mm->next) {
          nm = xmalloc(sizeof *nm);
          *nm = *mm;
          nm->next = 0;
          *tailp = nm;
          tailp = (const struct mx_node **)&nm->next;
        }
      } else {
        /* This is some other expansion.  We recursively rewrite its argument
         * values according to h. */
        nm = xmalloc(sizeof *nm);
        *nm = *m;
        nm->args = xcalloc(nm->nargs, sizeof (struct mx_node *));
        for(n = 0; n < nm->nargs; ++n)
          nm->args[n] = mx_rewrite(m->args[n], h);
        nm->next = 0;
        *tailp = nm;
        tailp = (const struct mx_node **)&nm->next;
      }
      break;
    default:
      assert(!"invalid m->type");
    }
  }
  *tailp = 0;                           /* Mark end of list */
  return head;
}

/** @brief Expand a macro
 * @param e Macro definition
 * @param m Macro expansion
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-0 on error
 */
static int mx__expand_macro(const struct expansion *e,
                            const struct mx_node *m,
                            struct sink *output,
                            void *u) {
  hash *h = hash_new(sizeof (struct mx_node *));
  int n;

  /* We store the macro arguments in a hash.  Currently there is no check for
   * duplicate argument names (and this would be the wrong place for it
   * anyway); if you do that you just lose in some undefined way. */
  for(n = 0; n < m->nargs; ++n)
    hash_add(h, e->args[n], &m->args[n], HASH_INSERT);
  /* Generate a rewritten parse tree */
  m = mx_rewrite(e->definition, h);
  /* Expand the result */
  return mx_expand(m, output, u);
  /* mx_expand() will update the backtrace */
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
