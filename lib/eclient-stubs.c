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
 * @brief Generated asynchronous client API implementation
 */

int disorder_eclient_adopt(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "adopt", id, (char *)0);
}

int disorder_eclient_adduser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *user, const char *password, const char *rights, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "adduser", user, password, rights, (char *)0);
}

int disorder_eclient_allfiles(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "allfiles", dir, re, (char *)0);
}

int disorder_eclient_deluser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *user, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "deluser", user, (char *)0);
}

int disorder_eclient_dirs(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "dirs", dir, re, (char *)0);
}

int disorder_eclient_disable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "disable", (char *)0);
}

int disorder_eclient_edituser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *username, const char *property, const char *value, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "edituser", username, property, value, (char *)0);
}

int disorder_eclient_enable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "enable", (char *)0);
}

int disorder_eclient_enabled(disorder_eclient *c, disorder_eclient_integer_response *completed, void *v) {
  return simple(c, integer_response_opcallback, (void (*)())completed, v, "enabled", (char *)0);
}

int disorder_eclient_exists(disorder_eclient *c, disorder_eclient_integer_response *completed, const char *track, void *v) {
  return simple(c, integer_response_opcallback, (void (*)())completed, v, "exists", track, (char *)0);
}

int disorder_eclient_files(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "files", dir, re, (char *)0);
}

int disorder_eclient_get(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, const char *pref, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "get", track, pref, (char *)0);
}

int disorder_eclient_get_global(disorder_eclient *c, disorder_eclient_string_response *completed, const char *pref, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "get-global", pref, (char *)0);
}

int disorder_eclient_length(disorder_eclient *c, disorder_eclient_integer_response *completed, const char *track, void *v) {
  return simple(c, integer_response_opcallback, (void (*)())completed, v, "length", track, (char *)0);
}

int disorder_eclient_make_cookie(disorder_eclient *c, disorder_eclient_string_response *completed, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "make-cookie", (char *)0);
}

int disorder_eclient_move(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, long delta, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "move", track, disorder__integer, delta, (char *)0);
}

int disorder_eclient_moveafter(disorder_eclient *c, disorder_eclient_no_response *completed, const char *target, char **ids, int nids, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "moveafter", target, disorder__list, ids, nids, (char *)0);
}

int disorder_eclient_new_tracks(disorder_eclient *c, disorder_eclient_list_response *completed, long max, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "new", disorder__integer, max, (char *)0);
}

int disorder_eclient_nop(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "nop", (char *)0);
}

int disorder_eclient_part(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, const char *context, const char *namepart, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "part", track, context, namepart, (char *)0);
}

int disorder_eclient_pause(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "pause", (char *)0);
}

int disorder_eclient_play(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "play", track, (char *)0);
}

int disorder_eclient_playafter(disorder_eclient *c, disorder_eclient_no_response *completed, const char *target, char **tracks, int ntracks, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playafter", target, disorder__list, tracks, ntracks, (char *)0);
}

int disorder_eclient_playing(disorder_eclient *c, disorder_eclient_playing_response *completed, void *v) {
  return simple(c, playing_response_opcallback, (void (*)())completed, v, "playing", (char *)0);
}

int disorder_eclient_playlist_delete(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playlist-delete", playlist, (char *)0);
}

int disorder_eclient_playlist_get(disorder_eclient *c, disorder_eclient_list_response *completed, const char *playlist, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "playlist-get", playlist, (char *)0);
}

int disorder_eclient_playlist_get_share(disorder_eclient *c, disorder_eclient_string_response *completed, const char *playlist, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "playlist-get-share", playlist, (char *)0);
}

int disorder_eclient_playlist_lock(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playlist-lock", playlist, (char *)0);
}

int disorder_eclient_playlist_set(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, char **tracks, int ntracks, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playlist-set", playlist, disorder__body, tracks, ntracks, (char *)0);
}

int disorder_eclient_playlist_set_share(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, const char *share, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playlist-set-share", playlist, share, (char *)0);
}

int disorder_eclient_playlist_unlock(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "playlist-unlock", (char *)0);
}

int disorder_eclient_playlists(disorder_eclient *c, disorder_eclient_list_response *completed, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "playlists", (char *)0);
}

int disorder_eclient_queue(disorder_eclient *c, disorder_eclient_queue_response *completed, void *v) {
  return simple(c, queue_response_opcallback, (void (*)())completed, v, "queue", (char *)0);
}

int disorder_eclient_random_disable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "random-disable", (char *)0);
}

