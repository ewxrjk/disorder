/*
 * This file is part of DisOrder.
 * Copyright (C) 2010 Richard Kettlewell
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
  return disorder_simple(c, 0, "adopt", id, (char *)0);
}

int disorder_adduser(disorder_client *c, const char *user, const char *password, const char *rights) {
  return disorder_simple(c, 0, "adduser", user, password, rights, (char *)0);
}

int disorder_allfiles(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  return disorder_simple_list(c, filesp, nfilesp, "allfiles", dir, re, (char *)0);
}

int disorder_confirm(disorder_client *c, const char *confirmation) {
  char *u;
  int rc;
  if((rc = disorder_simple(c, &u, "confirm", confirmation  )))
    return rc;
  c->user = u;
  return 0;
}

int disorder_cookie(disorder_client *c, const char *cookie) {
  char *u;
  int rc;
  if((rc = disorder_simple(c, &u, "cookie", cookie  )))
    return rc;
  c->user = u;
  return 0;
}

int disorder_deluser(disorder_client *c, const char *user) {
  return disorder_simple(c, 0, "deluser", user, (char *)0);
}

int disorder_dirs(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  return disorder_simple_list(c, filesp, nfilesp, "dirs", dir, re, (char *)0);
}

int disorder_disable(disorder_client *c) {
  return disorder_simple(c, 0, "disable", (char *)0);
}

int disorder_edituser(disorder_client *c, const char *username, const char *property, const char *value) {
  return disorder_simple(c, 0, "edituser", username, property, value, (char *)0);
}

int disorder_enable(disorder_client *c) {
  return disorder_simple(c, 0, "enable", (char *)0);
}

int disorder_enabled(disorder_client *c, int *enabledp) {
  char *v;
  int rc;
  if((rc = disorder_simple(c, &v, "enabled", (char *)0)))
    return rc;
  return boolean("enabled", v, enabledp);
}

int disorder_exists(disorder_client *c, const char *track, int *existsp) {
  char *v;
  int rc;
  if((rc = disorder_simple(c, &v, "exists", track, (char *)0)))
    return rc;
  return boolean("exists", v, existsp);
}

int disorder_files(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp) {
  return disorder_simple_list(c, filesp, nfilesp, "files", dir, re, (char *)0);
}

int disorder_get(disorder_client *c, const char *track, const char *pref, char **valuep) {
  return dequote(disorder_simple(c, valuep, "get", track, pref, (char *)0), valuep);
}

int disorder_get_global(disorder_client *c, const char *pref, char **valuep) {
  return dequote(disorder_simple(c, valuep, "get-global", pref, (char *)0), valuep);
}

int disorder_length(disorder_client *c, const char *track, long *lengthp) {
  char *v;
  int rc;

  if((rc = disorder_simple(c, &v, "length", track, (char *)0)))
    return rc;
  *lengthp = atol(v);
  xfree(v);
  return 0;
}

int disorder_make_cookie(disorder_client *c, char **cookiep) {
  return dequote(disorder_simple(c, cookiep, "make-cookie", (char *)0), cookiep);
}

int disorder_move(disorder_client *c, const char *track, long delta) {
  char buf_delta[16];
  byte_snprintf(buf_delta, sizeof buf_delta, "%ld", delta);
  return disorder_simple(c, 0, "move", track, buf_delta, (char *)0);
}

int disorder_moveafter(disorder_client *c, const char *target, char **ids, int nids) {
  return disorder_simple(c, 0, "moveafter", target, disorder_list, ids, nids, (char *)0);
}

int disorder_nop(disorder_client *c) {
  return disorder_simple(c, 0, "nop", (char *)0);
}

int disorder_part(disorder_client *c, const char *track, const char *context, const char *part, char **partp) {
  return dequote(disorder_simple(c, partp, "part", track, context, part, (char *)0), partp);
}

int disorder_pause(disorder_client *c) {
  return disorder_simple(c, 0, "pause", (char *)0);
}

int disorder_play(disorder_client *c, const char *track, char **idp) {
  return dequote(disorder_simple(c, idp, "play", track, (char *)0), idp);
}

int disorder_playafter(disorder_client *c, const char *target, char **tracks, int ntracks) {
  return disorder_simple(c, 0, "playafter", target, disorder_list, tracks, ntracks, (char *)0);
}

int disorder_playing(disorder_client *c, struct queue_entry **playingp) {
  return onequeue(c, "playing", playingp);
}

int disorder_playlist_delete(disorder_client *c, const char *playlist) {
  return disorder_simple(c, 0, "playlist-delete", playlist, (char *)0);
}

int disorder_playlist_get(disorder_client *c, const char *playlist, char ***tracksp, int *ntracksp) {
  return disorder_simple_list(c, tracksp, ntracksp, "playlist-get", playlist, (char *)0);
}

int disorder_playlist_get_share(disorder_client *c, const char *playlist, char **sharep) {
  return dequote(disorder_simple(c, sharep, "playlist-get-share", playlist, (char *)0), sharep);
}

int disorder_playlist_lock(disorder_client *c, const char *playlist) {
  return disorder_simple(c, 0, "playlist-lock", playlist, (char *)0);
}

int disorder_playlist_set(disorder_client *c, const char *playlist, char **tracks, int ntracks) {
  return disorder_simple(c, 0, "playlist-set", playlist, disorder_body, tracks, ntracks, (char *)0);
}

int disorder_playlist_set_share(disorder_client *c, const char *playlist, const char *share) {
  return disorder_simple(c, 0, "playlist-set-share", playlist, share, (char *)0);
}

int disorder_playlist_unlock(disorder_client *c) {
  return disorder_simple(c, 0, "playlist-unlock", (char *)0);
}

int disorder_playlists(disorder_client *c, char ***playlistsp, int *nplaylistsp) {
  return disorder_simple_list(c, playlistsp, nplaylistsp, "playlists", (char *)0);
}

int disorder_prefs(disorder_client *c, const char *track, struct kvp **prefsp) {
  return pairlist(c, prefsp, "prefs", track, (char *)0);
}

int disorder_queue(disorder_client *c, struct queue_entry **queuep) {
  return somequeue(c, "queue", queuep);
}

int disorder_random_disable(disorder_client *c) {
  return disorder_simple(c, 0, "random-disable", (char *)0);
}

int disorder_random_enable(disorder_client *c) {
  return disorder_simple(c, 0, "random-enable", (char *)0);
}

int disorder_random_enabled(disorder_client *c, int *enabledp) {
  char *v;
  int rc;
  if((rc = disorder_simple(c, &v, "random-enabled", (char *)0)))
    return rc;
  return boolean("random-enabled", v, enabledp);
}

int disorder_recent(disorder_client *c, struct queue_entry **recentp) {
  return somequeue(c, "recent", recentp);
}

int disorder_reconfigure(disorder_client *c) {
  return disorder_simple(c, 0, "reconfigure", (char *)0);
}

int disorder_register(disorder_client *c, const char *username, const char *password, const char *email, char **confirmationp) {
  return dequote(disorder_simple(c, confirmationp, "register", username, password, email, (char *)0), confirmationp);
}

int disorder_reminder(disorder_client *c, const char *username) {
  return disorder_simple(c, 0, "reminder", username, (char *)0);
}

int disorder_remove(disorder_client *c, const char *id) {
  return disorder_simple(c, 0, "remove", id, (char *)0);
}

int disorder_rescan(disorder_client *c) {
  return disorder_simple(c, 0, "rescan", (char *)0);
}

int disorder_resolve(disorder_client *c, const char *track, char **resolvedp) {
  return dequote(disorder_simple(c, resolvedp, "resolve", track, (char *)0), resolvedp);
}

int disorder_resume(disorder_client *c) {
  return disorder_simple(c, 0, "resume", (char *)0);
}

int disorder_revoke(disorder_client *c) {
  return disorder_simple(c, 0, "revoke", (char *)0);
}

int disorder_scratch(disorder_client *c, const char *id) {
  return disorder_simple(c, 0, "scratch", id, (char *)0);
}

int disorder_schedule_del(disorder_client *c, const char *event) {
  return disorder_simple(c, 0, "schedule-del", event, (char *)0);
}

int disorder_schedule_get(disorder_client *c, const char *id, struct kvp **actiondatap) {
  return pairlist(c, actiondatap, "schedule-get", id, (char *)0);
}

int disorder_schedule_list(disorder_client *c, char ***idsp, int *nidsp) {
  return disorder_simple_list(c, idsp, nidsp, "schedule-list", (char *)0);
}

int disorder_search(disorder_client *c, const char *terms, char ***tracksp, int *ntracksp) {
  return disorder_simple_list(c, tracksp, ntracksp, "search", terms, (char *)0);
}

int disorder_set(disorder_client *c, const char *track, const char *pref, const char *value) {
  return disorder_simple(c, 0, "set", track, pref, value, (char *)0);
}

int disorder_set_global(disorder_client *c, const char *pref, const char *value) {
  return disorder_simple(c, 0, "set-global", pref, value, (char *)0);
}

int disorder_shutdown(disorder_client *c) {
  return disorder_simple(c, 0, "shutdown", (char *)0);
}

int disorder_stats(disorder_client *c, char ***statsp, int *nstatsp) {
  return disorder_simple_list(c, statsp, nstatsp, "stats", (char *)0);
}

int disorder_tags(disorder_client *c, char ***tagsp, int *ntagsp) {
  return disorder_simple_list(c, tagsp, ntagsp, "tags", (char *)0);
}

int disorder_unset(disorder_client *c, const char *track, const char *pref) {
  return disorder_simple(c, 0, "unset", track, pref, (char *)0);
}

int disorder_unset_global(disorder_client *c, const char *pref) {
  return disorder_simple(c, 0, "unset-global", pref, (char *)0);
}

int disorder_userinfo(disorder_client *c, const char *username, const char *property, char **valuep) {
  return dequote(disorder_simple(c, valuep, "userinfo", username, property, (char *)0), valuep);
}

int disorder_users(disorder_client *c, char ***usersp, int *nusersp) {
  return disorder_simple_list(c, usersp, nusersp, "users", (char *)0);
}

int disorder_version(disorder_client *c, char **versionp) {
  return dequote(disorder_simple(c, versionp, "version", (char *)0), versionp);
}

