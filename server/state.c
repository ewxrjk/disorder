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

/** @brief Current AF_UNIX socket path */
static const char *current_unix;

/** @brief Current AF_UNIX socket */
static int current_unix_fd;

/** @brief TCP listener definition */
struct listener {
  /** @brief Next listener */
  struct listener *next;

  /** @brief Local socket address */
  struct sockaddr *sa;

  /** @brief File descriptor */
  int fd;
};

/** @brief Current listeners */
static struct listener *listeners;

/** @brief Current audio API */
const struct uaudio *api;

/** @brief Quit DisOrder */
void quit(ev_source *ev) {
  info("shutting down...");
  quitting(ev);
  trackdb_close();
  trackdb_deinit();
  info("exiting");
  exit(0);
}

/** @brief Create a copy of an @c addrinfo structure */
static struct sockaddr *copy_sockaddr(const struct addrinfo *addr) {
  struct sockaddr *sa = xmalloc_noptr(addr->ai_addrlen);
  memcpy(sa, addr->ai_addr, addr->ai_addrlen);
  return sa;
}

/** @brief Create and destroy sockets to set current configuration */
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
      /* We ignore any failures (though server_start() will have
       * logged them). */
    }
  }
  /* if res is still set it needs freeing */
  if(res)
    freeaddrinfo(res);
}

/** @brief Reconfigure the server
 * @param reload 0 at startup, 1 for a reload
 */
int reconfigure(ev_source *ev, int reload) {
  int need_another_rescan = 0;
  int ret = 0;

  D(("reconfigure(%d)", reload));
  /* Deconfigure the old audio API if there is one */
  if(api) {
    if(api->close_mixer)
      api->close_mixer();
    api = NULL;
  }
  if(reload) {
    /* If there's a rescan in progress, cancel it but remember to start a fresh
     * one after the reload */
    need_another_rescan = trackdb_rescan_cancel();
    /* Close the track database */
    trackdb_close();
    /* (Re-)read the configuration */
    if(config_read(1/*server*/))
      ret = -1;
    else {
      /* Tell the speaker it needs to reload its config too. */
      speaker_reload();
      info("%s: installed new configuration", configfile);
    }
    /* Re-open track database.  We don't attempt to support database version
     * upgrade at this point - the software hasn't changed, after all. */
    trackdb_open(TRACKDB_NO_UPGRADE);
  } else
    /* We only allow for upgrade at startup */
    trackdb_open(TRACKDB_CAN_UPGRADE);
  /* New audio API */
  api = uaudio_find(config->api);
  if(api->configure)
    api->configure();
  if(api->open_mixer)
    api->open_mixer();
  /* If we interrupted a rescan of all the tracks, start a new one */
  if(need_another_rescan)
    trackdb_rescan(ev, 1/*check*/, 0, 0);
  /* Arrange timeouts for schedule actions */
  schedule_init(ev);
  /* TODO what about leftover callbacks from before the reload? */
  if(!ret) {
    /* Read the queue back in */
    queue_read();
    recent_read();
    /* Reset sockets */
    reset_socket(ev);
  }
  /* TODO we need a careful think about what you can and cannot change with a
   * reload.
   *
   * For instance saving and restoring the queue limits what we can usefuly
   * keep in the queue data structure.  As a general rule, long-term state
   * ought to be off-limits.  So 'home directory' will have to stay where it
   * is.  (This actually means the AF_UNIX socket will never be re-opened.)
   *
   * Players certainly ought to be reconfigurable but this raises the ugly
   * question of what to do about already-started background decoders.
   *
   * The audio API is easy to do for the server but a pain for the speaker.
   *
   * These two points suggest a general approach: if things that affect the
   * speaker change, we could just restart it and any extant background
   * decoders, at the next track change.  This would generate a bit of a gap
   * but presumably reconfiguration is a rare event so this might be
   * acceptable.
   */
  return ret;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
