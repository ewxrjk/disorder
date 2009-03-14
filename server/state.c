/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007-2009 Richard Kettlewell
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
/** @file server/state.c
 * @brief Global server state
 */
#include "disorder-server.h"

static const char *current_unix;
static int current_unix_fd;

/** @brief TCP listener definition */
struct listener {
  struct listener *next;
  struct sockaddr *sa;
  int fd;
};

/** @brief Current listeners */
static struct listener *listeners;

/** @brief Current audio API */
const struct uaudio *api;

void quit(ev_source *ev) {
  info("shutting down...");
  quitting(ev);
  trackdb_close();
  trackdb_deinit();
  info("exiting");
  exit(0);
}

static struct sockaddr *copy_sockaddr(const struct addrinfo *addr) {
  struct sockaddr *sa = xmalloc_noptr(addr->ai_addrlen);
  memcpy(sa, addr->ai_addr, addr->ai_addrlen);
  return sa;
}

static void reset_socket(ev_source *ev) {
  const char *new_unix;
  struct addrinfo *res, *r;
  struct listener *l, **ll;
  struct sockaddr_un sun;

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
  if(config->listen.af != -1)
    res = netaddress_resolve(&config->listen, 1, IPPROTO_TCP);
  else
    res = 0;

  /* Close any current listeners that aren't required any more */
  ll = &listeners;
  while((l = *ll)) {
    for(r = res; r; r = r->ai_next)
      if(!sockaddrcmp(r->ai_addr, l->sa))
	break;
    if(!r) {
      /* Didn't find a match, remove this one */
      server_stop(ev, l->fd);
      *ll = l->next;
    } else {
      /* This address is still wanted */
      ll = &l->next;
    }
  }

  /* Open any new listeners that are required */
  for(r = res; r; r = r->ai_next) {
    for(l = listeners; l; l = l->next)
      if(!sockaddrcmp(r->ai_addr, l->sa))
	break;
    if(!l) {
      /* Didn't find a match, need a new listener */
      int fd = server_start(ev, r->ai_family, r->ai_addrlen, r->ai_addr,
			    format_sockaddr(r->ai_addr));
      if(fd >= 0) {
	l = xmalloc(sizeof *l);
	l->next = listeners;
	l->sa = copy_sockaddr(r);
	l->fd = fd;
	listeners = l;
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
  if(api) {
    if(api->close_mixer)
      api->close_mixer();
    api = NULL;
  }
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
  api = uaudio_find(config->api);
  if(api->configure)
    api->configure();
  if(api->open_mixer)
    api->open_mixer();
  if(need_another_rescan)
    trackdb_rescan(ev, 1/*check*/, 0, 0);
  /* Arrange timeouts for schedule actions */
  schedule_init(ev);
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
