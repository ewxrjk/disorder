/*
 * This file is part of Disobedience
 * Copyright (C) 2007-2010 Richard Kettlewell
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
/** @file disobedience/rtp.c
 * @brief RTP player support for Disobedience
 */

#include "disobedience.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>

/** @brief Path to RTP player's control socket */
static char *rtp_socket;

/** @brief Path to RTP player's logfile */
static char *rtp_log;

/** @brief Initialize @ref rtp_socket and @ref rtp_log if necessary */
static void rtp_init(void) {
  if(!rtp_socket) {
    const char *home = getenv("HOME");
    char *dir, *base;
    struct utsname uts;

    byte_xasprintf(&dir, "%s/.disorder/", home);
    mkdir(dir, 02700);
    if(uname(&uts) < 0)
      byte_xasprintf(&base, "%s/", dir);
    else
      byte_xasprintf(&base, "%s/%s-", dir, uts.nodename);
    byte_xasprintf(&rtp_socket, "%srtp", base);
    byte_xasprintf(&rtp_log, "%srtp.log", base);
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

int rtp_getvol(int *l, int *r) {
  FILE *fp;
  int rc = -1;

  fp = rtp_connect(); if(!fp) goto end;
  fprintf(fp, "getvol\n"); fflush(fp);
  if(fscanf(fp, "%d %d\n", l, r) != 2) goto end;
  rc = 0;
end:
  if(fp) fclose(fp);
  return rc;
}

int rtp_setvol(int *l, int *r) {
  FILE *fp = rtp_connect(); if(!fp) return -1;
  fprintf(fp, "setvol %d %d\n", *l, *r); fflush(fp);
  if(fscanf(fp, "%d %d\n", l, r) != 2) { /* do nothing */ }
  fclose(fp);
  return 0;
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
      disorder_fatal(errno, "error calling setsid");
    if(!xfork()) {
      /* grandchild */
      exitfn = _exit;
      /* log errors and output somewhere reasonably sane.  rtp_running()
       * will have made sure the directory exists. */
      if((fd = open(rtp_log, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
	disorder_fatal(errno, "creating %s", rtp_log);
      if(dup2(fd, 1) < 0
	 || dup2(fd, 2) < 0)
	disorder_fatal(errno, "dup2");
      if(close(fd) < 0)
	disorder_fatal(errno, "close");
      /* We don't want to hang onto whatever stdin was */
      if((fd = open("/dev/null", O_RDONLY)) < 0)
        disorder_fatal(errno, "opening /dev/null");
      if(dup2(fd, 0) < 0)
        disorder_fatal(errno, "dup2");
      if(close(fd) < 0)
	disorder_fatal(errno, "close");
      /* execute the player */
      execlp("disorder-playrtp",
	     "disorder-playrtp",
             "--socket", rtp_socket,
             "--api", rtp_api,
             (char *)0);
      disorder_fatal(errno, "disorder-playrtp");
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

static char *rtp_config_file(void) {
  static char *rtp_config;
  const char *home = getenv("HOME");
  if(!rtp_config)
    byte_xasprintf(&rtp_config, "%s/.disorder/api", home);
  return rtp_config;
}

const char *rtp_api;

void load_rtp_config(void) {
  char *rtp_config = rtp_config_file();
  FILE *fp;
  if((fp = fopen(rtp_config, "r"))) {
    char *line;
    if(inputline(rtp_config, fp, &line, '\n') == 0) {
      for(int n = 0; uaudio_apis[n]; ++n)
        if(!strcmp(uaudio_apis[n]->name, line))
          rtp_api = line;
    }
    fclose(fp);
  }
  if(!rtp_api)
    rtp_api = uaudio_default(uaudio_apis, UAUDIO_API_CLIENT)->name;
}

void save_rtp_config(void) {
  if(rtp_api) {
    char *rtp_config = rtp_config_file();
    char *tmp;
    byte_xasprintf(&tmp, "%s.tmp", rtp_config);
    FILE *fp;
    if(!(fp = fopen(tmp, "w"))){
      fpopup_msg(GTK_MESSAGE_ERROR, "error opening %s: %s",
                 tmp, strerror(errno));
      return;
    }
    if(fprintf(fp, "%s\n", rtp_api) < 0) {
      fpopup_msg(GTK_MESSAGE_ERROR, "error writing to %s: %s",
                 tmp, strerror(errno));
      fclose(fp);
      return;
    }
    if(fclose(fp) < 0) {
      fpopup_msg(GTK_MESSAGE_ERROR, "error closing %s: %s",
                 tmp, strerror(errno));
      return;
    }
    if(rename(tmp, rtp_config) < 0) {
      fpopup_msg(GTK_MESSAGE_ERROR, "error renaming %s: %s",
                 tmp, strerror(errno));
    }
  }
}

void change_rtp_api(const char *api) {
  if(rtp_api && !strcmp(api, rtp_api))
    return;                             /* no change */
  int running = rtp_running();
  if(running)
    stop_rtp();
  rtp_api = api;
  save_rtp_config();
  // TODO this is racy and does not work; the player doesn't shut down quickly
  // enough.
  if(running)
    start_rtp();
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
