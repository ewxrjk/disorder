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

/** @file lib/macros.h
 * @brief Macro expansion
 */

#ifndef MACROS_H
#define MACROS_H

/** @brief One node in a macro expansion parse tree */
struct mx_node {
  /** @brief Next element or NULL at end of list */
  struct mx_node *next;

  /** @brief Node type, @ref MX_TEXT or @ref MX_EXPANSION. */
  int type;

  /** @brief Filename containing this node */
  const char *filename;
  
  /** @brief Line number at start of this node */
  int line;
  
  /** @brief Plain text (if @p type is @ref MX_TEXT) */
  char *text;

  /** @brief Expansion name (if @p type is @ref MX_EXPANSION) */
  char *name;

  /** @brief Argument count (if @p type is @ref MX_EXPANSION) */
  int nargs;

  /** @brief Argument values, parsed recursively (or NULL if @p nargs is 0) */
  const struct mx_node **args;
};

/** @brief Text node */
#define MX_TEXT 0

/** @brief Expansion node */
#define MX_EXPANSION 1

const struct mx_node *mx_parse(const char *filename,
			       int line,
			       const char *input,
			       const char *end);

char *mx_dump(const struct mx_node *m);

#endif /* MACROS_H */


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
