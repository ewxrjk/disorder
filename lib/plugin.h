/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#ifndef PLUGIN_H
#define PLUGIN_H

/* general ********************************************************************/

struct plugin;

typedef void *plugin_handle;
typedef void function_t(void);

const struct plugin *open_plugin(const char *name,
				 unsigned flags);
#define PLUGIN_FATAL 0x0001		/* fatal() on error */
/* Open a plugin.  Returns a null pointer on error or a handle to it
 * on success. */

function_t *get_plugin_function(const struct plugin *handle,
				const char *symbol);
const void *get_plugin_object(const struct plugin *handle,
			      const char *symbol);
/* Look up a function or an object in a plugin */

/* track length computation ***************************************************/

long tracklength(const char *track, const char *path);
/* compute the length of the track.  @track@ is the UTF-8 name of the
 * track, @path@ is the file system name (or 0 for tracks that don't
 * exist in the filesystem).  The return value should be a positive
 * number of seconds, 0 for unknown or -1 if an error occurred. */

/* collection interface *******************************************************/

void scan(const char *module, const char *root);
/* write a list of path names below @root@ to standard output. */
  
int check(const char *module, const char *root, const char *path);
/* Recheck a track, given its root and path name.  Return 1 if it
 * exists, 0 if it does not exist and -1 if an error occurred. */

/* notification interface *****************************************************/

void notify_play(const char *track,
		 const char *submitter);
/* we're going to play @track@.  It was submitted by @submitter@
 * (might be a null pointer) */

void notify_scratch(const char *track,
		    const char *submitter,
		    const char *scratcher,
		    int seconds);
/* @scratcher@ scratched @track@ after @seconds@.  It was submitted by
 * @submitter@ (might be a null pointer) */

void notify_not_scratched(const char *track,
			  const char *submitter);
/* @track@ (submitted by @submitter@, which might be a null pointer)
 * was not scratched. */

void notify_queue(const char *track,
		  const char *submitter);
/* @track@ was queued by @submitter@ */

void notify_queue_remove(const char *track,
			 const char *remover);
/* @track@ removed from the queue by @remover@ (never a null pointer) */

void notify_queue_move(const char *track,
		       const char *mover);
/* @track@ moved in the queue by @mover@ (never a null pointer) */

void notify_pause(const char *track,
		  const char *pauser);
/* TRACK was paused by PAUSER (might be a null pointer) */

void notify_resume(const char *track,
		   const char *resumer);
/* TRACK was resumed by PAUSER (might be a null pointer) */

/* track playing **************************************************************/

unsigned long play_get_type(const struct plugin *pl);
/* Get the type word for this plugin */

void *play_prefork(const struct plugin *pl,
		   const char *track);
/* Call the prefork function for PL and return the user data */

void play_track(const struct plugin *pl,
		const char *const *parameters,
		int nparameters,
		const char *path,
		const char *track);
/* play a track.  Called inside a fork. */

void play_cleanup(const struct plugin *pl, void *data);
/* Call the cleanup function for PL if necessary */

int play_pause(const struct plugin *pl, long *playedp, void *data);
/* Pause track. */

void play_resume(const struct plugin *pl, void *data);
/* Resume track. */

#endif /* PLUGIN_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
