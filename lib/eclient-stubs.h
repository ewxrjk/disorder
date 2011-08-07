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
#ifndef ECLIENT_STUBS_H
#define ECLIENT_STUBS_H

/** @brief Adopt a track
 *
 * Makes the calling user owner of a randomly picked track.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param id Track ID
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_adopt(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v);

/** @brief Create a user
 *
 * Create a new user.  Requires the 'admin' right.  Email addresses etc must be filled in in separate commands.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param user New username
 * @param password Initial password
 * @param rights Initial rights (optional)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_adduser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *user, const char *password, const char *rights, void *v);

/** @brief List files and directories in a directory
 *
 * See 'files' and 'dirs' for more specific lists.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_allfiles(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v);

/** @brief Delete user
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param user User to delete
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_deluser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *user, void *v);

/** @brief List directories in a directory
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_dirs(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v);

/** @brief Disable play
 *
 * Play will stop at the end of the current track, if one is playing.  Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_disable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Set a user property
 *
 * With the 'admin' right you can do anything.  Otherwise you need the 'userinfo' right and can only set 'email' and 'password'.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param username User to modify
 * @param property Property name
 * @param value New property value
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_edituser(disorder_eclient *c, disorder_eclient_no_response *completed, const char *username, const char *property, const char *value, void *v);

/** @brief Enable play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_enable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Detect whether play is enabled
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_enabled(disorder_eclient *c, disorder_eclient_integer_response *completed, void *v);

/** @brief Test whether a track exists
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_exists(disorder_eclient *c, disorder_eclient_integer_response *completed, const char *track, void *v);

/** @brief List files in a directory
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_files(disorder_eclient *c, disorder_eclient_list_response *completed, const char *dir, const char *re, void *v);

/** @brief Get a track preference
 *
 * If the track does not exist that is an error.  If the track exists but the preference does not then a null value is returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param pref Preference name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_get(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, const char *pref, void *v);

/** @brief Get a global preference
 *
 * If the preference does exist not then a null value is returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param pref Global preference name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_get_global(disorder_eclient *c, disorder_eclient_string_response *completed, const char *pref, void *v);

/** @brief Get a track's length
 *
 * If the track does not exist an error is returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_length(disorder_eclient *c, disorder_eclient_integer_response *completed, const char *track, void *v);

/** @brief Create a login cookie for this user
 *
 * The cookie may be redeemed via the 'cookie' command
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_make_cookie(disorder_eclient *c, disorder_eclient_string_response *completed, void *v);

/** @brief Move a track
 *
 * Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track ID or name
 * @param delta How far to move the track towards the head of the queue
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_move(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, long delta, void *v);

/** @brief Move multiple tracks
 *
 * Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param target Move after this track, or to head if ""
 * @param ids List of tracks to move by ID
 * @param nids Length of ids
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_moveafter(disorder_eclient *c, disorder_eclient_no_response *completed, const char *target, char **ids, int nids, void *v);

/** @brief List recently added tracks
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param max Maximum tracks to fetch, or 0 for all available
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_new_tracks(disorder_eclient *c, disorder_eclient_list_response *completed, long max, void *v);

/** @brief Do nothing
 *
 * Used as a keepalive.  No authentication required.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_nop(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Get a track name part
 *
 * If the name part cannot be constructed an empty string is returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param context Context ("sort" or "display")
 * @param part Name part ("artist", "album" or "title")
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_part(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, const char *context, const char *part, void *v);

/** @brief Pause the currently playing track
 *
 * Requires the 'pause' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_pause(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Play a track
 *
 * Requires the 'play' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track to play
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_play(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, void *v);

/** @brief Play multiple tracks
 *
 * Requires the 'play' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param target Insert into queue after this track, or at head if ""
 * @param tracks List of track names to play
 * @param ntracks Length of tracks
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playafter(disorder_eclient *c, disorder_eclient_no_response *completed, const char *target, char **tracks, int ntracks, void *v);

/** @brief Retrieve the playing track
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playing(disorder_eclient *c, disorder_eclient_playing_response *completed, void *v);

/** @brief Delete a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist to delete
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_delete(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, void *v);

/** @brief List the contents of a playlist
 *
 * Requires the 'read' right and oermission to read the playlist.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_get(disorder_eclient *c, disorder_eclient_list_response *completed, const char *playlist, void *v);

/** @brief Get a playlist's sharing status
 *
 * Requires the 'read' right and permission to read the playlist.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist to read
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_get_share(disorder_eclient *c, disorder_eclient_string_response *completed, const char *playlist, void *v);

/** @brief Lock a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.  A given connection may lock at most one playlist.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist to delete
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_lock(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, void *v);

/** @brief Set the contents of a playlist
 *
 * Requires the 'play' right and permission to modify the playlist, which must be locked.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist to modify
 * @param tracks New list of tracks for playlist
 * @param ntracks Length of tracks
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_set(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, char **tracks, int ntracks, void *v);

/** @brief Set a playlist's sharing status
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param playlist Playlist to modify
 * @param share New sharing status ("public", "private" or "shared")
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_set_share(disorder_eclient *c, disorder_eclient_no_response *completed, const char *playlist, const char *share, void *v);

/** @brief Unlock the locked playlist playlist
 *
 * The playlist to unlock is implicit in the connection.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlist_unlock(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief List playlists
 *
 * Requires the 'read' right.  Only playlists that you have permission to read are returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_playlists(disorder_eclient *c, disorder_eclient_list_response *completed, void *v);

/** @brief List the queue
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_queue(disorder_eclient *c, disorder_eclient_queue_response *completed, void *v);

/** @brief Disable random play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_random_disable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Enable random play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_random_enable(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Detect whether random play is enabled
 *
 * Random play counts as enabled even if play is disabled.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_random_enabled(disorder_eclient *c, disorder_eclient_integer_response *completed, void *v);

/** @brief List recently played tracks
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_recent(disorder_eclient *c, disorder_eclient_queue_response *completed, void *v);

/** @brief Re-read configuraiton file.
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_reconfigure(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Register a new user
 *
 * Requires the 'register' right which is usually only available to the 'guest' user.  Redeem the confirmation string via 'confirm' to complete registration.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param username Requested new username
 * @param password Requested initial password
 * @param email New user's email address
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_register(disorder_eclient *c, disorder_eclient_string_response *completed, const char *username, const char *password, const char *email, void *v);

/** @brief Send a password reminder.
 *
 * If the user has no valid email address, or no password, or a reminder has been sent too recently, then no reminder will be sent.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param username User to remind
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_reminder(disorder_eclient *c, disorder_eclient_no_response *completed, const char *username, void *v);

/** @brief Remove a track form the queue.
 *
 * Requires one of the 'remove mine', 'remove random' or 'remove any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param id Track ID
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_remove(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v);

/** @brief Rescan all collections for new or obsolete tracks.
 *
 * Requires the 'rescan' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_rescan(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Resolve a track name
 *
 * Converts aliases to non-alias track names
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name (might be an alias)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_resolve(disorder_eclient *c, disorder_eclient_string_response *completed, const char *track, void *v);

/** @brief Resume the currently playing track
 *
 * Requires the 'pause' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_resume(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Revoke a cookie.
 *
 * It will not subsequently be possible to log in with the cookie.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_revoke(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Terminate the playing track.
 *
 * Requires one of the 'scratch mine', 'scratch random' or 'scratch any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param id Track ID (optional)
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_scratch(disorder_eclient *c, disorder_eclient_no_response *completed, const char *id, void *v);

/** @brief Schedule a track to play in the future
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param when When to play the track
 * @param priority Event priority ("normal" or "junk")
 * @param track Track to play
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_schedule_add_play(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *track, void *v);

/** @brief Schedule a global setting to be changed in the future
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param when When to change the setting
 * @param priority Event priority ("normal" or "junk")
 * @param pref Global preference to set
 * @param value New value of global preference
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_schedule_add_set_global(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *pref, const char *value, void *v);

/** @brief Schedule a global setting to be unset in the future
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param when When to change the setting
 * @param priority Event priority ("normal" or "junk")
 * @param pref Global preference to set
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_schedule_add_unset_global(disorder_eclient *c, disorder_eclient_no_response *completed, time_t when, const char *priority, const char *pref, void *v);

/** @brief Delete a scheduled event.
 *
 * Users can always delete their own scheduled events; with the admin right you can delete any event.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param event ID of event to delete
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_schedule_del(disorder_eclient *c, disorder_eclient_no_response *completed, const char *event, void *v);

/** @brief List scheduled events
 *
 * This just lists IDs.  Use 'schedule-get' to retrieve more detail
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_schedule_list(disorder_eclient *c, disorder_eclient_list_response *completed, void *v);

/** @brief Search for tracks
 *
 * Terms are either keywords or tags formatted as 'tag:TAG-NAME'.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param terms List of search terms
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_search(disorder_eclient *c, disorder_eclient_list_response *completed, const char *terms, void *v);

/** @brief Set a track preference
 *
 * Requires the 'prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param pref Preference name
 * @param value New value
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_set(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, const char *pref, const char *value, void *v);

/** @brief Set a global preference
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param pref Preference name
 * @param value New value
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_set_global(disorder_eclient *c, disorder_eclient_no_response *completed, const char *pref, const char *value, void *v);

/** @brief Request server shutdown
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_shutdown(disorder_eclient *c, disorder_eclient_no_response *completed, void *v);

/** @brief Get server statistics
 *
 * The details of what the server reports are not really defined.  The returned strings are intended to be printed out one to a line.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_stats(disorder_eclient *c, disorder_eclient_list_response *completed, void *v);

/** @brief Get a list of known tags
 *
 * Only tags which apply to at least one track are returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_tags(disorder_eclient *c, disorder_eclient_list_response *completed, void *v);

/** @brief Unset a track preference
 *
 * Requires the 'prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param track Track name
 * @param pref Preference name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_unset(disorder_eclient *c, disorder_eclient_no_response *completed, const char *track, const char *pref, void *v);

/** @brief Set a global preference
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param pref Preference name
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_unset_global(disorder_eclient *c, disorder_eclient_no_response *completed, const char *pref, void *v);

/** @brief Get a user property.
 *
 * If the user does not exist an error is returned, if the user exists but the property does not then a null value is returned.
 *
 * @param c Client
 * @param completed Called upon completion
 * @param username User to read
 * @param property Property to read
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_userinfo(disorder_eclient *c, disorder_eclient_string_response *completed, const char *username, const char *property, void *v);

/** @brief Get a list of users
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_users(disorder_eclient *c, disorder_eclient_list_response *completed, void *v);

/** @brief Get the server version
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_version(disorder_eclient *c, disorder_eclient_string_response *completed, void *v);

/** @brief Set the volume
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param left Left channel volume
 * @param right Right channel volume
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_set_volume(disorder_eclient *c, disorder_eclient_no_response *completed, long left, long right, void *v);

/** @brief Get the volume
 *
 * 
 *
 * @param c Client
 * @param completed Called upon completion
 * @param v Passed to @p completed
 * @return 0 if the command was queued successfuly, non-0 on error
 */
int disorder_eclient_get_volume(disorder_eclient *c, disorder_eclient_pair_integer_response *completed, void *v);

#endif
