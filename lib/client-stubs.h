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
#ifndef CLIENT_STUBS_H
#define CLIENT_STUBS_H

/** @brief Adopt a track
 *
 * Makes the calling user owner of a randomly picked track.
 *
 * @param c Client
 * @param id Track ID
 * @return 0 on success, non-0 on error
 */
int disorder_adopt(disorder_client *c, const char *id);

/** @brief Create a user
 *
 * Create a new user.  Requires the 'admin' right.  Email addresses etc must be filled in in separate commands.
 *
 * @param c Client
 * @param user New username
 * @param password Initial password
 * @param rights Initial rights (optional)
 * @return 0 on success, non-0 on error
 */
int disorder_adduser(disorder_client *c, const char *user, const char *password, const char *rights);

/** @brief List files and directories in a directory
 *
 * See 'files' and 'dirs' for more specific lists.
 *
 * @param c Client
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param filesp List of matching files and directories
 * @param nfilesp Number of elements in filesp
 * @return 0 on success, non-0 on error
 */
int disorder_allfiles(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp);

/** @brief Confirm registration
 *
 * The confirmation string must have been created with 'register'.  The username is returned so the caller knows who they are.
 *
 * @param c Client
 * @param confirmation Confirmation string
 * @return 0 on success, non-0 on error
 */
int disorder_confirm(disorder_client *c, const char *confirmation);

/** @brief Log in with a cookie
 *
 * The cookie must have been created with 'make-cookie'.  The username is returned so the caller knows who they are.
 *
 * @param c Client
 * @param cookie Cookie string
 * @return 0 on success, non-0 on error
 */
int disorder_cookie(disorder_client *c, const char *cookie);

/** @brief Delete user
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @param user User to delete
 * @return 0 on success, non-0 on error
 */
int disorder_deluser(disorder_client *c, const char *user);

/** @brief List directories in a directory
 *
 * 
 *
 * @param c Client
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param filesp List of matching directories
 * @param nfilesp Number of elements in filesp
 * @return 0 on success, non-0 on error
 */
int disorder_dirs(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp);

/** @brief Disable play
 *
 * Play will stop at the end of the current track, if one is playing.  Requires the 'global prefs' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_disable(disorder_client *c);

/** @brief Set a user property
 *
 * With the 'admin' right you can do anything.  Otherwise you need the 'userinfo' right and can only set 'email' and 'password'.
 *
 * @param c Client
 * @param username User to modify
 * @param property Property name
 * @param value New property value
 * @return 0 on success, non-0 on error
 */
int disorder_edituser(disorder_client *c, const char *username, const char *property, const char *value);

/** @brief Enable play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_enable(disorder_client *c);

/** @brief Detect whether play is enabled
 *
 * 
 *
 * @param c Client
 * @param enabledp 1 if play is enabled and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_enabled(disorder_client *c, int *enabledp);

/** @brief Test whether a track exists
 *
 * 
 *
 * @param c Client
 * @param track Track name
 * @param existsp 1 if the track exists and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_exists(disorder_client *c, const char *track, int *existsp);

/** @brief List files in a directory
 *
 * 
 *
 * @param c Client
 * @param dir Directory to list (optional)
 * @param re Regexp that results must match (optional)
 * @param filesp List of matching files
 * @param nfilesp Number of elements in filesp
 * @return 0 on success, non-0 on error
 */
int disorder_files(disorder_client *c, const char *dir, const char *re, char ***filesp, int *nfilesp);

/** @brief Get a track preference
 *
 * If the track does not exist that is an error.  If the track exists but the preference does not then a null value is returned.
 *
 * @param c Client
 * @param track Track name
 * @param pref Preference name
 * @param valuep Preference value
 * @return 0 on success, non-0 on error
 */
int disorder_get(disorder_client *c, const char *track, const char *pref, char **valuep);

/** @brief Get a global preference
 *
 * If the preference does exist not then a null value is returned.
 *
 * @param c Client
 * @param pref Global preference name
 * @param valuep Preference value
 * @return 0 on success, non-0 on error
 */
int disorder_get_global(disorder_client *c, const char *pref, char **valuep);

/** @brief Get a track's length
 *
 * If the track does not exist an error is returned.
 *
 * @param c Client
 * @param track Track name
 * @param lengthp Track length in seconds
 * @return 0 on success, non-0 on error
 */
int disorder_length(disorder_client *c, const char *track, long *lengthp);

