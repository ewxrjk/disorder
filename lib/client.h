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
int disorder_version(disorder_client *c, char **versionp);
int disorder_play(disorder_client *c, const char *track);
int disorder_remove(disorder_client *c, const char *track);
int disorder_move(disorder_client *c, const char *track, int delta);
int disorder_enable(disorder_client *c);
int disorder_disable(disorder_client *c);
int disorder_scratch(disorder_client *c, const char *id);
int disorder_shutdown(disorder_client *c);
int disorder_reconfigure(disorder_client *c);
int disorder_rescan(disorder_client *c);
int disorder_playing(disorder_client *c, struct queue_entry **qp);
int disorder_recent(disorder_client *c, struct queue_entry **qp);
int disorder_queue(disorder_client *c, struct queue_entry **qp);
int disorder_directories(disorder_client *c, const char *dir, const char *re,
			 char ***vecp, int *nvecp);
int disorder_files(disorder_client *c, const char *dir, const char *re,
		   char ***vecp, int *nvecp);
int disorder_allfiles(disorder_client *c, const char *dir, const char *re,
		      char ***vecp, int *nvecp);
char *disorder_user(disorder_client *c);
int disorder_exists(disorder_client *c, const char *track, int *existsp);
int disorder_enabled(disorder_client *c, int *enabledp);
int disorder_set(disorder_client *c, const char *track,
		 const char *key, const char *value);
int disorder_unset(disorder_client *c, const char *track,
		   const char *key);
int disorder_get(disorder_client *c, const char *track, const char *key,
		 char **valuep);
int disorder_prefs(disorder_client *c, const char *track,
		   struct kvp **kp);
int disorder_length(disorder_client *c, const char *track,
		    long *valuep);
int disorder_search(disorder_client *c, const char *terms,
		    char ***vecp, int *nvecp);
int disorder_random_enable(disorder_client *c);
int disorder_random_disable(disorder_client *c);
int disorder_random_enabled(disorder_client *c, int *enabledp);
int disorder_stats(disorder_client *c,
		   char ***vecp, int *nvecp);
int disorder_set_volume(disorder_client *c, int left, int right);
int disorder_get_volume(disorder_client *c, int *left, int *right);
int disorder_log(disorder_client *c, struct sink *s);
int disorder_part(disorder_client *c, char **partp,
		  const char *track, const char *context, const char *part);
int disorder_resolve(disorder_client *c, char **trackp, const char *track);
int disorder_pause(disorder_client *c);
int disorder_resume(disorder_client *c);
int disorder_tags(disorder_client *c,
		   char ***vecp, int *nvecp);
int disorder_set_global(disorder_client *c,
			const char *key, const char *value);
int disorder_unset_global(disorder_client *c, const char *key);
int disorder_get_global(disorder_client *c, const char *key, char **valuep);
int disorder_new_tracks(disorder_client *c,
			char ***vecp, int *nvecp,
			int max);
int disorder_rtp_address(disorder_client *c, char **addressp, char **portp);
int disorder_adduser(disorder_client *c,
		     const char *user, const char *password,
		     const char *rights);
int disorder_deluser(disorder_client *c, const char *user);
int disorder_userinfo(disorder_client *c, const char *user, const char *key,
		      char **valuep);
int disorder_edituser(disorder_client *c, const char *user,
		      const char *key, const char *value);
int disorder_users(disorder_client *c,
		   char ***vecp, int *nvecp);
int disorder_register(disorder_client *c, const char *user,
		      const char *password, const char *email,
		      char **confirmp);
int disorder_confirm(disorder_client *c, const char *confirm);
int disorder_make_cookie(disorder_client *c, char **cookiep);
const char *disorder_last(disorder_client *c);
int disorder_revoke(disorder_client *c);
int disorder_reminder(disorder_client *c, const char *user);
int disorder_schedule_list(disorder_client *c, char ***idsp, int *nidsp);
int disorder_schedule_del(disorder_client *c, const char *id);
int disorder_schedule_get(disorder_client *c, const char *id,
			  struct kvp **actiondatap);
int disorder_schedule_add(disorder_client *c,
			  time_t when,
			  const char *priority,
			  const char *action,
			  ...);
int disorder_adopt(disorder_client *c, const char *id);
int disorder_playlist_delete(disorder_client *c,
                             const char *playlist);
int disorder_playlist_get(disorder_client *c, const char *playlist,
                          char ***tracksp, int *ntracksp);
int disorder_playlists(disorder_client *c,
                       char ***playlistsp, int *nplaylists);
int disorder_playlist_get_share(disorder_client *c, const char *playlist,
                                char **sharep);
int disorder_playlist_set_share(disorder_client *c, const char *playlist,
                                const char *share);
int disorder_playlist_lock(disorder_client *c, const char *playlist);
int disorder_playlist_unlock(disorder_client *c);
int disorder_playlist_set(disorder_client *c,
                          const char *playlist,
                          char **tracks,
                          int ntracks);

#endif /* CLIENT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
