/*
 * Automatically generated file, see scripts/protocol
 *
 * DO NOT EDIT.
 */
/*
 * This file is part of DisOrder.
 * Copyright (C) 2010-11 Richard Kettlewell
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

int disorder_adopt(disorder_client *c, const char *id) {
  return disorder_simple(c, NULL, "adopt", id, (char *)NULL);
}

int disorder_adduser(disorder_client *c, const char *user, const char *password, const char *rights) {
  return disorder_simple(c, NULL, "adduser", user, password, rights, (char *)NULL);
}

int disorder_allfiles(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  int rc = disorder_simple(c, NULL, "allfiles", dir, re, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, filesp, nfilesp))
    return -1;
  return 0;
}

int disorder_confirm(disorder_client *c, const char *confirmation) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "confirm", confirmation, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "confirm");
    return -1;
  }
  c->user = v[0];
  return 0;
}

int disorder_cookie(disorder_client *c, const char *cookie) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "cookie", cookie, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "cookie");
    return -1;
  }
  c->user = v[0];
  return 0;
}

int disorder_deluser(disorder_client *c, const char *user) {
  return disorder_simple(c, NULL, "deluser", user, (char *)NULL);
}

int disorder_dirs(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  int rc = disorder_simple(c, NULL, "dirs", dir, re, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, filesp, nfilesp))
    return -1;
  return 0;
}

int disorder_disable(disorder_client *c) {
  return disorder_simple(c, NULL, "disable", (char *)NULL);
}

int disorder_edituser(disorder_client *c, const char *username, const char *property, const char *value) {
  return disorder_simple(c, NULL, "edituser", username, property, value, (char *)NULL);
}

int disorder_enable(disorder_client *c) {
  return disorder_simple(c, NULL, "enable", (char *)NULL);
}

int disorder_enabled(disorder_client *c, int *enabledp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "enabled", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "enabled");
    return -1;
  }
  if(boolean("enabled", v[0], enabledp))
    return -1;
  return 0;
}

int disorder_exists(disorder_client *c, const char *track, int *existsp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "exists", track, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "exists");
    return -1;
  }
  if(boolean("exists", v[0], existsp))
    return -1;
  return 0;
}

int disorder_files(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  int rc = disorder_simple(c, NULL, "files", dir, re, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, filesp, nfilesp))
    return -1;
  return 0;
}

int disorder_get(disorder_client *c, const char *track, const char *pref, char **valuep) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "get", track, pref, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "get");
    return -1;
  }
  *valuep = v[0];
  return 0;
}

int disorder_get_global(disorder_client *c, const char *pref, char **valuep) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "get-global", pref, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "get-global");
    return -1;
  }
  *valuep = v[0];
  return 0;
}

int disorder_length(disorder_client *c, const char *track, long *lengthp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "length", track, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "length");
    return -1;
  }
  *lengthp = atol(v[0]);
  return 0;
}

int disorder_make_cookie(disorder_client *c, char **cookiep) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "make-cookie", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "make-cookie");
    return -1;
  }
  *cookiep = v[0];
  return 0;
}

int disorder_move(disorder_client *c, const char *track, long delta) {
  char buf_delta[16];
  byte_snprintf(buf_delta, sizeof buf_delta, "%ld", delta);
  return disorder_simple(c, NULL, "move", track, buf_delta, (char *)NULL);
}

int disorder_moveafter(disorder_client *c, const char *target, char **ids, int nids) {
  return disorder_simple(c, NULL, "moveafter", target, disorder_list, ids, nids, (char *)NULL);
}

int disorder_new_tracks(disorder_client *c, long max, char ***tracksp, int *ntracksp) {
  char buf_max[16];
  byte_snprintf(buf_max, sizeof buf_max, "%ld", max);
  int rc = disorder_simple(c, NULL, "new", buf_max, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, tracksp, ntracksp))
    return -1;
  return 0;
}

int disorder_nop(disorder_client *c) {
  return disorder_simple(c, NULL, "nop", (char *)NULL);
}

int disorder_part(disorder_client *c, const char *track, const char *context, const char *part, char **partp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "part", track, context, part, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "part");
    return -1;
  }
  *partp = v[0];
  return 0;
}

int disorder_pause(disorder_client *c) {
  return disorder_simple(c, NULL, "pause", (char *)NULL);
}

int disorder_play(disorder_client *c, const char *track, char **idp) {
  return disorder_simple(c, idp, "play", track, (char *)NULL);
}

int disorder_playafter(disorder_client *c, const char *target, char **tracks, int ntracks) {
  return disorder_simple(c, NULL, "playafter", target, disorder_list, tracks, ntracks, (char *)NULL);
}

int disorder_playing(disorder_client *c, struct queue_entry **playingp) {
  return onequeue(c, "playing", playingp);
}

int disorder_playlist_delete(disorder_client *c, const char *playlist) {
  return disorder_simple(c, NULL, "playlist-delete", playlist, (char *)NULL);
}

int disorder_playlist_get(disorder_client *c, const char *playlist, char ***tracksp, int *ntracksp) {
  int rc = disorder_simple(c, NULL, "playlist-get", playlist, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, tracksp, ntracksp))
    return -1;
  return 0;
}

int disorder_playlist_get_share(disorder_client *c, const char *playlist, char **sharep) {
  return disorder_simple(c, sharep, "playlist-get-share", playlist, (char *)NULL);
}

int disorder_playlist_lock(disorder_client *c, const char *playlist) {
  return disorder_simple(c, NULL, "playlist-lock", playlist, (char *)NULL);
}

int disorder_playlist_set(disorder_client *c, const char *playlist, char **tracks, int ntracks) {
  return disorder_simple(c, NULL, "playlist-set", playlist, disorder_body, tracks, ntracks, (char *)NULL);
}

int disorder_playlist_set_share(disorder_client *c, const char *playlist, const char *share) {
  return disorder_simple(c, NULL, "playlist-set-share", playlist, share, (char *)NULL);
}

int disorder_playlist_unlock(disorder_client *c) {
  return disorder_simple(c, NULL, "playlist-unlock", (char *)NULL);
}

int disorder_playlists(disorder_client *c, char ***playlistsp, int *nplaylistsp) {
  int rc = disorder_simple(c, NULL, "playlists", (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, playlistsp, nplaylistsp))
    return -1;
  return 0;
}

int disorder_prefs(disorder_client *c, const char *track, struct kvp **prefsp) {
  return pairlist(c, prefsp, "prefs", track, (char *)NULL);
}

int disorder_queue(disorder_client *c, struct queue_entry **queuep) {
  int rc = disorder_simple(c, NULL, "queue", (char *)NULL);
  if(rc)
    return rc;
  if(readqueue(c, queuep))
    return -1;
  return 0;
}

int disorder_random_disable(disorder_client *c) {
  return disorder_simple(c, NULL, "random-disable", (char *)NULL);
}

int disorder_random_enable(disorder_client *c) {
  return disorder_simple(c, NULL, "random-enable", (char *)NULL);
}

int disorder_random_enabled(disorder_client *c, int *enabledp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "random-enabled", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "random-enabled");
    return -1;
  }
  if(boolean("random-enabled", v[0], enabledp))
    return -1;
  return 0;
}

int disorder_recent(disorder_client *c, struct queue_entry **recentp) {
  int rc = disorder_simple(c, NULL, "recent", (char *)NULL);
  if(rc)
    return rc;
  if(readqueue(c, recentp))
    return -1;
  return 0;
}

int disorder_reconfigure(disorder_client *c) {
  return disorder_simple(c, NULL, "reconfigure", (char *)NULL);
}

int disorder_register(disorder_client *c, const char *username, const char *password, const char *email, char **confirmationp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "register", username, password, email, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "register");
    return -1;
  }
  *confirmationp = v[0];
  return 0;
}

int disorder_reminder(disorder_client *c, const char *username) {
  return disorder_simple(c, NULL, "reminder", username, (char *)NULL);
}

int disorder_remove(disorder_client *c, const char *id) {
  return disorder_simple(c, NULL, "remove", id, (char *)NULL);
}

int disorder_rescan(disorder_client *c) {
  return disorder_simple(c, NULL, "rescan", (char *)NULL);
}

int disorder_resolve(disorder_client *c, const char *track, char **resolvedp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "resolve", track, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "resolve");
    return -1;
  }
  *resolvedp = v[0];
  return 0;
}

int disorder_resume(disorder_client *c) {
  return disorder_simple(c, NULL, "resume", (char *)NULL);
}

int disorder_revoke(disorder_client *c) {
  return disorder_simple(c, NULL, "revoke", (char *)NULL);
}

int disorder_rtp_address(disorder_client *c, char **addressp, char **portp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "rtp-address", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 2) {
    disorder_error(0, "malformed reply to %s", "rtp-address");
    return -1;
  }
  *addressp = v[0];
  *portp = v[1];
  return 0;
}

int disorder_scratch(disorder_client *c, const char *id) {
  return disorder_simple(c, NULL, "scratch", id, (char *)NULL);
}

int disorder_schedule_del(disorder_client *c, const char *event) {
  return disorder_simple(c, NULL, "schedule-del", event, (char *)NULL);
}

int disorder_schedule_get(disorder_client *c, const char *id, struct kvp **actiondatap) {
  return pairlist(c, actiondatap, "schedule-get", id, (char *)NULL);
}

int disorder_schedule_list(disorder_client *c, char ***idsp, int *nidsp) {
  int rc = disorder_simple(c, NULL, "schedule-list", (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, idsp, nidsp))
    return -1;
  return 0;
}

int disorder_search(disorder_client *c, const char *terms, char ***tracksp, int *ntracksp) {
  int rc = disorder_simple(c, NULL, "search", terms, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, tracksp, ntracksp))
    return -1;
  return 0;
}

int disorder_set(disorder_client *c, const char *track, const char *pref, const char *value) {
  return disorder_simple(c, NULL, "set", track, pref, value, (char *)NULL);
}

int disorder_set_global(disorder_client *c, const char *pref, const char *value) {
  return disorder_simple(c, NULL, "set-global", pref, value, (char *)NULL);
}

int disorder_shutdown(disorder_client *c) {
  return disorder_simple(c, NULL, "shutdown", (char *)NULL);
}

int disorder_stats(disorder_client *c, char ***statsp, int *nstatsp) {
  int rc = disorder_simple(c, NULL, "stats", (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, statsp, nstatsp))
    return -1;
  return 0;
}

int disorder_tags(disorder_client *c, char ***tagsp, int *ntagsp) {
  int rc = disorder_simple(c, NULL, "tags", (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, tagsp, ntagsp))
    return -1;
  return 0;
}

int disorder_unset(disorder_client *c, const char *track, const char *pref) {
  return disorder_simple(c, NULL, "unset", track, pref, (char *)NULL);
}

int disorder_unset_global(disorder_client *c, const char *pref) {
  return disorder_simple(c, NULL, "unset-global", pref, (char *)NULL);
}

int disorder_userinfo(disorder_client *c, const char *username, const char *property, char **valuep) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "userinfo", username, property, (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "userinfo");
    return -1;
  }
  *valuep = v[0];
  return 0;
}

int disorder_users(disorder_client *c, char ***usersp, int *nusersp) {
  int rc = disorder_simple(c, NULL, "users", (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, usersp, nusersp))
    return -1;
  return 0;
}

int disorder_version(disorder_client *c, char **versionp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "version", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 1) {
    disorder_error(0, "malformed reply to %s", "version");
    return -1;
  }
  *versionp = v[0];
  return 0;
}

int disorder_set_volume(disorder_client *c, long left, long right) {
  char buf_left[16];
  byte_snprintf(buf_left, sizeof buf_left, "%ld", left);
  char buf_right[16];
  byte_snprintf(buf_right, sizeof buf_right, "%ld", right);
  return disorder_simple(c, NULL, "volume", buf_left, buf_right, (char *)NULL);
}

int disorder_get_volume(disorder_client *c, long *leftp, long *rightp) {
  char **v, *r;
  int nv;
  int rc = disorder_simple(c, &r, "volume", (char *)NULL);
  if(rc)
    return rc;
  v = split(r, &nv, SPLIT_QUOTES, 0, 0);
  if(nv != 2) {
    disorder_error(0, "malformed reply to %s", "volume");
    return -1;
  }
  *leftp = atol(v[0]);
  *rightp = atol(v[1]);
  return 0;
}

