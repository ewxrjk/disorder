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

#include "macros.h"
#include "mem.h"
#include "vector.h"
#include "log.h"

VECTOR_TYPE(mx_node_vector, const struct mx_node *, xrealloc);

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

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
