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
#ifndef CLIENT_STUBS_H
#define CLIENT_STUBS_H

/** @brief Adopt a track
 *
 * Makes the calling user owner of a randomly picked track.
 *
 * @param id Track ID
 * @return 0 on success, non-0 on error
 */
int disorder_adopt(disorder_client *c, const char *id);

/** @brief Create a user
 *
 * Create a new user.  Requires the 'admin' right.  Email addresses etc must be filled in in separate commands.
 *
 * @param user New username
 * @param password Initial password
 * @param rights Initial rights (optional)
 * @return 0 on success, non-0 on error
 */
int disorder_adduser(disorder_client *c, const char *user, const char *password, const char *rights);

/** @brief Confirm registration
 *
 * The confirmation string must have been created with 'register'.  The username is returned so the caller knows who they are.
 *
 * @param confirmation Confirmation string
 * @return 0 on success, non-0 on error
 */
int disorder_confirm(disorder_client *c, const char *confirmation);
/** @brief Log in with a cookie
 *
 * The cookie must have been created with 'make-cookie'.  The username is returned so the caller knows who they are.
 *
 * @param cookie Cookie string
 * @return 0 on success, non-0 on error
 */
int disorder_cookie(disorder_client *c, const char *cookie);
/** @brief Delete user
 *
 * Requires the 'admin' right.
 *
 * @param user User to delete
 * @return 0 on success, non-0 on error
 */
int disorder_deluser(disorder_client *c, const char *user);

/** @brief Disable play
 *
 * Play will stop at the end of the current track, if one is playing.  Requires the 'global prefs' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_disable(disorder_client *c);

/** @brief Set a user property
 *
 * With the 'admin' right you can do anything.  Otherwise you need the 'userinfo' right and can only set 'email' and 'password'.
 *
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
 * @return 0 on success, non-0 on error
 */
int disorder_enable(disorder_client *c);

/** @brief Detect whether play is enabled
 *
 * 
 *
 * @param enabledp 1 if play is enabled and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_enabled(disorder_client *c, int *enabledp);

/** @brief Test whether a track exists
 *
 * 
 *
 * @param track Track name
 * @param existsp 1 if the track exists and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_exists(disorder_client *c, const char *track, int *existsp);

/** @brief Get a track preference
 *
 * If the track does not exist that is an error.  If the track exists but the preference does not then a null value is returned.
 *
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
 * @param pref Global preference name
 * @param valuep Preference value
 * @return 0 on success, non-0 on error
 */
int disorder_get_global(disorder_client *c, const char *pref, char **valuep);

/** @brief Create a login cookie for this user
 *
 * The cookie may be redeemed via the 'cookie' command
 *
 * @param cookiep Newly created cookie
 * @return 0 on success, non-0 on error
 */
int disorder_make_cookie(disorder_client *c, char **cookiep);

/** @brief Do nothing
 *
 * Used as a keepalive.  No authentication required.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_nop(disorder_client *c);

/** @brief Get a track name part
 *
 * If the name part cannot be constructed an empty string is returned.
 *
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
 * @return 0 on success, non-0 on error
 */
int disorder_pause(disorder_client *c);

/** @brief Delete a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param playlist Playlist to delete
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_delete(disorder_client *c, const char *playlist);

/** @brief Lock a playlist
 *
 * Requires the 'play' right and permission to modify the playlist.  A given connection may lock at most one playlist.
 *
 * @param playlist Playlist to delete
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_lock(disorder_client *c, const char *playlist);

/** @brief Get a playlist's sharing status
 *
 * Requires the 'read' right and permission to read the playlist.
 *
 * @param playlist Playlist to read
 * @param sharep Sharing status ("public", "private" or "shared")
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_get_share(disorder_client *c, const char *playlist, char **sharep);

/** @brief Set a playlist's sharing status
 *
 * Requires the 'play' right and permission to modify the playlist.
 *
 * @param playlist Playlist to modify
 * @param share New sharing status ("public", "private" or "shared")
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_set_share(disorder_client *c, const char *playlist, const char *share);

/** @brief Unlock the locked playlist playlist
 *
 * The playlist to unlock is implicit in the connection.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_unlock(disorder_client *c);

/** @brief Disable random play
 *
 * Requires the 'global prefs' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_random_disable(disorder_client *c);

/** @brief Enable random play
 *
 * Requires the 'global prefs' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_random_enable(disorder_client *c);

/** @brief Detect whether random play is enabled
 *
 * Random play counts as enabled even if play is disabled.
 *
 * @param enabledp 1 if random play is enabled and 0 otherwise
 * @return 0 on success, non-0 on error
 */