/** @brief Create a login cookie for this user
 *
 * The cookie may be redeemed via the 'cookie' command
 *
 * @param c Client
 * @param cookiep Newly created cookie
 * @return 0 on success, non-0 on error
 */
int disorder_make_cookie(disorder_client *c, char **cookiep);

/** @brief Move a track
 *
 * Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param track Track ID or name
 * @param delta How far to move the track towards the head of the queue
 * @return 0 on success, non-0 on error
 */
int disorder_move(disorder_client *c, const char *track, long delta);

/** @brief Move multiple tracks
 *
 * Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param target Move after this track, or to head if ""
 * @param ids List of tracks to move by ID
 * @param nids Length of ids
 * @return 0 on success, non-0 on error
 */
int disorder_moveafter(disorder_client *c, const char *target, char **ids, int nids);

/** @brief List recently added tracks
 *
 * 
 *
 * @param c Client
 * @param max Maximum tracks to fetch, or 0 for all available
 * @param tracksp Recently added tracks
 * @param ntracksp Number of elements in tracksp
 * @return 0 on success, non-0 on error
 */
int disorder_new_tracks(disorder_client *c, long max, char ***tracksp, int *ntracksp);

/** @brief Do nothing
 *
 * Used as a keepalive.  No authentication required.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_nop(disorder_client *c);

/** @brief Get a track name part
 *
 * If the name part cannot be constructed an empty string is returned.
 *
 * @param c Client
 * @param track Track name
 * @param context Context ("sort" or "display")
 * @param part Name part ("artist", "album" or "title")
 * @param partp Value of name part
 * @return 0 on success, non-0 on error
 */
int disorder_part(disorder_client *c, const char *track, const char *context, const char *part, char **partp);

/** @brief Pause the currently playing track
 *
 * Requires the 'pause' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_pause(disorder_client *c);

/** @brief Play a track
 *
 * Requires the 'play' right.
 *
 * @param c Client
 * @param track Track to play
 * @param idp Queue ID of new track
 * @return 0 on success, non-0 on error
 */
int disorder_play(disorder_client *c, const char *track, char **idp);

/** @brief Play multiple tracks
 *
 * Requires the 'play' right.
 *
 * @param c Client
 * @param target Insert into queue after this track, or at head if ""
 * @param tracks List of track names to play
 * @param ntracks Length of tracks
 * @return 0 on success, non-0 on error
 */
int disorder_playafter(disorder_client *c, const char *target, char **tracks, int ntracks);

/** @brief Retrieve the playing track
 *
 * 
 *
 * @param c Client
 * @param playingp Details of the playing track
 * @return 0 on success, non-0 on error
 */
int disorder_playing(disorder_client *c, struct queue_entry **playingp);

/** @brief Delete a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param c Client
 * @param playlist Playlist to delete
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_delete(disorder_client *c, const char *playlist);

/** @brief List the contents of a playlist
 *
 * Requires the 'read' right and oermission to read the playlist.
 *
 * @param c Client
 * @param playlist Playlist name
 * @param tracksp List of tracks in playlist
 * @param ntracksp Number of elements in tracksp
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_get(disorder_client *c, const char *playlist, char ***tracksp, int *ntracksp);

/** @brief Get a playlist's sharing status
 *
 * Requires the 'read' right and permission to read the playlist.
 *
 * @param c Client
 * @param playlist Playlist to read
 * @param sharep Sharing status ("public", "private" or "shared")
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_get_share(disorder_client *c, const char *playlist, char **sharep);

/** @brief Lock a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.  A given connection may lock at most one playlist.
 *
 * @param c Client
 * @param playlist Playlist to delete
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_lock(disorder_client *c, const char *playlist);

/** @brief Set the contents of a playlist
 *
 * Requires the 'play' right and permission to modify the playlist, which must be locked.
 *
 * @param c Client
 * @param playlist Playlist to modify
 * @param tracks New list of tracks for playlist
 * @param ntracks Length of tracks
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_set(disorder_client *c, const char *playlist, char **tracks, int ntracks);

/** @brief Set a playlist's sharing status
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param c Client
 * @param playlist Playlist to modify
 * @param share New sharing status ("public", "private" or "shared")
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_set_share(disorder_client *c, const char *playlist, const char *share);

/** @brief Unlock the locked playlist playlist
 *
 * The playlist to unlock is implicit in the connection.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_unlock(disorder_client *c);

/** @brief List playlists
 *
 * Requires the 'read' right.  Only playlists that you have permission to read are returned.
 *
 * @param c Client
 * @param playlistsp Playlist names
 * @param nplaylistsp Number of elements in playlistsp
 * @return 0 on success, non-0 on error
 */
