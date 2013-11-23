/*
 * Automatically generated file, see scripts/protocol
 *
 * DO NOT EDIT.
 */
/*
 * This file is part of DisOrder.
 * Copyright (C) 2010-11, 13 Richard Kettlewell
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
/** @file lib/client-stubs.c
 * @brief Generated client API implementation
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "confirm", confirmation, (char *)NULL);
  if(rc)
    return rc;
  c->user = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_cookie(disorder_client *c, const char *cookie) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "cookie", cookie, (char *)NULL);
  if(rc)
    return rc;
  c->user = v[0];
  v[0] = NULL;
  free_strings(nv, v);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "enabled", (char *)NULL);
  if(rc)
    return rc;
  if(boolean("enabled", v[0], enabledp))
    return -1;
  free_strings(nv, v);
  return 0;
}

int disorder_exists(disorder_client *c, const char *track, int *existsp) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "exists", track, (char *)NULL);
  if(rc)
    return rc;
  if(boolean("exists", v[0], existsp))
    return -1;
  free_strings(nv, v);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "get", track, pref, (char *)NULL);
  if(rc)
    return rc;
  *valuep = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_get_global(disorder_client *c, const char *pref, char **valuep) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "get-global", pref, (char *)NULL);
  if(rc)
    return rc;
  *valuep = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_length(disorder_client *c, const char *track, long *lengthp) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "length", track, (char *)NULL);
  if(rc)
    return rc;
  *lengthp = atol(v[0]);
  free_strings(nv, v);
  return 0;
}

int disorder_make_cookie(disorder_client *c, char **cookiep) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "make-cookie", (char *)NULL);
  if(rc)
    return rc;
  *cookiep = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_move(disorder_client *c, const char *track, long delta) {
  return disorder_simple(c, NULL, "move", track, disorder__integer, delta, (char *)NULL);
}

int disorder_moveafter(disorder_client *c, const char *target, char **ids, int nids) {
  return disorder_simple(c, NULL, "moveafter", target, disorder__list, ids, nids, (char *)NULL);
}

int disorder_new_tracks(disorder_client *c, long max, char ***tracksp, int *ntracksp) {
  int rc = disorder_simple(c, NULL, "new", disorder__integer, max, (char *)NULL);
  if(rc)
    return rc;
  if(readlist(c, tracksp, ntracksp))
    return -1;
  return 0;
}

int disorder_nop(disorder_client *c) {
  return disorder_simple(c, NULL, "nop", (char *)NULL);
}

int disorder_part(disorder_client *c, const char *track, const char *context, const char *namepart, char **partp) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "part", track, context, namepart, (char *)NULL);
  if(rc)
    return rc;
  *partp = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_pause(disorder_client *c) {
  return disorder_simple(c, NULL, "pause", (char *)NULL);
}

int disorder_play(disorder_client *c, const char *track, char **idp) {
  return disorder_simple(c, idp, "play", track, (char *)NULL);
}

int disorder_playafter(disorder_client *c, const char *target, char **tracks, int ntracks) {
  return disorder_simple(c, NULL, "playafter", target, disorder__list, tracks, ntracks, (char *)NULL);
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
  return disorder_simple(c, NULL, "playlist-set", playlist, disorder__body, tracks, ntracks, (char *)NULL);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "random-enabled", (char *)NULL);
  if(rc)
    return rc;
  if(boolean("random-enabled", v[0], enabledp))
    return -1;
  free_strings(nv, v);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "register", username, password, email, (char *)NULL);
  if(rc)
    return rc;
  *confirmationp = v[0];
  v[0] = NULL;
  free_strings(nv, v);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "resolve", track, (char *)NULL);
  if(rc)
    return rc;
  *resolvedp = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_resume(disorder_client *c) {
  return disorder_simple(c, NULL, "resume", (char *)NULL);
}

int disorder_revoke(disorder_client *c) {
  return disorder_simple(c, NULL, "revoke", (char *)NULL);
}

int disorder_rtp_address(disorder_client *c, char **addressp, char **portp) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 2, "rtp-address", (char *)NULL);
  if(rc)
    return rc;
  *addressp = v[0];
  v[0] = NULL;
  *portp = v[1];
  v[1] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_rtp_cancel(disorder_client *c) {
  return disorder_simple(c, NULL, "rtp-cancel", (char *)NULL);
}

int disorder_rtp_request(disorder_client *c, const char *address, const char *port) {
  return disorder_simple(c, NULL, "rtp-request", address, port, (char *)NULL);
}

int disorder_scratch(disorder_client *c, const char *id) {
  return disorder_simple(c, NULL, "scratch", id, (char *)NULL);
}

int disorder_schedule_add_play(disorder_client *c, time_t when, const char *priority, const char *track) {
  return disorder_simple(c, NULL, "schedule-add", disorder__time, when, priority, "play", track, (char *)NULL);
}

int disorder_schedule_add_set_global(disorder_client *c, time_t when, const char *priority, const char *pref, const char *value) {
  return disorder_simple(c, NULL, "schedule-add", disorder__time, when, priority, "set-global", pref, value, (char *)NULL);
}

int disorder_schedule_add_unset_global(disorder_client *c, time_t when, const char *priority, const char *pref) {
  return disorder_simple(c, NULL, "schedule-add", disorder__time, when, priority, "set-global", pref, (char *)NULL);
}

int disorder_schedule_del(disorder_client *c, const char *id) {
  return disorder_simple(c, NULL, "schedule-del", id, (char *)NULL);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "userinfo", username, property, (char *)NULL);
  if(rc)
    return rc;
  *valuep = v[0];
  v[0] = NULL;
  free_strings(nv, v);
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
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 1, "version", (char *)NULL);
  if(rc)
    return rc;
  *versionp = v[0];
  v[0] = NULL;
  free_strings(nv, v);
  return 0;
}

int disorder_set_volume(disorder_client *c, long left, long right) {
  return disorder_simple(c, NULL, "volume", disorder__integer, left, disorder__integer, right, (char *)NULL);
}

int disorder_get_volume(disorder_client *c, long *leftp, long *rightp) {
  char **v;
  int nv, rc = disorder_simple_split(c, &v, &nv, 2, "volume", (char *)NULL);
  if(rc)
    return rc;
  *leftp = atol(v[0]);
  *rightp = atol(v[1]);
  free_strings(nv, v);
  return 0;
}