int disorder_random_enabled(disorder_client *c, int *enabledp);

/** @brief Re-read configuraiton file.
 *
 * Requires the 'admin' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_reconfigure(disorder_client *c);

/** @brief Register a new user
 *
 * Requires the 'register' right which is usually only available to the 'guest' user.  Redeem the confirmation string via 'confirm' to complete registration.
 *
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
 * @param username User to remind
 * @return 0 on success, non-0 on error
 */
int disorder_reminder(disorder_client *c, const char *username);

/** @brief Remove a track form the queue.
 *
 * Requires one of the 'remove mine', 'remove random' or 'remove any' rights depending on how the track came to be added to the queue.
 *
 * @param id Track ID
 * @return 0 on success, non-0 on error
 */
int disorder_remove(disorder_client *c, const char *id);

/** @brief Rescan all collections for new or obsolete tracks.
 *
 * Requires the 'rescan' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_rescan(disorder_client *c);

/** @brief Resolve a track name
 *
 * Converts aliases to non-alias track names
 *
 * @param track Track name (might be an alias)
 * @param resolvedp Resolve track name (definitely not an alias)
 * @return 0 on success, non-0 on error
 */
int disorder_resolve(disorder_client *c, const char *track, char **resolvedp);

/** @brief Resume the currently playing track
 *
 * Requires the 'pause' right.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_resume(disorder_client *c);

/** @brief Revoke a cookie.
 *
 * It will not subsequently be possible to log in with the cookie.
 *
 * @return 0 on success, non-0 on error
 */
int disorder_revoke(disorder_client *c);

/** @brief Terminate the playing track.
 *
 * Requires one of the 'scratch mine', 'scratch random' or 'scratch any' rights depending on how the track came to be added to the queue.
 *
 * @param id Track ID (optional)
 * @return 0 on success, non-0 on error
 */
int disorder_scratch(disorder_client *c, const char *id);

/** @brief Delete a scheduled event.
 *
 * Users can always delete their own scheduled events; with the admin right you can delete any event.
 *
 * @param event ID of event to delete
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_del(disorder_client *c, const char *event);

/** @brief Set a track preference
 *
 * Requires the 'prefs' right.
 *
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
 * @param pref Preference name
 * @param value New value
 * @return 0 on success, non-0 on error
 */
int disorder_set_global(disorder_client *c, const char *pref, const char *value);

/** @brief Unset a track preference
 *
 * Requires the 'prefs' right.
 *
 * @param track Track name
 * @param pref Preference name
 * @return 0 on success, non-0 on error
 */
int disorder_unset(disorder_client *c, const char *track, const char *pref);

/** @brief Set a global preference
 *
 * Requires the 'global prefs' right.
 *
 * @param pref Preference name
 * @return 0 on success, non-0 on error
 */
int disorder_unset_global(disorder_client *c, const char *pref);

/** @brief Get a user property.
 *
 * If the user does not exist an error is returned, if the user exists but the property does not then a null value is returned.
 *
 * @param username User to read
 * @param property Property to read
 * @param valuep Value of property
 * @return 0 on success, non-0 on error
 */
int disorder_userinfo(disorder_client *c, const char *username, const char *property, char **valuep);

/** @brief Get the server version
 *
 * 
 *
 * @param versionp Server version string
 * @return 0 on success, non-0 on error
 */
int disorder_version(disorder_client *c, char **versionp);

#endif
