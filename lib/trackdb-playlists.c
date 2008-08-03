/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file lib/trackdb-playlists.c
 * @brief Track database playlist support
 *
 * This file implements reading and modification of playlists, including access
 * control, but not locking or event logging (at least yet).
 */
#include "common.h"

#include <errno.h>

#include "trackdb-int.h"
#include "mem.h"
#include "log.h"
#include "configuration.h"
#include "vector.h"

static int trackdb_playlist_get_tid(const char *name,
                                    const char *who,
                                    char ***tracksp,
                                    int *ntracksp,
                                    char **sharep,
                                    DB_TXN *tid);
static int trackdb_playlist_set_tid(const char *name,
                                    const char *who,
                                    char **tracks,
                                    int ntracks,
                                    const char *share,
                                    DB_TXN *tid);
static int trackdb_playlist_list_tid(const char *who,
                                     char ***playlistsp,
                                     int *nplaylistsp,
                                     DB_TXN *tid);
static int trackdb_playlist_delete_tid(const char *name,
                                       const char *who,
                                       DB_TXN *tid);

/** @brief Parse a playlist name
 * @param name Playlist name
 * @param ownerp Where to put owner, or NULL
 * @param sharep Where to put default sharing, or NULL
 * @return 0 on success, -1 on error
 *
 * Playlists take the form USER.PLAYLIST or just PLAYLIST.  The PLAYLIST part
 * is alphanumeric and nonempty.  USER is a username (see valid_username()).
 */
int playlist_parse_name(const char *name,
                        char **ownerp,
                        char **sharep) {
  const char *dot = strchr(name, '.'), *share;
  char *owner;

  if(dot) {
    /* Owned playlist */
    owner = xstrndup(name, dot - name);
    if(!valid_username(owner))
      return -1;
    if(!valid_username(dot + 1))
      return -1;
    share = "private";
  } else {
    /* Shared playlist */
    if(!valid_username(name))
      return -1;
    owner = 0;
    share = "shared";
  }
  if(ownerp)
    *ownerp = owner;
  if(sharep)
    *sharep = xstrdup(share);
  return 0;
}

/** @brief Check read access rights
 * @param name Playlist name
 * @param who Who wants to read
 * @param share Playlist share status
 */
static int playlist_may_read(const char *name,
                             const char *who,
                             const char *share) {
  char *owner;
  
  if(playlist_parse_name(name, &owner, 0))
    return 0;
  /* Anyone can read shared playlists */
  if(!owner)
    return 1;
  /* You can always read playlists you own */
  if(!strcmp(owner, who))
    return 1;
  /* You can read public playlists */
  if(!strcmp(share, "public"))
    return 1;
  /* Anything else is prohibited */
  return 0;
}

/** @brief Check modify access rights
 * @param name Playlist name
 * @param who Who wants to modify
 * @param share Playlist share status
 */
static int playlist_may_write(const char *name,
                              const char *who,
                              const char attribute((unused)) *share) {
  char *owner;
  
  if(playlist_parse_name(name, &owner, 0))
    return 0;
  /* Anyone can modify shared playlists */
  if(!owner)
    return 1;
  /* You can always modify playlists you own */
  if(!strcmp(owner, who))
    return 1;
  /* Anything else is prohibited */
  return 0;
}

/** @brief Get playlist data
 * @param name Name of playlist
 * @param who Who wants to know
 * @param tracksp Where to put list of tracks, or NULL
 * @param ntracksp Where to put count of tracks, or NULL
 * @param sharep Where to put sharing type, or NULL
 * @return 0 on success, non-0 on error
 *
 * Possible return values:
 * - @c 0 on success
 * - @c ENOENT if the playlist doesn't exist
 * - @c EINVAL if the playlist name is invalid
 * - @c EACCES if the playlist cannot be read by @p who
 */
int trackdb_playlist_get(const char *name,
                         const char *who,
                         char ***tracksp,
                         int *ntracksp,
                         char **sharep) {
  int e;

  if(playlist_parse_name(name, 0, 0)) {
    error(0, "invalid playlist name '%s'", name);
    return EINVAL;
  }
  WITH_TRANSACTION(trackdb_playlist_get_tid(name, who,
                                            tracksp, ntracksp, sharep,
                                            tid));
  /* Don't expose libdb error codes too much */
  if(e == DB_NOTFOUND)
    e = ENOENT;
  return e;
}