int disorder_playlists(disorder_client *c, char ***playlistsp, int *nplaylistsp);

/** @brief Get all the preferences for a track
 *
 * 
 *
 * @param c Client
 * @param track Track name
 * @param prefsp Track preferences
 * @return 0 on success, non-0 on error
 */
int disorder_prefs(disorder_client *c, const char *track, struct kvp **prefsp);

/** @brief List the queue
 *
 * 
 *
 * @param c Client
 * @param queuep Current queue contents
 * @return 0 on success, non-0 on error
 */
int disorder_queue(disorder_client *c, struct queue_entry **queuep);

/** @brief Disable random play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_random_disable(disorder_client *c);

/** @brief Enable random play
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_random_enable(disorder_client *c);

/** @brief Detect whether random play is enabled
 *
 * Random play counts as enabled even if play is disabled.
 *
 * @param c Client
 * @param enabledp 1 if random play is enabled and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_random_enabled(disorder_client *c, int *enabledp);

/** @brief List recently played tracks
 *
 * 
 *
 * @param c Client
 * @param recentp Recently played tracks
 * @return 0 on success, non-0 on error
 */
int disorder_recent(disorder_client *c, struct queue_entry **recentp);

/** @brief Re-read configuraiton file.
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_reconfigure(disorder_client *c);

/** @brief Register a new user
 *
 * Requires the 'register' right which is usually only available to the 'guest' user.  Redeem the confirmation string via 'confirm' to complete registration.
 *
 * @param c Client
 * @param username Requested new username
 * @param password Requested initial password
 * @param email New user's email address
 * @param confirmationp Confirmation string
 * @return 0 on success, non-0 on error
 */
int disorder_register(disorder_client *c, const char *username, const char *password, const char *email, char **confirmationp);

/** @brief Send a password reminder.
 *
 * If the user has no valid email address, or no password, or a reminder has been sent too recently, then no reminder will be sent.
 *
 * @param c Client
 * @param username User to remind
 * @return 0 on success, non-0 on error
 */
int disorder_reminder(disorder_client *c, const char *username);

/** @brief Remove a track form the queue.
 *
 * Requires one of the 'remove mine', 'remove random' or 'remove any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param id Track ID
 * @return 0 on success, non-0 on error
 */
int disorder_remove(disorder_client *c, const char *id);

/** @brief Rescan all collections for new or obsolete tracks.
 *
 * Requires the 'rescan' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_rescan(disorder_client *c);

/** @brief Resolve a track name
 *
 * Converts aliases to non-alias track names
 *
 * @param c Client
 * @param track Track name (might be an alias)
 * @param resolvedp Resolve track name (definitely not an alias)
 * @return 0 on success, non-0 on error
 */
int disorder_resolve(disorder_client *c, const char *track, char **resolvedp);

/** @brief Resume the currently playing track
 *
 * Requires the 'pause' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_resume(disorder_client *c);

/** @brief Revoke a cookie.
 *
 * It will not subsequently be possible to log in with the cookie.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_revoke(disorder_client *c);

/** @brief Get the server's RTP address information
 *
 * 
 *
 * @param c Client
 * @param addressp Where to store hostname or address
 * @param portp Where to store service name or port number
 * @return 0 on success, non-0 on error
 */
int disorder_rtp_address(disorder_client *c, char **addressp, char **portp);

/** @brief Terminate the playing track.
 *
 * Requires one of the 'scratch mine', 'scratch random' or 'scratch any' rights depending on how the track came to be added to the queue.
 *
 * @param c Client
 * @param id Track ID (optional)
 * @return 0 on success, non-0 on error
 */
int disorder_scratch(disorder_client *c, const char *id);

