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
/** @file disobedience/lookup.c
 * @brief Disobedience server lookups and caching
 */
#include "disobedience.h"

static int namepart_lookups_outstanding;
static const struct cache_type cachetype_string = { 3600 };
static const struct cache_type cachetype_integer = { 3600 };

/** @brief Called when a namepart lookup has completed or failed
 *
 * When there are no lookups in flight a redraw is provoked.  This might well
 * provoke further lookups.
 */
static void namepart_completed_or_failed(void) {
  --namepart_lookups_outstanding;
  if(!namepart_lookups_outstanding)
    /* When all lookups complete, we update any displays that care */
    event_raise("lookups-completed", 0);
}

/** @brief Called when a namepart lookup has completed */
static void namepart_completed(void *v, const char *err, const char *value) {
  D(("namepart_completed"));
  if(err) {
    gtk_label_set_text(GTK_LABEL(report_label), err);
    value = "?";
  }
  const char *key = v;
  
  cache_put(&cachetype_string, key, value);
  namepart_completed_or_failed();
}

/** @brief Called when a length lookup has completed */
static void length_completed(void *v, const char *err, long l) {
  D(("length_completed"));
  if(err) {
    gtk_label_set_text(GTK_LABEL(report_label), err);
    l = -1;
  }
  const char *key = v;
  long *value;
  
  D(("namepart_completed"));
  value = xmalloc(sizeof *value);
  *value = l;
  cache_put(&cachetype_integer, key, value);
  namepart_completed_or_failed();
}

/** @brief Arrange to fill in a namepart cache entry */
static void namepart_fill(const char *track,
                          const char *context,
                          const char *part,
                          const char *key) {
  D(("namepart_fill %s %s %s %s", track, context, part, key));
  ++namepart_lookups_outstanding;
  D(("namepart_lookups_outstanding -> %d\n", namepart_lookups_outstanding));
  disorder_eclient_namepart(client, namepart_completed,
                            track, context, part, (void *)key);
}

/** @brief Look up a namepart
 * @param track Track name
 * @param context Context
 * @param part Name part
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
const char *namepart(const char *track,
                     const char *context,
                     const char *part) {
  char *key;
  const char *value;

  D(("namepart %s %s %s", track, context, part));
  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  value = cache_get(&cachetype_string, key);
  if(!value) {
    D(("deferring..."));
    namepart_fill(track, context, part, key);
    value = "?";
  }
  return value;
}

/** @brief Called from @ref disobedience/properties.c when we know a name part has changed */
void namepart_update(const char *track,
                     const char *context,
                     const char *part) {
  char *key;

  byte_xasprintf(&key, "namepart context=%s part=%s track=%s",
                 context, part, track);
  /* Only refetch if it's actually in the cache. */
  if(cache_get(&cachetype_string, key))
    namepart_fill(track, context, part, key);
}

/** @brief Look up a track length
 *
 * If it is in the cache then just return its value.  If not then look it up
 * and arrange for the queues to be updated when its value is available. */
long namepart_length(const char *track) {
  char *key;
  const long *value;

  D(("getlength %s", track));
  byte_xasprintf(&key, "length track=%s", track);
  value = cache_get(&cachetype_integer, key);
  if(value)
    return *value;
  D(("deferring..."));;
  ++namepart_lookups_outstanding;
  D(("namepart_lookups_outstanding -> %d\n", namepart_lookups_outstanding));
  disorder_eclient_length(client, length_completed, track, key);
  return -1;
}

/** @brief Resolve a track name
 *
 * Returns the supplied track name if it doesn't have the answer yet.
 */
char *namepart_resolve(const char *track) {
  char *key;

  byte_xasprintf(&key, "resolve track=%s", track);
  const char *value = cache_get(&cachetype_string, key);
  if(!value) {
    D(("deferring..."));
    ++namepart_lookups_outstanding;
    D(("namepart_lookups_outstanding -> %d\n", namepart_lookups_outstanding));
    disorder_eclient_resolve(client, namepart_completed,
                             track, (void *)key);
    value = track;
  }
  return xstrdup(value);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