int disorder_eclient_random_enable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "random-enable", (char *)0);
}

int disorder_eclient_random_enabled(disorder_eclient *c, disorder_eclient_integer_response *completed, void *v) {
  return simple(c, integer_response_opcallback, (void (*)())completed, v, "random-enabled", (char *)0);
}

int disorder_eclient_recent(disorder_eclient *c, disorder_eclient_queue_response *completed, void *v) {
  return simple(c, queue_response_opcallback, (void (*)())completed, v, "recent", (char *)0);
}

int disorder_eclient_reconfigure(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "reconfigure", (char *)0);
}

int disorder_eclient_register(disorder_eclient *c, disorder_eclient_string_response *completed, const char *username, const char *password, const char *email, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "register", username, password, email, (char *)0);
}

int disorder_eclient_reminder(disorder_eclient *c, disorder_eclient_no_response *completed, const char *username, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "reminder", username, (char *)0);
}

int disorder_eclient_remove(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "remove", id, (char *)0);
}

int disorder_eclient_rescan(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "rescan", (char *)0);
}

int disorder_eclient_resolve(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "resolve", track, (char *)0);
}

int disorder_eclient_resume(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "resume", (char *)0);
}

int disorder_eclient_revoke(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "revoke", (char *)0);
}

int disorder_eclient_rtp_cancel(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "rtp-cancel", (char *)0);
}

int disorder_eclient_rtp_request(disorder_eclient *c, disorder_eclient_no_response *completed, const char *address, const char *port, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "rtp-request", address, port, (char *)0);
}

int disorder_eclient_scratch(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "scratch", id, (char *)0);
}

int disorder_eclient_schedule_add_play(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *track, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "schedule-add", disorder__time, when, priority, "play", track, (char *)0);
}

int disorder_eclient_schedule_add_set_global(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *pref, const char *value, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "schedule-add", disorder__time, when, priority, "set-global", pref, value, (char *)0);
}

int disorder_eclient_schedule_add_unset_global(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *pref, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "schedule-add", disorder__time, when, priority, "set-global", pref, (char *)0);
}

int disorder_eclient_schedule_del(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "schedule-del", id, (char *)0);
}

int disorder_eclient_schedule_list(disorder_eclient *c, disorder_eclient_list_response *completed, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "schedule-list", (char *)0);
}

int disorder_eclient_search(disorder_eclient *c, disorder_eclient_list_response *completed, const char *terms, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "search", terms, (char *)0);
}

int disorder_eclient_set(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, const char *pref, const char *value, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "set", track, pref, value, (char *)0);
}

int disorder_eclient_set_global(disorder_eclient *c, disorder_eclient_no_response *completed, const char *pref, const char *value, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "set-global", pref, value, (char *)0);
}

int disorder_eclient_shutdown(disorder_eclient *c, disorder_eclient_no_response *completed, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "shutdown", (char *)0);
}

int disorder_eclient_stats(disorder_eclient *c, disorder_eclient_list_response *completed, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "stats", (char *)0);
}

int disorder_eclient_tags(disorder_eclient *c, disorder_eclient_list_response *completed, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "tags", (char *)0);
}

int disorder_eclient_unset(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, const char *pref, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "unset", track, pref, (char *)0);
}

int disorder_eclient_unset_global(disorder_eclient *c, disorder_eclient_no_response *completed, const char *pref, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "unset-global", pref, (char *)0);
}

int disorder_eclient_userinfo(disorder_eclient *c, disorder_eclient_string_response *completed, const char *username, const char *property, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "userinfo", username, property, (char *)0);
}

int disorder_eclient_users(disorder_eclient *c, disorder_eclient_list_response *completed, void *v) {
  return simple(c, list_response_opcallback, (void (*)())completed, v, "users", (char *)0);
}

int disorder_eclient_version(disorder_eclient *c, disorder_eclient_string_response *completed, void *v) {
  return simple(c, string_response_opcallback, (void (*)())completed, v, "version", (char *)0);
}

int disorder_eclient_set_volume(disorder_eclient *c, disorder_eclient_no_response *completed, long left, long right, void *v) {
  return simple(c, no_response_opcallback, (void (*)())completed, v, "volume", disorder__integer, left, disorder__integer, right, (char *)0);
}

int disorder_eclient_get_volume(disorder_eclient *c, disorder_eclient_pair_integer_response *completed, void *v) {
  return simple(c, pair_integer_response_opcallback, (void (*)())completed, v, "volume", (char *)0);
}