/** @brief Schedule a track to play in the future
 *
 * 
 *
 * @param c Client
 * @param when When to play the track
 * @param priority Event priority ("normal" or "junk")
 * @param track Track to play
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_add_play(disorder_client *c, time_t when, const char *priority, const char *track);

/** @brief Schedule a global setting to be changed in the future
 *
 * 
 *
 * @param c Client
 * @param when When to change the setting
 * @param priority Event priority ("normal" or "junk")
 * @param pref Global preference to set
 * @param value New value of global preference
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_add_set_global(disorder_client *c, time_t when, const char *priority, const char *pref, const char *value);

/** @brief Schedule a global setting to be unset in the future
 *
 * 
 *
 * @param c Client
 * @param when When to change the setting
 * @param priority Event priority ("normal" or "junk")
 * @param pref Global preference to set
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_add_unset_global(disorder_client *c, time_t when, const char *priority, const char *pref);

/** @brief Delete a scheduled event.
 *
 * Users can always delete their own scheduled events; with the admin right you can delete any event.
 *
 * @param c Client
 * @param event ID of event to delete
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_del(disorder_client *c, const char *event);

/** @brief Get the details of scheduled event
 *
 * 
 *
 * @param c Client
 * @param id Event ID
 * @param actiondatap Details of event
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_get(disorder_client *c, const char *id, struct kvp **actiondatap);

/** @brief List scheduled events
 *
 * This just lists IDs.  Use 'schedule-get' to retrieve more detail
 *
 * @param c Client
 * @param idsp List of event IDs
 * @param nidsp Number of elements in idsp
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_list(disorder_client *c, char ***idsp, int *nidsp);

/** @brief Search for tracks
 *
 * Terms are either keywords or tags formatted as 'tag:TAG-NAME'.
 *
 * @param c Client
 * @param terms List of search terms
 * @param tracksp List of matching tracks
 * @param ntracksp Number of elements in tracksp
 * @return 0 on success, non-0 on error
 */
int disorder_search(disorder_client *c, const char *terms, char ***tracksp, int *ntracksp);

/** @brief Set a track preference
 *
 * Requires the 'prefs' right.
 *
 * @param c Client
 * @param track Track name
 * @param pref Preference name
 * @param value New value
 * @return 0 on success, non-0 on error
 */
int disorder_set(disorder_client *c, const char *track, const char *pref, const char *value);

/** @brief Set a global preference
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param pref Preference name
 * @param value New value
 * @return 0 on success, non-0 on error
 */
int disorder_set_global(disorder_client *c, const char *pref, const char *value);

/** @brief Request server shutdown
 *
 * Requires the 'admin' right.
 *
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_shutdown(disorder_client *c);

/** @brief Get server statistics
 *
 * The details of what the server reports are not really defined.  The returned strings are intended to be printed out one to a line.
 *
 * @param c Client
 * @param statsp List of server information strings.
 * @param nstatsp Number of elements in statsp
 * @return 0 on success, non-0 on error
 */
int disorder_stats(disorder_client *c, char ***statsp, int *nstatsp);

/** @brief Get a list of known tags
 *
 * Only tags which apply to at least one track are returned.
 *
 * @param c Client
 * @param tagsp List of tags
 * @param ntagsp Number of elements in tagsp
 * @return 0 on success, non-0 on error
 */
int disorder_tags(disorder_client *c, char ***tagsp, int *ntagsp);

/** @brief Unset a track preference
 *
 * Requires the 'prefs' right.
 *
 * @param c Client
 * @param track Track name
 * @param pref Preference name
 * @return 0 on success, non-0 on error
 */
int disorder_unset(disorder_client *c, const char *track, const char *pref);

/** @brief Set a global preference
 *
 * Requires the 'global prefs' right.
 *
 * @param c Client
 * @param pref Preference name
 * @return 0 on success, non-0 on error
 */
int disorder_unset_global(disorder_client *c, const char *pref);

/** @brief Get a user property.
 *
 * If the user does not exist an error is returned, if the user exists but the property does not then a null value is returned.
 *
 * @param c Client
 * @param username User to read
 * @param property Property to read
 * @param valuep Value of property
 * @return 0 on success, non-0 on error
 */
int disorder_userinfo(disorder_client *c, const char *username, const char *property, char **valuep);

/** @brief Get a list of users
 *
 * 
 *
 * @param c Client
 * @param usersp List of users
 * @param nusersp Number of elements in usersp
 * @return 0 on success, non-0 on error
 */
int disorder_users(disorder_client *c, char ***usersp, int *nusersp);

/** @brief Get the server version
 *
 * 
 *
 * @param c Client
 * @param versionp Server version string
 * @return 0 on success, non-0 on error
 */
int disorder_version(disorder_client *c, char **versionp);

/** @brief Set the volume
 *
 * 
 *
 * @param c Client
 * @param left Left channel volume
 * @param right Right channel volume
 * @return 0 on success, non-0 on error
 */
int disorder_set_volume(disorder_client *c, long left, long right);

/** @brief Get the volume
 *
 * 
 *
 * @param c Client
 * @param leftp Left channel volume
 * @param rightp Right channel volume
 * @return 0 on success, non-0 on error
 */
int disorder_get_volume(disorder_client *c, long *leftp, long *rightp);

#endif