static int trackdb_playlist_get_tid(const char *name,
                                    const char *who,
                                    char ***tracksp,
                                    int *ntracksp,
                                    char **sharep,
                                    DB_TXN *tid) {
  struct kvp *k;
  int e, ntracks;
  const char *s;

  if((e = trackdb_getdata(trackdb_playlistsdb, name, &k, tid)))
    return e;
  /* Get sharability */
  if(!(s = kvp_get(k, "sharing"))) {
    error(0, "playlist '%s' has no 'sharing' key", name);
    s = "private";
  }
  /* Check the read is allowed */
  if(!playlist_may_read(name, who, s))
    return EACCES;
  /* Return sharability */
  if(sharep)
    *sharep = xstrdup(s);
  /* Get track count */
  if(!(s = kvp_get(k, "count"))) {
    error(0, "playlist '%s' has no 'count' key", name);
    s = "0";
  }
  ntracks = atoi(s);
  if(ntracks < 0) {
    error(0, "playlist '%s' has negative count", name);
    ntracks = 0;
  }
  /* Return track count */
  if(ntracksp)
    *ntracksp = ntracks;
  if(tracksp) {
    /* Get track list */
    char **tracks = xcalloc(ntracks + 1, sizeof (char *));
    char b[16];

    for(int n = 0; n < ntracks; ++n) {
      snprintf(b, sizeof b, "%d", n);
      if(!(s = kvp_get(k, b))) {
        error(0, "playlist '%s' lacks track %d", name, n);
        s = "unknown";
      }
      tracks[n] = xstrdup(s);
    }
    tracks[ntracks] = 0;
    /* Return track list */
    *tracksp = tracks;
  }
  return 0;
}

/** @brief Modify or create a playlist
 * @param name Playlist name
 * @param tracks List of tracks to set, or NULL to leave alone
 * @param ntracks Length of @p tracks
 * @param share Sharing status, or NULL to leave alone
 * @return 0 on success, non-0 on error
 *
 * If the playlist exists it is just modified.
 *
 * If the playlist does not exist it is created.  The default set of tracks is
 * none, and the default sharing is private (if it is an owned one) or shared
 * (otherwise).
 *
 * If neither @c tracks nor @c share are set then we only do an access check.
 * The database is never modified (even to create the playlist) in this
 * situation.
 *
 * Possible return values:
 * - @c 0 on success
 * - @c EINVAL if the playlist name is invalid
 * - @c EACCES if the playlist cannot be modified by @p who
 */
int trackdb_playlist_set(const char *name,
                         const char *who,
                         char **tracks,
                         int ntracks,
                         const char *share) {
  int e;
  char *owner;
  
  if(playlist_parse_name(name, &owner, 0)) {
    error(0, "invalid playlist name '%s'", name);
    return EINVAL;
  }
  /* Check valid share types */
  if(share) {
    if(owner) {
      /* Playlists with an owner must be public or private */
      if(strcmp(share, "public")
         && strcmp(share, "private")) {
        error(0, "playlist '%s' must be public or private", name);
        return EINVAL;
      }
    } else {
      /* Playlists with no owner must be shared */
      if(strcmp(share, "shared")) {
        error(0, "playlist '%s' must be shared", name);
        return EINVAL;
      }
    }        
  }
  /* We've checked as much as we can for now, now go and attempt the change */
  WITH_TRANSACTION(trackdb_playlist_set_tid(name, who, tracks, ntracks, share,
                                            tid));
  return e;
}

static int trackdb_playlist_set_tid(const char *name,
                                    const char *who,
                                    char **tracks,
                                    int ntracks,
                                    const char *share,
                                    DB_TXN *tid) {
  struct kvp *k;
  int e;
  const char *s;

  if((e = trackdb_getdata(trackdb_playlistsdb, name, &k, tid))
     && e != DB_NOTFOUND)
    return e;
  /* If the playlist doesn't exist set some defaults */
  if(e == DB_NOTFOUND) {
    char *defshare, *owner;

    if(playlist_parse_name(name, &owner, &defshare))
      return EINVAL;
    /* Can't create a non-shared playlist belonging to someone else.  In fact
     * this should be picked up by playlist_may_write() below but it's clearer
     * to do it here. */
    if(owner && strcmp(owner, who))
      return EACCES;
    k = 0;
    kvp_set(&k, "count", 0);
    kvp_set(&k, "sharing", defshare);
  }
  /* Check that the modification is allowed */
  if(!(s = kvp_get(k, "sharing"))) {
    error(0, "playlist '%s' has no 'sharing' key", name);
    s = "private";
  }
  if(!playlist_may_write(name, who, s))
    return EACCES;
  /* If no change was requested then don't even create */
  if(!share && !tracks)
    return 0;
  /* Set the new values */
  if(share)
    kvp_set(&k, "sharing", share);
  if(tracks) {
    char b[16];
    int oldcount, n;

    /* Sanity check track count */
    if(ntracks < 0 || ntracks > config->playlist_max) {
      error(0, "invalid track count %d", ntracks);
      return EINVAL;
    }
    /* Set the tracks */
    for(n = 0; n < ntracks; ++n) {
      snprintf(b, sizeof b, "%d", n);
      kvp_set(&k, b, tracks[n]);
    }
    /* Get the old track count */
    if((s = kvp_get(k, "count")))
      oldcount = atoi(s);
    else
      oldcount = 0;
    /* Delete old slots */
    for(; n < oldcount; ++n) {
      snprintf(b, sizeof b, "%d", n);
      kvp_set(&k, b, NULL);
    }
    /* Set the new count */
    snprintf(b, sizeof b, "%d", ntracks);
    kvp_set(&k, "count", b);
  }
  /* Store the resulting record */
  return trackdb_putdata(trackdb_playlistsdb, name, k, tid, 0);
}

