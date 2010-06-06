/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2009 Richard Kettlewell
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
/** @file server/background.c
 * @brief Background process support for playing tracks
 */

#include "disorder-server.h"

/** @brief Fork the player or decoder for @p q
 * @param ev Event loop
 * @param player Pointer to player information
 * @param q Track to play or decode
 * @param child Function to run inside fork
 * @param bgdata Passed to @c child()
 *
 * @c q->pl had better already be set.
 */
int play_background(ev_source *ev,
                    const struct stringlist *player,
                    struct queue_entry *q,
                    play_background_child_fn *child,
                    void *bgdata) {
  int lfd;
  struct pbgc_params params[1];

  memset(params, 0, sizeof params);
  /* Get the raw path.  This needs to be done outside the fork.  It's needed by
   * the play-track callback which has to have the raw filename bytes we got
   * from readdir() as well as the normalized unicode version of the track
   * name.  (Emphasize 'normalized'; even if you use UTF-8 for your filenames,
   * they might not be normalized and if they are they might not be normalized
   * to the same canonical form as DisOrder uses.) */
  params->rawpath = trackdb_rawpath(q->track);
  /* Call the prefork function in the player module.  None of the built-in
   * modules use this so it's not well tested, unfortunately. */
  if(q->type & DISORDER_PLAYER_PREFORK)
    if(!(q->data = play_prefork(q->pl, q->track))) {
      disorder_error(0, "prefork function for %s failed", q->track);
      return START_HARDFAIL;
    }
  /* Capture the player/decoder's stderr and feed it into our logs.
   *
   * Use the second arg as the tag if available (it's probably a command name),
   * otherwise the module name. */
  if(!isatty(2))
    lfd = logfd(ev, player->s[2] ? player->s[2] : player->s[1]);
  else
    lfd = -1;
  /* Parse player arguments */
  int optc = player->n - 2;
  const char **optv = (const char **)&player->s[2];
  while(optc > 0 && optv[0][0] == '-') {
    if(!strcmp(optv[0], "--")) {
      ++optv;
      --optc;
      break;
    }
    if(!strcmp(optv[0], "--wait-for-device")
       || !strncmp(optv[0], "--wait-for-device=", 18)) {
      const char *waitdevice;
      if((waitdevice = strchr(optv[0], '='))) {
	params->waitdevice = waitdevice + 1;
      } else
        params->waitdevice = "";	/* use default */
      ++optv;
      --optc;
    } else {
      disorder_error(0, "unknown option %s", optv[0]);
      return START_HARDFAIL;
    }
  }
  params->argc = optc;
  params->argv = optv;
  /* Create the child process */
  switch(q->pid = fork()) {
  case 0:
    /* Child of disorderd */
    exitfn = _exit;
    progname = "disorderd-fork";
    ev_signal_atfork(ev);
    signal(SIGPIPE, SIG_DFL);
    /* Send our log output to DisOrder's logs */
    if(lfd != -1) {
      xdup2(lfd, 1);
      xdup2(lfd, 2);
      xclose(lfd);			/* tidy up */
    }
    /* Create a new process group, ID = child PID */
    setpgid(0, 0);
    _exit(child(q, params, bgdata));
  case -1:
    /* Back in disorderd (child could not be created) */
    disorder_error(errno, "error calling fork");
    if(q->type & DISORDER_PLAYER_PREFORK)
      play_cleanup(q->pl, q->data);	/* else would leak */
    if(lfd != -1)
      xclose(lfd);
    return START_SOFTFAIL;
  }
  /* We don't need the child's end of the log pipe */
  if(lfd != -1)
    xclose(lfd);
  /* Set the child's process group ID.
   *
   * But wait, didn't we already set it in the child?  Yes, but it's possible
   * that we'll need to address it by process group ID before it gets that far,
   * so we set it here too.  One or the other may fail but as long as one
   * succeeds that's fine.
   */
  setpgid(q->pid, q->pid);
  /* Ask the event loop to tell us when the child terminates */
  D(("player subprocess ID %lu", (unsigned long)q->pid));
  return START_OK;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
