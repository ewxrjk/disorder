/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2009, 2011, 2013 Richard Kettlewell
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
/** @file lib/queue.c
 * @brief Track queues
 */
#include "common.h"

#include <errno.h>
#include <stddef.h>

#include "mem.h"
#include "queue.h"
#include "log.h"
#include "split.h"
#include "syscalls.h"
#include "table.h"
#include "printf.h"
#include "vector.h"

const char *const playing_states[] = {
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

/** @brief String values for @c origin field */
const char *const track_origins[] = {
  "adopted",
  "picked",
  "random",
  "scheduled",
  "scratch",
};

#define VALUE(q, offset, type) *(type *)((char *)q + offset)

/** @brief Insert queue entry @p n just after @p b
 * @param b Insert after this entry
 * @param n New entry to insert
 */
void queue_insert_entry(struct queue_entry *b, struct queue_entry *n) {
  n->prev = b;
  n->next = b->next;
  n->next->prev = n;
  n->prev->next = n;
}

/* remove an entry from a doubly-linked list */
void queue_delete_entry(struct queue_entry *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
}

static int unmarshall_long(char *data, struct queue_entry *q,
			   size_t offset,
			   void (*error_handler)(const char *, void *),
			   void *u) {
  char errbuf[1024];
  if(xstrtol(&VALUE(q, offset, long), data, 0, 0)) {
    error_handler(format_error(ec_errno, errno, errbuf, sizeof errbuf), u);
    return -1;
  }
  return 0;
}

static const char *marshall_long(const struct queue_entry *q, size_t offset) {
  char buffer[256];
  int n;

  n = byte_snprintf(buffer, sizeof buffer, "%ld", VALUE(q, offset, long));
  if(n < 0)
    disorder_fatal(errno, "error converting int");
  else if((size_t)n >= sizeof buffer)
    disorder_fatal(0, "long converted to decimal is too long");
  return xstrdup(buffer);
}

static void free_none(struct queue_entry attribute((unused)) *q,
                      size_t attribute((unused)) offset) {
}

#define free_long free_none

static int unmarshall_string(char *data, struct queue_entry *q,
			     size_t offset,
			     void attribute((unused)) (*error_handler)(const char *, void *),
			     void attribute((unused)) *u) {
  VALUE(q, offset, char *) = xstrdup(data);
  return 0;
}

static const char *marshall_string(const struct queue_entry *q, size_t offset) {
  return VALUE(q, offset, char *);
}

static void free_string(struct queue_entry *q, size_t offset) {
  xfree(VALUE(q, offset, char *));
}

static int unmarshall_time_t(char *data, struct queue_entry *q,
			     size_t offset,
			     void (*error_handler)(const char *, void *),
			     void *u) {
  long_long ul;
  char errbuf[1024];

  if(xstrtoll(&ul, data, 0, 0)) {
    error_handler(format_error(ec_errno, errno, errbuf, sizeof errbuf), u);
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
    disorder_fatal(errno, "error converting time");
  else if((size_t)n >= sizeof buffer)
    disorder_fatal(0, "time converted to decimal is too long");
  return xstrdup(buffer);
}

#define free_time_t free_none

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

static int unmarshall_origin(char *data, struct queue_entry *q,
                             size_t offset,
                             void (*error_handler)(const char *, void *),
                             void *u) {
  int n;

  if((n = table_find(track_origins, 0, sizeof (char *),
		     sizeof track_origins / sizeof *track_origins,
		     data)) < 0) {
    D(("origin=[%s] n=%d", data, n));
    error_handler("invalid origin", u);
    return -1;
  }
  VALUE(q, offset, enum track_origin) = n;
  return 0;
}

static const char *marshall_state(const struct queue_entry *q, size_t offset) {
  return playing_states[VALUE(q, offset, enum playing_state)];
}

static const char *marshall_origin(const struct queue_entry *q, size_t offset) {
  return track_origins[VALUE(q, offset, enum track_origin)];
}

#define free_state free_none
#define free_origin free_none

#define F(n, h) { #n, offsetof(struct queue_entry, n), marshall_##h, unmarshall_##h, free_##h }

/** @brief A field in a @ref queue_entry */
static const struct queue_field {
  /** @brief Field name */
  const char *name;

  /** @brief Offset of value in @ref queue_entry structure */
  size_t offset;

  /** @brief Marshaling function */
  const char *(*marshall)(const struct queue_entry *q, size_t offset);

  /** @brief Unmarshaling function */
  int (*unmarshall)(char *data, struct queue_entry *q, size_t offset,
		    void (*error_handler)(const char *, void *),
		    void *u);

  /** @brief Destructor */
  void (*free)(struct queue_entry *q, size_t offset);
} fields[] = {
  /* Keep this table sorted. */
  F(expected, time_t),
  F(id, string),
  F(origin, origin),
  F(played, time_t),
  F(scratched, string),
  F(sofar, long),
  F(state, state),
  F(submitter, string),
  F(track, string),
  F(when, time_t),
  F(wstat, long)
};
#define NFIELDS (sizeof fields / sizeof *fields)

int queue_unmarshall(struct queue_entry *q, const char *s,
		     void (*error_handler)(const char *, void *),
		     void *u) {
  char **vec;
  int nvec;

  q->pid = -1;                          /* =none */
  if(!(vec = split(s, &nvec, SPLIT_QUOTES, error_handler, u)))
    return -1;
  int rc = queue_unmarshall_vec(q, nvec, vec, error_handler, u);
  free_strings(nvec, vec);
  return rc;
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

void queue_free(struct queue_entry *q, int rest) {
  if(!q)
    return;
  if(rest)
    queue_free(q->next, rest);
  for(unsigned n = 0; n < NFIELDS; ++n)
    fields[n].free(q, fields[n].offset);
  xfree(q);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
