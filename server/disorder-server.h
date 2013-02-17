/*
 * This file is part of DisOrder
 * Copyright (C) 2008-2012 Richard Kettlewell
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
/** @file server/disorder-server.h
 * @brief Definitions for server and allied utilities
 */

#ifndef DISORDER_SERVER_H
#define DISORDER_SERVER_H

#include "common.h"

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gcrypt.h>
#include <getopt.h>
#include <grp.h>
#include <locale.h>
#include <netinet/in.h>
#include <pcre.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "addr.h"
#include "authhash.h"
#include "base64.h"
#include "cache.h"
#include "charset.h"
#include "configuration.h"
#include "cookies.h"
#include "defs.h"
#include "disorder.h"
#include "event.h"
#include "eventlog.h"
#include "eventlog.h"
#include "hash.h"
#include "hex.h"
#include "inputline.h"
#include "kvp.h"
#include "log.h"
#include "logfd.h"
#include "mem.h"
#include "mime.h"
#include "printf.h"
#include "queue.h"
#include "random.h"
#include "rights.h"
#include "sendmail.h"
#include "sink.h"
#include "speaker-protocol.h"
#include "split.h"
#include "syscalls.h"
#include "table.h"
#include "trackdb-int.h"
#include "trackdb.h"
#include "trackname.h"
#include "uaudio.h"
#include "unicode.h"
#include "user.h"
#include "vector.h"
#include "version.h"
#include "wstat.h"

extern const struct uaudio *api;

void daemonize(const char *tag, int fac, const char *pidfile);
/* Go into background.  Send stdout/stderr to syslog.
 * If @pri@ is non-null, it should be "facility.level"
 * If @tag@ is non-null, it is used as a tag to each message
 * If @pidfile@ is non-null, the PID is written to that file.
 */

void quit(ev_source *ev) attribute((noreturn));
/* terminate the daemon */

int reconfigure(ev_source *ev, unsigned flags);
/* reconfigure */

void reset_sockets(ev_source *ev);

/** @brief Set when starting server */
#define RECONFIGURE_FIRST 0x0001

/** @brief Set when reloading after SIGHUP etc */
#define RECONFIGURE_RELOADING 0x0002

void dbparams_check(void);

extern struct queue_entry qhead;
/* queue of things yet to be played.  the head will be played
 * soonest. */

extern struct queue_entry phead;
/* things that have been played in the past.  the head is the oldest. */

extern long pcount;

void queue_read(void);
/* read the queue in.  Calls @fatal@ on error. */

void queue_write(void);
/* write the queue out.  Calls @fatal@ on error. */

void recent_read(void);
/* read the recently played list in.  Calls @fatal@ on error. */

void recent_write(void);
/* write the recently played list out.  Calls @fatal@ on error. */

struct queue_entry *queue_add(const char *track, const char *submitter,
			      int where, const char *target,
                              enum track_origin origin);
#define WHERE_START 0			/* Add to head of queue */
#define WHERE_END 1			/* Add to end of queue */
#define WHERE_BEFORE_RANDOM 2		/* End, or before random track */
#define WHERE_AFTER 3                   /* After the target */
#define WHERE_NOWHERE 4                 /* Don't add to queue at all */
/* add an entry to the queue.  Return a pointer to the new entry. */

void queue_remove(struct queue_entry *q, const char *who);
/* remove an from the queue */

struct queue_entry *queue_find(const char *key);
/* find a track in the queue by name or ID */

void queue_played(struct queue_entry *q);
/* add @q@ to the played list */

int queue_move(struct queue_entry *q, int delta, const char *who);
/* move element @q@ in the queue towards the front (@delta@ > 0) or towards the
 * back (@delta@ < 0).  The return value is the leftover delta once we've hit
 * the end in whichever direction we were going. */

void queue_moveafter(struct queue_entry *target,
		     int nqs, struct queue_entry **qs, const char *who);
/* Move all the elements QS to just after TARGET, or to the head if
 * TARGET=0. */

void queue_fix_sofar(struct queue_entry *q);
/* Fix up the sofar field for standalone players */

void schedule_init(ev_source *ev);
const char *schedule_add(ev_source *ev,
			 struct kvp *actiondata);
int schedule_del(const char *id);
struct kvp *schedule_get(const char *id);
char **schedule_list(int *neventsp);

extern struct queue_entry *playing;	/* playing track or 0 */
extern int paused;			/* non-0 if paused */

void play(ev_source *ev);
/* try to play something, if playing is enabled and nothing is playing
 * already */

/** @brief Return true if @p represents a true flag */
int flag_enabled(const char *s);

int playing_is_enabled(void);
/* return true iff playing is enabled */

void enable_playing(const char *who, ev_source *ev);
/* enable playing */

void disable_playing(const char *who, ev_source *ev);
/* disable playing. */

int random_is_enabled(void);
/* return true iff random play is enabled */

void enable_random(const char *who, ev_source *ev);
/* enable random play */

void disable_random(const char *who, ev_source *ev);
/* disable random play */

void scratch(const char *who, const char *id);
/* scratch the playing track.  @who@ identifies the scratcher. @id@ is
 * the ID or a null pointer. */

void quitting(ev_source *ev);
/* called to terminate current track and shut down speaker process
 * when quitting */

void speaker_setup(ev_source *ev);
/* set up the speaker subprocess */

void speaker_reload(void);
/* Tell the speaker process to reload its configuration. */

int pause_playing(const char *who);
/* Pause the current track.  Return 0 on success, -1 on error.  WHO
 * can be 0. */

void resume_playing(const char *who);
/* Resume after a pause.  WHO can be 0. */

int prepare(ev_source *ev,
	    struct queue_entry *q);
/* Prepare to play Q */

void abandon(ev_source *ev,
	     struct queue_entry *q);
/* Abandon a possibly-prepared track. */

void add_random_track(ev_source *ev);
/* If random play is enabled then try to add a track to the queue. */

int server_start(ev_source *ev, int pf,
		 size_t socklen, const struct sockaddr *sa,
		 const char *name,
                 int privileged);
/* start listening.  Return the fd. */

int server_stop(ev_source *ev, int fd);
/* Stop listening on @fd@ */

extern int volume_left, volume_right;	/* last known volume */

extern int wideopen;			/* blindly accept all logins */

/* plugins */

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

long tracklength(const char *plugin, const char *track, const char *path);

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

/* background process support *************************************************/

/** @brief Child process parameters */
struct pbgc_params {
  /** @brief Length of player command */
  int argc;
  /** @brief Player command */
  const char **argv;
  /** @brief Raw track name */
  const char *rawpath;
};

/** @brief Callback to play or prepare a track
 * @param q Track to play or decode
 * @param bgdata User data pointer
 * @return Exit code
 */
typedef int play_background_child_fn(struct queue_entry *q,
                                     const struct pbgc_params *params,
                                     void *bgdata);

int play_background(ev_source *ev,
                    const struct stringlist *player,
                    struct queue_entry *q,
                    play_background_child_fn *child,
                    void *bgdata);

/* Return values from start(),  prepare() and play_background() */

#define START_OK 0	   /**< @brief Succeeded. */
#define START_HARDFAIL 1   /**< @brief Track is broken. */
#define START_SOFTFAIL 2   /**< @brief Track OK, system (temporarily?) broken */

void periodic_mount_check(ev_source *ev_);

/** @brief How often to check for new (or old) filesystems */
# define MOUNT_CHECK_INTERVAL 5         /* seconds */

#endif /* DISORDER_SERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
