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
#include <errno.h>
#include <stddef.h>

#include "mem.h"
#include "queue.h"
#include "log.h"
#include "split.h"
#include "syscalls.h"
#include "table.h"
#include "printf.h"

const char *playing_states[] = {
  "failed",
  "isscratch",
  "no_player",
  "ok",
  "paused",
  "quitting",
  "random",
  "scratched",
  "started",
  "unplayed"
};

#define VALUE(q, offset, type) *(type *)((char *)q + offset)

static int unmarshall_long(char *data, struct queue_entry *q,
			   size_t offset,
			   void (*error_handler)(const char *, void *),
			   void *u) {
  if(xstrtol(&VALUE(q, offset, long), data, 0, 0)) {
    error_handler(strerror(errno), u);
    return -1;
  }
  return 0;
}

static const char *marshall_long(const struct queue_entry *q, size_t offset) {
  char buffer[256];
  int n;

  n = byte_snprintf(buffer, sizeof buffer, "%ld", VALUE(q, offset, long));
  if(n < 0)
    fatal(errno, "error converting int");
  else if((size_t)n >= sizeof buffer)
    fatal(0, "long converted to decimal is too long");
  return xstrdup(buffer);
}

static int unmarshall_string(char *data, struct queue_entry *q,
			     size_t offset,
			     void attribute((unused)) (*error_handler)(const char *, void *),
			     void attribute((unused)) *u) {
  VALUE(q, offset, char *) = data;
  return 0;
}

static const char *marshall_string(const struct queue_entry *q, size_t offset) {
  return VALUE(q, offset, char *);
}

static int unmarshall_time_t(char *data, struct queue_entry *q,
			     size_t offset,
			     void (*error_handler)(const char *, void *),
			     void *u) {
  long_long ul;

  if(xstrtoll(&ul, data, 0, 0)) {
    error_handler(strerror(errno), u);
    return -1;
  }
  VALUE(q, offset, time_t) = ul;
  return 0;
}

static const char *marshall_time_t(const struct queue_entry *q, size_t offset) {
  char buffer[256];
  int n;

  n = byte_snprintf(buffer, sizeof buffer,
		    "%"PRIdMAX, (intmax_t)VALUE(q, offset, time_t));
  if(n < 0)
    fatal(errno, "error converting time");
  else if((size_t)n >= sizeof buffer)
    fatal(0, "time converted to decimal is too long");
  return xstrdup(buffer);
}

static int unmarshall_state(char *data, struct queue_entry *q,
			    size_t offset,
			    void (*error_handler)(const char *, void *),
			    void *u) {
  int n;

  if((n = table_find(playing_states, 0, sizeof (char *),
		     sizeof playing_states / sizeof *playing_states,
		     data)) < 0) {
    D(("state=[%s] n=%d", data, n));
    error_handler("invalid state", u);
    return -1;
  }
  VALUE(q, offset, enum playing_state) = n;
  return 0;
}

static const char *marshall_state(const struct queue_entry *q, size_t offset) {
  return playing_states[VALUE(q, offset, enum playing_state)];
}

#define F(n, h) { #n, offsetof(struct queue_entry, n), marshall_##h, unmarshall_##h }

static const struct field {
  const char *name;
  size_t offset;
  const char *(*marshall)(const struct queue_entry *q, size_t offset);
  int (*unmarshall)(char *data, struct queue_entry *q, size_t offset,
		    void (*error_handler)(const char *, void *),
		    void *u);
} fields[] = {
  /* Keep this table sorted. */
  F(expected, time_t),
  F(id, string),
  F(played, time_t),
  F(scratched, string),
  F(sofar, long),
  F(state, state),
  F(submitter, string),
  F(track, string),
  F(when, time_t),
  F(wstat, long)
};

int queue_unmarshall(struct queue_entry *q, const char *s,
		     void (*error_handler)(const char *, void *),
		     void *u) {
  char **vec;
  int nvec;

  if(!(vec = split(s, &nvec, SPLIT_QUOTES, error_handler, u)))
    return -1;
  return queue_unmarshall_vec(q, nvec, vec, error_handler, u);
}

int queue_unmarshall_vec(struct queue_entry *q, int nvec, char **vec,
			 void (*error_handler)(const char *, void *),
			 void *u) {
  int n;

  if(nvec % 2 != 0) {
    error_handler("invalid marshalled queue format", u);
    return -1;
  }
  while(*vec) {
    D(("key %s value %s", vec[0], vec[1]));
    if((n = TABLE_FIND(fields, name, *vec)) < 0) {
      error_handler("unknown key in queue data", u);
      return -1;
    } else {
      if(fields[n].unmarshall(vec[1], q, fields[n].offset, error_handler, u))
	return -1;
    }
    vec += 2;
  }
  return 0;
}

char *queue_marshall(const struct queue_entry *q) {
  unsigned n;
  const char *vec[sizeof fields / sizeof *fields], *v;
  char *r, *s;
  size_t len = 1;

  for(n = 0; n < sizeof fields / sizeof *fields; ++n)
    if((v = fields[n].marshall(q, fields[n].offset))) {
      vec[n] = quoteutf8(v);
      len += strlen(vec[n]) + strlen(fields[n].name) + 2;
    } else
      vec[n] = 0;
  s = r = xmalloc_noptr(len);
  for(n = 0; n < sizeof fields / sizeof *fields; ++n)
    if(vec[n]) {
      *s++ = ' ';
      s += strlen(strcpy(s, fields[n].name));
      *s++ = ' ';
      s += strlen(strcpy(s, vec[n]));
    }
  return r;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
