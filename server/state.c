/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#include <stdio.h>
#include <pcre.h>
#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "event.h"
#include "play.h"
#include "trackdb.h"
#include "state.h"
#include "configuration.h"
#include "log.h"
#include "queue.h"
#include "server-queue.h"
#include "server.h"
#include "printf.h"
#include "addr.h"

static const char *current_unix;
static int current_unix_fd;

static struct addrinfo *current_listen_addrinfo;
static int current_listen_fd;

void quit(ev_source *ev) {
  quitting(ev);
  trackdb_close();
  trackdb_deinit();
  info("terminating");
  _exit(0);
}

static void reset_socket(ev_source *ev) {
  const char *new_unix;
  struct addrinfo *res;
  struct sockaddr_un sun;
  char *name;
  
  static const struct addrinfo pref = {
    AI_PASSIVE,
    PF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    0,
    0,
    0,
    0
  };

  /* unix first */
  new_unix = config_get_file("socket");
  if(!current_unix || strcmp(current_unix, new_unix)) {
    /* either there was no socket, or there was but a different path */
    if(current_unix) {
      /* stop the old one and remove it from the filesystem */
      server_stop(ev, current_unix_fd);
      if(unlink(current_unix) < 0)
	fatal(errno, "unlink %s", current_unix);
    }
    /* start the new one */
    if(strlen(new_unix) >= sizeof sun.sun_path)
      fatal(0, "socket path %s is too long", new_unix);
    memset(&sun, 0, sizeof sun);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, new_unix);
    if(unlink(new_unix) < 0 && errno != ENOENT)
      fatal(errno, "unlink %s", new_unix);
    if((current_unix_fd = server_start(ev, PF_UNIX, sizeof sun,
				       (const struct sockaddr *)&sun,
				       new_unix)) >= 0) {
      current_unix = new_unix;
      if(chmod(new_unix, 0777) < 0)
	fatal(errno, "error calling chmod %s", new_unix);
    } else
      current_unix = 0;
  }

  /* get the new listen config */
  if(config->listen.n)
    res = get_address(&config->listen, &pref, &name);
  else
    res = 0;

  if((res && !current_listen_addrinfo)
     || (current_listen_addrinfo
	 && (!res
	     || addrinfocmp(res, current_listen_addrinfo)))) {
    /* something has to change */
    if(current_listen_addrinfo) {
      /* delete the old listener */
      server_stop(ev, current_listen_fd);
      freeaddrinfo(current_listen_addrinfo);
      current_listen_addrinfo = 0;
    }
    if(res) {
      /* start the new listener */
      if((current_listen_fd = server_start(ev, res->ai_family, res->ai_addrlen,
					   res->ai_addr, name)) >= 0) {
	current_listen_addrinfo = res;
	res = 0;
      }
    }
  }
  /* if res is still set it needs freeing */
  if(res)
    freeaddrinfo(res);
}

int reconfigure(ev_source *ev, int reload) {
  int need_another_rescan = 0;
  int ret = 0;

  D(("reconfigure(%d)", reload));
  if(reload) {
    need_another_rescan = trackdb_rescan_cancel();
    trackdb_close();
    if(config_read(1))
      ret = -1;
    else {
      /* Tell the speaker it needs to reload its config too. */
      speaker_reload();
      info("%s: installed new configuration", configfile);
    }
    trackdb_open(TRACKDB_NO_UPGRADE);
  } else
    /* We only allow for upgrade at startup */
    trackdb_open(TRACKDB_CAN_UPGRADE);
  if(need_another_rescan)
    trackdb_rescan(ev);
  if(!ret) {
    queue_read();
    recent_read();
    reset_socket(ev);
  }
  return ret;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
