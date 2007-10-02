/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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

#ifndef CLIENT_H
#define CLIENT_H

/* A simple synchronous client interface. */

typedef struct disorder_client disorder_client;

struct queue_entry;
struct kvp;
struct sink;

/* Parameter strings (e.g. @track@) are UTF-8 unless specified
 * otherwise. */

disorder_client *disorder_new(int verbose);
/* create a new disorder_client */

int disorder_running(disorder_client *c);
/* return 1 if the server is running, else 0 */

int disorder_connect(disorder_client *c);
/* connect a disorder_client using the default settings */

int disorder_connect_sock(disorder_client *c,
			  const struct sockaddr *sa,
			  socklen_t len,
			  const char *username,
			  const char *password,
			  const char *ident);
/* connect a disorder_client */

int disorder_close(disorder_client *c);
/* close a disorder_client */

int disorder_become(disorder_client *c, const char *user);
/* become another user (trusted users only) */

int disorder_version(disorder_client *c, char **versionp);
/* get the server version */

int disorder_play(disorder_client *c, const char *track);
/* add a track to the queue */

int disorder_remove(disorder_client *c, const char *track);
/* remove a track from the queue */

int disorder_move(disorder_client *c, const char *track, int delta);
/* move a track in the queue @delta@ steps towards the head */

int disorder_enable(disorder_client *c);
/* enable playing if it is not already enabled */

int disorder_disable(disorder_client *c);
/* disable playing if it is not already disabled. */

int disorder_scratch(disorder_client *c, const char *id);
/* scratch the currently playing track.  If @id@ is not a null pointer
 * then the scratch will be ignored if the ID does not mactch. */

int disorder_shutdown(disorder_client *c);
/* shut down the server immediately */

int disorder_reconfigure(disorder_client *c);
/* re-read the configuration file */

int disorder_rescan(disorder_client *c);
/* initiate a rescan */

int disorder_playing(disorder_client *c, struct queue_entry **qp);
/* get the details of the currently playing track (null pointer if
 * nothing playing).  The first entry in the list is the next track to
 * be played. */

int disorder_recent(disorder_client *c, struct queue_entry **qp);
/* get a list of recently played tracks.  The LAST entry in the list
 * is last track to have been played. */

int disorder_queue(disorder_client *c, struct queue_entry **qp);
/* get the queue */

int disorder_directories(disorder_client *c, const char *dir, const char *re,
			 char ***vecp, int *nvecp);
/* get subdirectories of @dir@, or of the root if @dir@ is an null
 * pointer */

int disorder_files(disorder_client *c, const char *dir, const char *re,
		   char ***vecp, int *nvecp);
/* get list of files in @dir@ */

int disorder_allfiles(disorder_client *c, const char *dir, const char *re,
		      char ***vecp, int *nvecp);
/* get list of files and directories in @dir@ */

char *disorder_user(disorder_client *c);
/* remind ourselves what user we went in as */

int disorder_exists(disorder_client *c, const char *track, int *existsp);
/* set @*existsp@ to 1 if the track exists, else 0 */

int disorder_enabled(disorder_client *c, int *enabledp);
/* set @*enabledp@ to 1 if playing enabled, else 0 */

int disorder_set(disorder_client *c, const char *track,
		 const char *key, const char *value);
int disorder_unset(disorder_client *c, const char *track,
		   const char *key);
int disorder_get(disorder_client *c, const char *track, const char *key,
		 char **valuep);
int disorder_prefs(disorder_client *c, const char *track,
		   struct kvp **kp);
/* set, unset, get, list preferences */

int disorder_length(disorder_client *c, const char *track,
		    long *valuep);
/* get the length of a track in seconds, if it is known */

int disorder_search(disorder_client *c, const char *terms,
		    char ***vecp, int *nvecp);
/* get a list of tracks matching @words@ */

int disorder_random_enable(disorder_client *c);
/* enable random play if it is not already enabled */

int disorder_random_disable(disorder_client *c);
/* disable random play if it is not already disabled */

int disorder_random_enabled(disorder_client *c, int *enabledp);
/* determine whether random play is enabled */

int disorder_stats(disorder_client *c,
		   char ***vecp, int *nvecp);
/* get server statistics */

int disorder_set_volume(disorder_client *c, int left, int right);
int disorder_get_volume(disorder_client *c, int *left, int *right);
/* get or set the volume */

int disorder_log(disorder_client *c, struct sink *s);
/* send log output to @s@ */

int disorder_part(disorder_client *c, char **partp,
		  const char *track, const char *context, const char *part);
/* get a track name part */

int disorder_resolve(disorder_client *c, char **trackp, const char *track);
/* resolve a track name */

int disorder_pause(disorder_client *c);
/* Pause the currently playing track. */

int disorder_resume(disorder_client *c);
/* Resume after a pause. */

int disorder_tags(disorder_client *c,
		   char ***vecp, int *nvecp);
/* get known tags */

int disorder_set_global(disorder_client *c,
			const char *key, const char *value);
int disorder_unset_global(disorder_client *c, const char *key);
int disorder_get_global(disorder_client *c, const char *key, char **valuep);
/* get/unset/set global prefs */

int disorder_new_tracks(disorder_client *c,
			char ***vecp, int *nvecp,
			int max);
/* get new tracks */

#endif /* CLIENT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
