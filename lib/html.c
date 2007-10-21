/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/html.c
 * @brief Noddy HTML parser
 */

#include <config.h>
#include "types.h"

#include <string.h>
#include <ctype.h>
#include <stddef.h>

#include "hash.h"
#include "html.h"
#include "mem.h"
#include "log.h"
#include "vector.h"
#include "charset.h"
#include "table.h"

/** @brief Entity table type */
struct entity {
  const char *name;
  uint32_t value;
};

/** @brief Known entities
 *
 * We only support the entities that turn up in the HTML files we
 * actually care about.
 *
 * Keep in alphabetical order.
 */
static const struct entity entities[] = {
  { "amp", '&' },
  { "gt", '>' },
  { "lt", '<' }
};

/** @brief Skip whitespace */
static const char *skipwhite(const char *input) {
  while(*input && isspace((unsigned char)*input))
    ++input;
  return input;
}

/** @brief Parse an entity */
static const char *parse_entity(const char *input,
				uint32_t *entityp) {
  input = skipwhite(input);
  if(*input == '#') {
    input = skipwhite(input + 1);
    if(*input == 'x')
      *entityp = strtoul(skipwhite(input + 1), (char **)&input, 16);
    else
      *entityp = strtoul(input, (char **)&input, 10);
  } else {
    struct dynstr name[1];
    int n;

    dynstr_init(name);
    while(isalnum((unsigned char)*input))
      dynstr_append(name, tolower((unsigned char)*input++));
    dynstr_terminate(name);
    if((n = TABLE_FIND(entities, struct entity, name, name->vec)) < 0) {
      error(0, "unknown entity '%s'", name->vec);
      *entityp = '?';
    } else
      *entityp = entities[n].value;
  }
  input = skipwhite(input);
  if(*input == ';')
    ++input;
  return input;
}

/** @brief Parse one character or entity and append it to a @ref dynstr */
static const char *parse_one(const char *input, struct dynstr *d) {
  if(*input == '&') {
    uint32_t c;
    input = parse_entity(input + 1, &c);
    if(one_ucs42utf8(c, d))
      dynstr_append(d, '?');	/* U+FFFD might be a better choice */
  } else
    dynstr_append(d, *input++);
  return input;
}

/** @brief Too-stupid-to-live HTML parser
 * @param callbacks Parser callbacks
 * @param input HTML document
 * @param u User data pointer
 * @return 0 on success, -1 on error
 */
int html_parse(const struct html_parser_callbacks *callbacks,
	       const char *input,
	       void *u) {
  struct dynstr text[1];

  dynstr_init(text);
  while(*input) {
    if(*input == '<') {
      struct dynstr tag[1];
      hash *attrs;

      /* flush collected text */
      if(text->nvec) {
	dynstr_terminate(text);
	callbacks->text(text->vec, u);
	text->nvec = 0;
      }
      dynstr_init(tag);
      input = skipwhite(input + 1);
      /* see if it's an open or close tag */
      if(*input == '/') {
	input = skipwhite(input + 1);
	attrs = 0;
      } else
	attrs = hash_new(sizeof(char *));
      /* gather tag */
      while(isalnum((unsigned char)*input))
	dynstr_append(tag, tolower((unsigned char)*input++));
      dynstr_terminate(tag);
      input = skipwhite(input);
      if(attrs) {
	/* gather attributes */
	while(*input && *input != '>') {
	  struct dynstr name[1], value[1];

	  dynstr_init(name);
	  dynstr_init(value);
	  /* attribute name */
	  while(isalnum((unsigned char)*input))
	    dynstr_append(name, tolower((unsigned char)*input++));
	  dynstr_terminate(name);	
	  input = skipwhite(input);
	  if(*input == '=') {
	    /* attribute value */
	    input = skipwhite(input + 1);
	    if(*input == '"' || *input == '\'') {
	      /* quoted value */
	      const int q = *input++;
	      while(*input && *input != q)
		input = parse_one(input, value);
	      if(*input == q)
		++input;
	    } else {
	      /* unquoted value */
	      while(*input && *input != '>' && !isspace((unsigned char)*input))
		input = parse_one(input, value);
	    }
	    dynstr_terminate(value);
	  }
	  /* stash the value */
	  hash_add(attrs, name->vec, value->vec, HASH_INSERT_OR_REPLACE);
	  input = skipwhite(input);
	}
      }
      if(*input != '>') {
	error(0, "unterminated tag %s", tag->vec);
	return -1;
      }
      ++input;
      if(attrs)
	callbacks->open(tag->vec, attrs, u);
      else
	callbacks->close(tag->vec, u);
    } else
      input = parse_one(input, text);
  }
  /* flush any trailing text */
  if(text->nvec) {
    dynstr_terminate(text);
    callbacks->text(text->vec, u);
    text->nvec = 0;
  }
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
