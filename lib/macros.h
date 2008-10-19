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

/** @file lib/macros.h
 * @brief Macro expansion
 */

#ifndef MACROS_H
#define MACROS_H

struct sink;

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
  const char *text;

  /** @brief Expansion name (if @p type is @ref MX_EXPANSION) */
  const char *name;

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


/** @brief Callback for simple expansions
 * @param nargs Number of arguments
 * @param args Pointer to array of arguments
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-zero on error
 */
typedef int mx_simple_callback(int nargs,
                               char **args,
                               struct sink *output,
                               void *u);

/** @brief Callback for magic expansions
 * @param nargs Number of arguments
 * @param args Pointer to array of arguments
 * @param output Where to send output
 * @param u User data
 * @return 0 on success, non-zero on error
 */
typedef int mx_magic_callback(int nargs,
                              const struct mx_node **args,
                              struct sink *output,
                              void *u);

void mx_register(const char *name,
                 int min,
                 int max,
                 mx_simple_callback *callback);
void mx_register_magic(const char *name,
                       int min,
                       int max,
                       mx_magic_callback *callback);
int mx_register_macro(const char *name,
                      int nargs,
                      char **args,
                      const struct mx_node *definition);

void mx_register_builtin(void);
void mx_search_path(const char *s);
char *mx_find(const char *name, int report);

int mx_expand_file(const char *path,
                   struct sink *output,
                   void *u);
int mx_expand(const struct mx_node *m,
              struct sink *output,
              void *u);
int mx_expandstr(const struct mx_node *m,
                 char **sp,
                 void *u,
                 const char *what);
const struct mx_node *mx_rewrite(const struct mx_node *definition,
                                 hash *h);
const struct mx_node *mx_rewritel(const struct mx_node *m,
                                  ...);

int mx_str2bool(const char *s);
const char *mx_bool2str(int n);
int mx_bool_result(struct sink *output, int result);

#endif /* MACROS_H */


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
