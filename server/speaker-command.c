/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
 * Portions (C) 2007 Mark Wooding
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
/** @file server/speaker-command.c
 * @brief Support for @ref BACKEND_COMMAND */

#include <config.h>
#include "types.h"

#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "speaker-protocol.h"
#include "speaker.h"

/** @brief Pipe to subprocess
 *
 * This is the file descriptor to write to for @ref BACKEND_COMMAND.
 */
static int cmdfd = -1;

/** @brief poll array slot for @ref cmdfd
 *
 * Set by command_beforepoll().
 */
static int cmdfd_slot;

/** @brief Start the subprocess for @ref BACKEND_COMMAND */
static void fork_cmd(void) {
  pid_t cmdpid;
  int pfd[2];
  if(cmdfd != -1) close(cmdfd);
  xpipe(pfd);
  cmdpid = xfork();
  if(!cmdpid) {
    exitfn = _exit;
    signal(SIGPIPE, SIG_DFL);
    xdup2(pfd[0], 0);
    close(pfd[0]);
    close(pfd[1]);
    execl("/bin/sh", "sh", "-c", config->speaker_command, (char *)0);
    fatal(errno, "error execing /bin/sh");
  }
  close(pfd[0]);
  cmdfd = pfd[1];
  D(("forked cmd %d, fd = %d", cmdpid, cmdfd));
}

/** @brief Command backend initialization */
static void command_init(void) {
  info("selected command backend");
  fork_cmd();
}

/** @brief Play to a subprocess */
static size_t command_play(size_t frames) {
  size_t bytes = frames * bpf;
  int written_bytes;

  written_bytes = write(cmdfd, playing->buffer + playing->start, bytes);
  D(("actually play %zu bytes, wrote %d",
     bytes, written_bytes));
  if(written_bytes < 0) {
    switch(errno) {
    case EPIPE:
      error(0, "hmm, command died; trying another");
      fork_cmd();
      return 0;
    case EAGAIN:
      return 0;
    default:
      fatal(errno, "error writing to subprocess");
    }
  } else
    return written_bytes / bpf;
}

/** @brief Update poll array for writing to subprocess */
static void command_beforepoll(int attribute((unused)) *timeoutp) {
  /* We send sample data to the subprocess as fast as it can accept it.
   * This isn't ideal as pause latency can be very high as a result. */
  if(cmdfd >= 0)
    cmdfd_slot = addfd(cmdfd, POLLOUT);
}

/** @brief Process poll() results for subprocess play */
static int command_ready(void) {
  if(fds[cmdfd_slot].revents & (POLLOUT | POLLERR))
    return 1;
  else
    return 0;
}

const struct speaker_backend command_backend = {
  BACKEND_COMMAND,
  0,
  command_init,
  0,                                    /* activate */
  command_play,
  0,                                    /* deactivate */
  command_beforepoll,
  command_ready
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
