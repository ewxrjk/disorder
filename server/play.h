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

#ifndef PLAY_H
#define PLAY_H

extern struct queue_entry *playing;	/* playing track or 0 */
extern int paused;			/* non-0 if paused */

void play(ev_source *ev);
/* try to play something, if playing is enabled and nothing is playing
 * already */

int playing_is_enabled(void);
/* return true iff playing is enabled */

void enable_playing(const char *who, ev_source *ev);
/* enable playing */

void disable_playing(const char *who);
/* disable playing. */

int random_is_enabled(void);
/* return true iff random play is enabled */

void enable_random(const char *who, ev_source *ev);
/* enable random play */

void disable_random(const char *who);
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

int add_random_track(void);
/* If random play is enabled then try to add a track to the queue.  On success
 * (including deliberartely doing nothing) return 0.  On error return -1. */

#endif /* PLAY_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
