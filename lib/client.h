/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file lib/client.h
 * @brief Simple C client
 *
 * See @ref lib/eclient.h for an asynchronous-capable client
 * implementation.
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <time.h>
#include "configuration.h"

/** @brief Client data */
typedef struct disorder_client disorder_client;

struct queue_entry;
struct kvp;
struct sink;

disorder_client *disorder_new(int verbose);
int disorder_connect(disorder_client *c);
int disorder_connect_user(disorder_client *c,
			  const char *username,
			  const char *password);
int disorder_connect_cookie(disorder_client *c, const char *cookie);
int disorder_connect_generic(struct config *conf,
                             disorder_client *c,
                             const char *username,
                             const char *password,
                             const char *cookie);
int disorder_close(disorder_client *c);
int disorder_playing(disorder_client *c, struct queue_entry **qp);
char *disorder_user(disorder_client *c);
int disorder_prefs(disorder_client *c, const char *track,
		   struct kvp **kp);
int disorder_set_volume(disorder_client *c, int left, int right);
int disorder_get_volume(disorder_client *c, int *left, int *right);
int disorder_log(disorder_client *c, struct sink *s);
int disorder_rtp_address(disorder_client *c, char **addressp, char **portp);
const char *disorder_last(disorder_client *c);
int disorder_schedule_get(disorder_client *c, const char *id,
			  struct kvp **actiondatap);
int disorder_schedule_add(disorder_client *c,
			  time_t when,
			  const char *priority,
			  const char *action,
			  ...);

#include "client-stubs.h"

#endif /* CLIENT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
