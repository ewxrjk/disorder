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

/** @brief A unix domain socket */
struct unix_socket {
  const char *path;
  int fd;
};

struct unix_socket main_socket[1], priv_socket[1];

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
  disorder_info("shutting down...");
  quitting(ev);
  trackdb_close();
  trackdb_deinit(ev);
  /* Shutdown subprocesses.
   *
   * Subprocesses that use ev_child:
   * - the speaker
   * - the current rescan
   * - any decoders
   * - ...and players
   * - the track picker
   * - mail sender
   * - the deadlock manager
   *
   * Subprocesses that don't:
   * - any normalizers
   * These are not shut down currently.
   */
  ev_child_killall(ev);
  disorder_info("exiting");
  exit(0);
}

/** @brief Create a copy of an @c addrinfo structure */
static struct sockaddr *copy_sockaddr(const struct addrinfo *addr) {
  struct sockaddr *sa = xmalloc_noptr(addr->ai_addrlen);
  memcpy(sa, addr->ai_addr, addr->ai_addrlen);
  return sa;
}

static void reset_unix_socket(ev_source *ev,
                              struct unix_socket *s,
                              const char *new_path,
                              int privileged) {
  struct sockaddr_un sun;
  if(!s->path || strcmp(s->path, new_path)) {
    /* either there was no socket, or there was but a different path */
    if(s->path) {
      /* stop the old one and remove it from the filesystem */
      server_stop(ev, s->fd);
      if(unlink(s->path) < 0)
	disorder_fatal(errno, "unlink %s", s->path);
    }
    /* start the new one */
    if(strlen(new_path) >= sizeof sun.sun_path)
      disorder_fatal(0, "socket path %s is too long", new_path);
    memset(&sun, 0, sizeof sun);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, new_path);
    if(unlink(new_path) < 0 && errno != ENOENT)
      disorder_fatal(errno, "unlink %s", new_path);
    if((s->fd = server_start(ev, PF_UNIX, sizeof sun,
                             (const struct sockaddr *)&sun,
                             new_path,
                             privileged)) >= 0) {
      s->path = new_path;
      if(chmod(new_path, 0777) < 0)     /* TODO */
	disorder_fatal(errno, "error calling chmod %s", new_path);
    } else
      s->path = 0;
  }
}

/** @brief Create and destroy sockets to set current configuration */
void reset_sockets(ev_source *ev) {
  struct addrinfo *res, *r;
  struct listener *l, **ll;
  const char *private_dir = config_get_file("private");
  
  /* create the private socket directory */
  unlink(private_dir);
  if(mkdir(private_dir, 0700) < 0 && errno != EEXIST)
    disorder_fatal(errno, "error creating %s", private_dir);

  /* unix first */
  reset_unix_socket(ev, main_socket, config_get_file("socket"), 0);
  reset_unix_socket(ev, priv_socket, config_get_file("private/socket"), 1);

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
			    format_sockaddr(r->ai_addr), 0);
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
 * @param ev Event loop
 * @param flags Flags
 * @return As config_read(); 0 on success, -1 if could not (re-)read config
 */
int reconfigure(ev_source *ev, unsigned flags) {
  int need_another_rescan = 0;
  int ret = 0;

  D(("reconfigure(%x)", flags));
  /* Deconfigure the old audio API if there is one */
  if(api) {
    if(api->close_mixer)
      api->close_mixer();
    api = NULL;
  }
  if(flags & RECONFIGURE_RELOADING) {
    /* If there's a rescan in progress, cancel it but remember to start a fresh
     * one after the reload. */
    need_another_rescan = trackdb_rescan_cancel();
    /* (Re-)read the configuration */
    if(config_read(1/*server*/, config))
      ret = -1;
    else {
      /* Tell the speaker it needs to reload its config too. */
      speaker_reload();
      disorder_info("%s: installed new configuration", configfile);
    }
  }
  /* New audio API */
  api = uaudio_find(config->api);
  if(api->configure)
    api->configure();
  if(api->open_mixer)
    api->open_mixer();
  /* If we interrupted a rescan of all the tracks, start a new one */
  if(need_another_rescan)
    trackdb_rescan(ev, 1/*check*/, 0, 0);
  if(!ret && !(flags & RECONFIGURE_FIRST)) {
    /* Open/close sockets */
    reset_sockets(ev);
  }
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
