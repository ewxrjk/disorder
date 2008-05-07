/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <pcre.h>
#include <limits.h>
#include <fnmatch.h>
#include <ctype.h>

#include "mem.h"
#include "log.h"
#include "hex.h"
#include "charset.h"
#include "configuration.h"
#include "table.h"
#include "syscalls.h"
#include "kvp.h"
#include "vector.h"
#include "split.h"
#include "inputline.h"
#include "regsub.h"
#include "defs.h"
#include "sink.h"
#include "server-cgi.h"
#include "printf.h"
#include "mime.h"
#include "unicode.h"
#include "hash.h"

struct kvp *cgi_args;

/* options */
struct column {
  struct column *next;
  char *name;
  int ncolumns;
  char **columns;
};

/* macros */
struct cgi_macro {
  int nargs;
  char **args;
  const char *value;
};

#define RELIST(x) struct re *x, **x##_tail = &x

static int have_read_options;
static struct kvp *labels;
static struct column *columns;

static void include_options(const char *name);
static void cgi_expand_parsed(const char *name,
			      struct cgi_element *head,
			      const struct cgi_expansion *expansions,
			      size_t nexpansions,
			      cgi_1sink *output,
			      void *u);

void cgi_header(struct sink *output, const char *name, const char *value) {
  sink_printf(output, "%s: %s\r\n", name, value);
}

void cgi_body(struct sink *output) {
  sink_printf(output, "\r\n");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
