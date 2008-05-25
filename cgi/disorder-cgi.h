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
/** @file cgi/disorder-cgi.h
 * @brief Shared header for DisOrder CGI program
 */

#ifndef DISORDER_CGI_H
#define DISORDER_CGI_H

#include "common.h"

#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stddef.h>

#include "mem.h"
#include "kvp.h"
#include "queue.h"
#include "rights.h"
#include "sink.h"
#include "client.h"
#include "cgi.h"
#include "hash.h"
#include "macros.h"
#include "printf.h"
#include "defs.h"
#include "configuration.h"
#include "trackname.h"
#include "table.h"
#include "vector.h"
#include "url.h"
#include "log.h"
#include "inputline.h"
#include "split.h"
#include "mime.h"
#include "sendmail.h"
#include "charset.h"

extern disorder_client *dcgi_client;
extern char *dcgi_cookie;
extern const char *dcgi_error_string;
extern const char *dcgi_status_string;

/** @brief Entry in a list of tracks or directories */
struct dcgi_entry {
  /** @brief Track name */
  const char *track;
  /** @brief Sort key */
  const char *sort;
  /** @brief Display key */
  const char *display;
};

/** @brief Compare two @ref entry objects */
int dcgi_compare_entry(const void *a, const void *b);

void dcgi_expand(const char *name, int header);
void dcgi_action(const char *action);
void dcgi_error(const char *key);
void dcgi_login(void);
void dcgi_lookup(unsigned want);
void dcgi_lookup_reset(void);
void dcgi_expansions(void);
char *dcgi_cookie_header(void);
void dcgi_login(void);
void dcgi_get_cookie(void);
struct queue_entry *dcgi_findtrack(const char *id);

void option_set(const char *name, const char *value);
const char *option_label(const char *key);
int option_label_exists(const char *key);
char **option_columns(const char *name, int *ncolumns);

#define DCGI_QUEUE 0x0001
#define DCGI_PLAYING 0x0002
#define DCGI_RECENT 0x0004
#define DCGI_VOLUME 0x0008
#define DCGI_NEW 0x0040
#define DCGI_RIGHTS 0x0080
#define DCGI_ENABLED 0x0100
#define DCGI_RANDOM_ENABLED 0x0200

extern struct queue_entry *dcgi_queue;
extern struct queue_entry *dcgi_playing;
extern struct queue_entry *dcgi_recent;

extern int dcgi_volume_left;
extern int dcgi_volume_right;

extern char **dcgi_new;
extern int dcgi_nnew;

extern rights_type dcgi_rights;

extern int dcgi_enabled;
extern int dcgi_random_enabled;

#endif /* DISORDER_CGI_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
