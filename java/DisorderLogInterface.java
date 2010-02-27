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
package uk.org.greenend.disorder;

/**
 * Interface for log consumer.
 *
 * To use this class:
 * <ul>
 *
 * <li>Implement it, possibly subclassing from {@link
 *     uk.org.greenend.disorder.DisorderLog} if you don't want to
 *     implement all the methods you don't care about.
 *
 * <li>Create an instance.
 *
 * <li>In a thread, create a {@link
 *     uk.org.greenend.disorder.DisorderServer} and invoke pass the
 *     instance to its <code>log</code> method.  This won't return.
 *
 * <li>The methods of your instance will be called when things happen
 *     in the server.
 * </ul>
 *
 * <p>A number of methods are invoked automatically on startup to allow
 * the caller to know the current state.
 */
public interface DisorderLogInterface {
    /**
     * Called when a track is adopted.
     *
     * @param id Track ID
     * @param username Adopting user
     */
    public void adopted(String id, String username);

    /**
     * Called when a track completes.
     *
     * @param track Track name
     */
    public void completed(String track);

    /**
     * Called when a track fails.
     *
     * @param track Track name
     * @param error Error message
     */
    public void failed(String track, String error);

    /**
     * Called when a track is moved.
     * More detail is not included - refetch the queue to see its new state.
     *
     * @param username User who moved the track
     */
    public void moved(String username);

    /**
     * Called when a track starts playing.
     *
     * @param track Track that started
     * @param username User that chose track, or <code>null</code>
     */
    public void playing(String track, String username);

    /**
     * Called when a playlist is created.
     * It's possible that the names of private playlists will be leaked
     * via this method.
     *
     * @param playlist Playlist name
     * @param sharing Sharing status
     */
    public void playlistCreated(String playlist, String sharing);

    /**
     * Called when a playlist is deleted.
     * It's possible that the names of private playlists will be leaked
     * via this method.
     *
     * @param playlist Playlist name
     */
    public void playlistDeleted(String playlist);

    /**
     * Called when a playlist is modified.
     * It's possible that the names of private playlists will be leaked
     * via this method.
     *
     * <p>This is called when either the contents or the sharing
     * status of a playlist are modified, though only the latter is
     * included in the arguments.  Refetch the playlists's contents to
     * see if they have changed.
     *
     * @param playlist Playlist name
     * @param sharing New sharing status.
     */
    public void playlistModified(String playlist, String sharing);

    /**
     * Called when a track is added to the queue.
     * Refetch the queue to see <i>where</i> it was added.
     *
     * @param entry New queue entry
     */
    public void queue(TrackInformation entry);

    /**
     * Called when a track is added to the recently played list.
     *
     * @param entry New recent-list entry
     */
    public void recentAdded(TrackInformation entry);

    /**
     * Called when a track is removed from the recently played list.
     *
     * @param id ID of removed track
     */
    public void recentRemoved(String id);

    /**
     * Called when a track is removed from the queue.
     * This is called both when a track is explicitly removed and
     * when a track starts to play.  In the latter case <code>username</code>
     * will be <code>null</code>.
     *
     * @param id ID of removed track
     * @param username User who removed track, or <code>null</code>
     */
    public void removed(String id, String username);

    /**
     * Called when a rescan completes.
     */
    public void rescanned();

    /**
     * Called when a track is scratched.
     *
     * @param track Track that was scratched (it will be the playing track)
     * @param username User who scratched it
     */
    public void scratched(String track, String username);

    /**
     * Called when the current track completes OK.
     *
     */
    public void stateCompleted();

    /**
     * Called when play is enabled.
     *
     */
    public void stateEnabled();

    /**
     * Called when play is disabled.
     *
     */
    public void stateDisabled();

    /**
     * Called when random play is enabled.
     *
     */
    public void stateRandomEnabled();

    /**
     * Called when random play is disabled.
     *
     */
    public void stateRandomDisabled();

    /**
     * Called when the current track fails.
     *
     */
    public void stateFailed();

    /**
     * Called when the current track is paused.
     *
     */
    public void statePause();

    /**
     * Called when a track starts playing.
     *
     */
    public void statePlaying();

    /**
     * Called when the current track is resumed.
     *
     */
    public void stateResume();

    /**
     * Called when the user's rights change.
     *
     * @param newRights New rights string
     */
    public void stateRightsChanged(String newRights);

    /**
     * Called when the current track is scratched.
     *
     */
    public void stateScratched();
    
    /**
     * Called when a user is created.
     *
     * @param username Name of new user
     */
    public void userAdd(String username);

    /**
     * Called when a user is deleted.
     *
     * @param username Name of deleted user
     */
    public void userDelete(String username);

    /**
     * Called when a user has some property modified.
     *
     * @param username Name of user
     * @param property Property that was modified
     */
    public void userEdit(String username, String property);

    /**
     * Called when a user is confirmed.
     *
     * <p>This means that they completed creation of their login via
     * the web interface.  It has no religious significance.
     *
     * @param username Name of confirmed user
     */
    public void userConfirm(String username);

    /**
     * Called when the volume changes.
     *
     * @param left New left-channel volume
     * @param right New right-channel volume
     */
    public void volume(int left, int right);

}


/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
