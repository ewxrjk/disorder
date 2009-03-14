/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2006, 2007, 2009 Richard Kettlewell
 * Portions (C) 2007 Mark Wooding
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
/** @file lib/uaudio-command.c
 * @brief Support for commmand backend
 *
 * We use the code in @ref lib/uaudio-schedule.c to ensure that we write at
 * approximately the 'real' rate.  For disorder-playrtp this isn't very useful
 * (thought it might reduce the size of various buffers downstream of us) but
 * when run from the speaker it means that pausing stands a chance of working.
 */
#include "common.h"

#include <errno.h>
#include <unistd.h>

#include "syscalls.h"
#include "log.h"
#include "mem.h"
#include "wstat.h"
#include "uaudio.h"
#include "configuration.h"

/** @brief Pipe to subprocess */
static int command_fd;

/** @brief Child process ID */
static pid_t command_pid;

static const char *const command_options[] = {
  "command",
  "pause-mode",
  NULL
};

/** @brief Close pipe and wait for subprocess to terminate */
static void command_wait(void) {
  int w;
  pid_t rc;
  char *ws;

  close(command_fd);
  while((rc = waitpid(command_pid, &w, 0) < 0 && errno == EINTR))
    ;
  if(rc < 0)
    fatal(errno, "waitpid");
  if(w) {
    ws = wstat(w);
    error(0, "command subprocess %s", ws);
    xfree(ws);
  }
}

/** @brief Create subprocess */ 
static void command_open(void) {
  int pfd[2];
  const char *command;

  if(!(command = uaudio_get("command", NULL)))
    fatal(0, "'command' not set");
  xpipe(pfd);
  command_pid = xfork();
  if(!command_pid) {
    exitfn = _exit;
    signal(SIGPIPE, SIG_DFL);
    xdup2(pfd[0], 0);
    close(pfd[0]);
    close(pfd[1]);
    /* TODO it would be nice to set some environment variables given the sample
     * format.  The original intended model is that you adapt DisOrder to the
     * command you run but it'd be nice to support the opposite. */
    execl("/bin/sh", "sh", "-c", command, (char *)0);
    fatal(errno, "error executing /bin/sh");
  }
  close(pfd[0]);
  command_fd = pfd[1];
}

/** @brief Send audio data to subprocess */
static size_t command_play(void *buffer, size_t nsamples) {
  uaudio_schedule_synchronize();
  const size_t bytes = nsamples * uaudio_sample_size;
  int written = write(command_fd, buffer, bytes);
  if(written < 0) {
    switch(errno) {
    case EINTR:
      return 0;			/* will retry */
    case EPIPE:
      error(0, "audio command subprocess terminated");
      command_wait();
      command_open();
      return 0;			/* will retry */
    default:
      fatal(errno, "error writing to audio command subprocess");
    }
  }
  const size_t written_samples = written / uaudio_sample_size;
  uaudio_schedule_update(written_samples);
  return written_samples;
}

static void command_start(uaudio_callback *callback,
                      void *userdata) {
  const char *pausemode = uaudio_get("pause-mode", "silence");
  unsigned flags = 0;

  if(!strcmp(pausemode, "silence"))
    flags |= UAUDIO_THREAD_FAKE_PAUSE;
  else if(!strcmp(pausemode, "suspend"))
    ;
  else
    fatal(0, "unknown pause mode '%s'", pausemode);
  command_open();
  uaudio_schedule_init();
  uaudio_thread_start(callback,
                      userdata,
                      command_play,
                      uaudio_channels,
		      4096 / uaudio_sample_size,
                      flags);
}

static void command_stop(void) {
  uaudio_thread_stop();
  command_wait();
}

static void command_activate(void) {
  uaudio_schedule_reactivated = 1;
  uaudio_thread_activate();
}

static void command_deactivate(void) {
  uaudio_thread_deactivate();
}

static void command_configure(void) {
  uaudio_set("command", config->speaker_command);
}

const struct uaudio uaudio_command = {
  .name = "command",
  .options = command_options,
  .start = command_start,
  .stop = command_stop,
  .activate = command_activate,
  .deactivate = command_deactivate,
  .configure = command_configure,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
