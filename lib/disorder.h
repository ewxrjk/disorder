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

#ifndef DISORDER_H
#define DISORDER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory allocation **********************************************************/

void *disorder_malloc(size_t);
void *disorder_realloc(void *, size_t);
/* As malloc/realloc, but
 * 1) succeed or call fatal
 * 2) always clear (the unused part of) the new allocation
 * 3) are garbage-collected
 */

void *disorder_malloc_noptr(size_t);
void *disorder_realloc_noptr(void *, size_t);
char *disorder_strdup(const char *);
char *disorder_strndup(const char *, size_t);
/* As malloc/realloc/strdup, but
 * 1) succeed or call fatal
 * 2) are garbage-collected
 * 3) allocated space must not contain any pointers
 *
 * {xmalloc,xrealloc}_noptr don't promise to clear the new space
 */

#ifdef __GNUC__

int disorder_snprintf(char buffer[], size_t bufsize, const char *fmt, ...)
  __attribute__((format (printf, 3, 4)));
/* like snprintf */
  
int disorder_asprintf(char **rp, const char *fmt, ...)
  __attribute__((format (printf, 2, 3)));
/* like asprintf but uses xmalloc_noptr() */

#else

int disorder_snprintf(char buffer[], size_t bufsize, const char *fmt, ...);
/* like snprintf */
  
int disorder_asprintf(char **rp, const char *fmt, ...);
/* like asprintf but uses xmalloc_noptr() */

#endif
 

/* logging ********************************************************************/

void disorder_error(int errno_value, const char *fmt, ...);
/* report an error.  If errno_value is nonzero then the errno string
 * is included. */
  
void disorder_fatal(int errno_value, const char *fmt, ...);
/* report an error and terminate.  If errno_value is nonzero then the
 * errno string is included.  This is the only safe way to terminate
 * the process. */
  
void disorder_info(const char *fmt, ...);
/* log a message. */
  
/* track database *************************************************************/

int disorder_track_exists(const char *track);
/* return true if the track exists. */

const char *disorder_track_get_data(const char *track, const char *key);
/* get the value for @key@ (xstrdup'd) */

int disorder_track_set_data(const char *track,
			    const char *key, const char *value);
/* set the value of @key@ to @value@, or remove it if @value@ is a null
 * pointer.  Return 0 on success, -1 on error. */

const char *disorder_track_random(void); /* server plugins only */
/* return the name of a random track */
  
/* plugin interfaces **********************************************************/

long disorder_tracklength(const char *track, const char *path);
/* compute the length of the track.  @track@ is the UTF-8 name of the
 * track, @path@ is the file system name (or 0 for tracks that don't
 * exist in the filesystem).  The return value should be a positive
 * number of seconds, 0 for unknown or -1 if an error occurred. */

void disorder_scan(const char *root);
/* write a list of path names below @root@ to standard output. */

int disorder_check(const char *root, const char *path);
/* Recheck a track, given its root and path name.  Return 1 if it
 * exists, 0 if it does not exist and -1 if an error occurred. */
  
void disorder_notify_play(const char *track,
			  const char *submitter);
/* we're going to play @track@.  It was submitted by @submitter@
 * (might be a null pointer) */

void disorder_notify_scratch(const char *track,
			     const char *submitter,
			     const char *scratcher,
			     int seconds);
/* @scratcher@ scratched @track@ after @seconds@.  It was submitted by
 * @submitter@ (might be a null pointer) */

void disorder_notify_not_scratched(const char *track,
				   const char *submitter);
/* @track@ (submitted by @submitter@, which might be a null pointer)
 * was not scratched. */
  
void disorder_notify_queue(const char *track,
			   const char *submitter);
/* @track@ added to the queue by @submitter@ (never a null pointer) */

void disorder_notify_queue_remove(const char *track,
				  const char *remover);
/* @track@ removed from the queue by @remover@ (never a null pointer) */

void disorder_notify_queue_move(const char *track,
				const char *mover);
/* @track@ moved in the queue by @mover@ (never a null pointer) */

void disorder_notify_pause(const char *track,
			   const char *pauser);
/* TRACK was paused by PAUSER (might be a null pointer) */

void disorder_notify_resume(const char *track,
			    const char *resumer);
/* TRACK was resumed by PAUSER (might be a null pointer) */
  
/* player plugin interface ****************************************************/

extern const unsigned long disorder_player_type;

#define DISORDER_PLAYER_STANDALONE 0x00000000
/* this player plays sound directly */

#define DISORDER_PLAYER_RAW        0x00000001
/* player that sends raw samples to $DISORDER_RAW_FD */

#define DISORDER_PLAYER_TYPEMASK   0x000000ff
/* mask for player types */

#define DISORDER_PLAYER_PREFORK    0x00000100
/* call prefork function */

#define DISORDER_PLAYER_PAUSES     0x00000200
/* supports pausing */

void *disorder_play_prefork(const char *track);
/* Called outside the fork.  Should not block.  Returns a null pointer
 * on error.
 *
 * If _play_prefork is called then its return value is used as the
 * DATA argument to the following functions.  Otherwise the value of
 * DATA argument is indeterminate and must not be used. */

void disorder_play_track(const char *const *parameters,
                         int nparameters,
                         const char *path,
                         const char *track,
                         void *data);
/* Called to play a track.  Should either exec or only return when the
 * track has finished.  Should not call exit() (except after a
 * succesful exec).  Allowed to call _Exit(). */

int disorder_play_pause(long *playedp, void *data);
/* Pauses the playing track.  If the track can be paused returns 0 and
 * stores the number of seconds so far played via PLAYEDP, or sets it
 * to -1 if this is not known.  If the track cannot be paused then
 * returns -1.  Should not block.
 */

void disorder_play_resume(void *data);
/* Restarts play after a pause.  PLAYED is the value returned from the
 * original pause operation.  Should not block. */

void disorder_play_cleanup(void *data);
/* called to clean up DATA.  Should not block. */

#ifdef __cplusplus
};
#endif

#endif /* DISORDER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
