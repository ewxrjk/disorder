/*
 * This file is part of Disobedience
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file disobedience/rtp.c
 * @brief RTP player support for Disobedience
 */

#include "disobedience.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>

/** @brief Path to RTP player's control socket */
static char *rtp_socket;

/** @brief Path to RTP player's logfile */
static char *rtp_log;

/** @brief Initialize @ref rtp_socket and @ref rtp_log if necessary */
static void rtp_init(void) {
  if(!rtp_socket) {
    const char *home = getenv("HOME");
    char *dir;

    byte_xasprintf(&dir, "%s/.disorder", home);
    mkdir(dir, 02700);
    byte_xasprintf(&rtp_socket, "%s/rtp", dir);
    byte_xasprintf(&rtp_log, "%s/rtp.log", dir);
  }
}

/** @brief Return a connection to the RTP player's control socket */
static FILE *rtp_connect(void) {
  struct sockaddr_un un;
  int fd;
  FILE *fp;

  rtp_init();
  memset(&un, 0, sizeof un);
  un.sun_family = AF_UNIX;
  strcpy(un.sun_path, rtp_socket);
  fd = xsocket(PF_UNIX, SOCK_STREAM, 0);
  if(connect(fd, (const struct sockaddr *)&un, sizeof un) < 0) {
    /* Connection refused just means that the player quit without deleting its
     * socket */
    switch(errno) {
    case ECONNREFUSED:
    case ENOENT:
      break;
    default:
      /* Anything else may be a problem */
      fpopup_msg(GTK_MESSAGE_ERROR, "connecting to %s: %s",
                 rtp_socket, strerror(errno));
      break;
    }
    close(fd);
    return 0;
  }
  if(!(fp = fdopen(fd, "r+"))) {
    fpopup_msg(GTK_MESSAGE_ERROR, "error calling fdopen: %s", strerror(errno));
    close(fd);
    return 0;
  }
  return fp;
}

/** @brief Return non-0 iff the RTP player is running */
int rtp_running(void) {
  FILE *fp = rtp_connect();

  if(!fp)
    return 0;                           /* no connection -> not running */
  fclose(fp);                           /* connection -> running */
  return 1;
}

/** @brief Activate the RTP player if it is not running */
void start_rtp(void) {
  pid_t pid;
  int w, fd;

  if(rtp_running())
    return;                             /* already running */
  /* double-fork so we don't have to wait() later */
  if(!(pid = xfork())) {
    if(setsid() < 0)
      fatal(errno, "error calling setsid");
    if(!(pid = xfork())) {
      /* grandchild */
      exitfn = _exit;
      /* log errors and output somewhere reasonably sane.  rtp_running()
       * will have made sure the directory exists. */
      if((fd = open(rtp_log, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
	fatal(errno, "creating %s", rtp_log);
      if(dup2(fd, 1) < 0
	 || dup2(fd, 2) < 0)
	fatal(errno, "dup2");
      if(close(fd) < 0)
	fatal(errno, "close");
      /* We don't want to hang onto whatever stdin was */
      if((fd = open("/dev/null", O_RDONLY)) < 0)
        fatal(errno, "opening /dev/null");
      if(dup2(fd, 0) < 0)
        fatal(errno, "dup2");
      if(close(fd) < 0)
	fatal(errno, "close");
      /* execute the player */
      execlp("disorder-playrtp",
	     "disorder-playrtp", "--socket", rtp_socket, (char *)0);
      fatal(errno, "disorder-playrtp");
    } else {
      /* child */
      _exit(0);
    }
  } else {
    /* parent */
    while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
      ;
  }
}

/** @brief Stop the RTP player if it is running */
void stop_rtp(void) {
  FILE *fp = rtp_connect();

  if(!fp)
    return;                             /* already stopped */
  fprintf(fp, "stop\n");
  fclose(fp);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