/** @brief Get a list of playlists
 * @param who Who wants to know
 * @param playlistsp Where to put list of playlists
 * @param nplaylistsp Where to put count of playlists, or NULL
 */
void trackdb_playlist_list(const char *who,
                           char ***playlistsp,
                           int *nplaylistsp) {
  int e;

  WITH_TRANSACTION(trackdb_playlist_list_tid(who, playlistsp, nplaylistsp,
                                             tid));
}

static int trackdb_playlist_list_tid(const char *who,
                                     char ***playlistsp,
                                     int *nplaylistsp,
                                     DB_TXN *tid) {
  struct vector v[1];
  DBC *c;
  DBT k[1], d[1];
  int e;

  vector_init(v);
  c = trackdb_opencursor(trackdb_playlistsdb, tid);
  memset(k, 0, sizeof k);
  while(!(e = c->c_get(c, k, prepare_data(d), DB_NEXT))) {
    char *name = xstrndup(k->data, k->size), *owner;
    const char *share = kvp_get(kvp_urldecode(d->data, d->size),
                                "sharing");

    /* Extract owner; malformed names are skipped */
    if(playlist_parse_name(name, &owner, 0)) {
      error(0, "invalid playlist name '%s' found in database", name);
      continue;
    }
    if(!share) {
      error(0, "playlist '%s' has no 'sharing' key", name);
      continue;
    }
    /* Always list public and shared playlists
     * Only list private ones to their owner
     * Don't list anything else
     */
    if(!strcmp(share, "public")
       || !strcmp(share, "shared")
       || (!strcmp(share, "private")
           && owner && !strcmp(owner, who)))
      vector_append(v, name);
  }
  trackdb_closecursor(c);
  switch(e) {
  case DB_NOTFOUND:
    break;
  case DB_LOCK_DEADLOCK:
    return e;
  default:
    fatal(0, "c->c_get: %s", db_strerror(e));
  }
  vector_terminate(v);
  if(playlistsp)
    *playlistsp = v->vec;
  if(nplaylistsp)
    *nplaylistsp = v->nvec;
  return 0;
}

/** @brief Delete a playlist
 * @param name Playlist name
 * @param who Who is deleting it
 * @return 0 on success, non-0 on error
 *
 * Possible return values:
 * - @c 0 on success
 * - @c EINVAL if the playlist name is invalid
 * - @c EACCES if the playlist cannot be modified by @p who
 * - @c ENOENT if the playlist doesn't exist
 */
int trackdb_playlist_delete(const char *name,
                            const char *who) {
  int e;
  char *owner;
  
  if(playlist_parse_name(name, &owner, 0)) {
    error(0, "invalid playlist name '%s'", name);
    return EINVAL;
  }
  /* We've checked as much as we can for now, now go and attempt the change */
  WITH_TRANSACTION(trackdb_playlist_delete_tid(name, who, tid));
  if(e == DB_NOTFOUND)
    e = ENOENT;
  return e;
}

static int trackdb_playlist_delete_tid(const char *name,
                                       const char *who,
                                       DB_TXN *tid) {
  struct kvp *k;
  int e;
  const char *s;

  if((e = trackdb_getdata(trackdb_playlistsdb, name, &k, tid)))
    return e;
  /* Check that modification is allowed */
  if(!(s = kvp_get(k, "sharing"))) {
    error(0, "playlist '%s' has no 'sharing' key", name);
    s = "private";
  }
  if(!playlist_may_write(name, who, s))
    return EACCES;
  /* Delete the playlist */
  return trackdb_delkey(trackdb_playlistsdb, name, tid);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
