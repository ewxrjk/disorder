/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008, 2011 Richard Kettlewell
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
/** @file cgi/options.c
 * @brief CGI options
 *
 * Options represent an additional configuration system private to the
 * CGI program.
 */

#include "disorder-cgi.h"

/** @brief State for parsing an options file */
struct read_options_state {
  /** @brief Filename */
  const char *name;

  /** @brief Line number */
  int line;
};

static hash *labels;

static void option__readfile(const char *name);

static void option__label(int attribute((unused)) nvec,
			 char **vec) {
  option_set(vec[0], vec[1]);
}

static void option__include(int attribute((unused)) nvec,
			   char **vec) {
  option__readfile(vec[0]);
}

/** @brief Definition of an option command */
static struct option {
  /** @brief Command name */
  const char *name;
  /** @brief Minimum number of arguments */
  int minargs;
  /** @brief Maximum number of arguments */
  int maxargs;
  /** @brief Command handler */
  void (*handler)(int nvec, char **vec);
} options[] = {
  { "include", 1, 1, option__include },
  { "label", 2, 2, option__label },
};

static void option__split_error(const char *msg,
			       void *u) {
  struct read_options_state *cs = u;
  
  disorder_error(0, "%s:%d: %s", cs->name, cs->line, msg);
}

static void option__readfile(const char *name) {
  int n, i;
  FILE *fp;
  char **vec, *buffer;
  struct read_options_state cs;

  if(!(cs.name = mx_find(name, 1/*report*/)))
    return;
  if(!(fp = fopen(cs.name, "r")))
    disorder_fatal(errno, "error opening %s", cs.name);
  cs.line = 0;
  while(!inputline(cs.name, fp, &buffer, '\n')) {
    ++cs.line;
    if(!(vec = split(buffer, &n, SPLIT_COMMENTS|SPLIT_QUOTES,
		     option__split_error, &cs)))
      continue;
    if(!n)
      continue;
    if((i = TABLE_FIND(options, name, vec[0])) == -1) {
      disorder_error(0, "%s:%d: unknown option '%s'", cs.name, cs.line, vec[0]);
      continue;
    }
    ++vec;
    --n;
    if(n < options[i].minargs) {
      disorder_error(0, "%s:%d: too few arguments to '%s'", cs.name, cs.line, vec[-1]);
      continue;
    }
    if(n > options[i].maxargs) {
      disorder_error(0, "%s:%d: too many arguments to '%s'", cs.name, cs.line, vec[-1]);
      continue;
    }
    options[i].handler(n, vec);
  }
  fclose(fp);
}

static void option__init(void) {
  static int have_read_options;
  
  if(!have_read_options) {
    have_read_options = 1;
    labels = hash_new(sizeof (char *));
    option__readfile("options");
  }
}

/** @brief Set an option
 * @param name Option name
 * @param value Option value
 *
 * If the option was already set its value is replaced.
 *
 * @p name and @p value are copied.
 */
void option_set(const char *name, const char *value) {
  char *v = xstrdup(value);

  option__init();
  hash_add(labels, name, &v, HASH_INSERT_OR_REPLACE);
}

/** @brief Get a label
 * @param key Name of label
 * @return Value of label (never NULL)
 *
 * If label images.X isn't found then the return value is
 * <url.static>X.png, allowing url.static to be used to provide a base
 * for all images with per-image overrides.
 *
 * Otherwise undefined labels expand to their last (dot-separated)
 * component.
 */
const char *option_label(const char *key) {
  const char *label;
  char **lptr;

  option__init();
  lptr = hash_find(labels, key);
  if(lptr)
    return *lptr;
  /* No label found */
  if(!strncmp(key, "images.", 7)) {
    static const char *url_static;
    /* images.X defaults to <url.static>X.png */
    
    if(!url_static)
      url_static = option_label("url.static");
    byte_xasprintf((char **)&label, "%s%s.png", url_static, key + 7);
  } else if((label = strrchr(key, '.')))
    /* X.Y defaults to Y */
    ++label;
  else
    /* otherwise default to label name */
    label = key;
  return label;
}

/** @brief Test whether a label exists
 * @param key Name of label
 * @return 1 if label exists, otherwise 0
 *
 * Labels that don't exist still have an expansion (per option_label()
 * documentation), and possibly not even a placeholder one.
 */
int option_label_exists(const char *key) {
  option__init();
  return !!hash_find(labels, key);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
