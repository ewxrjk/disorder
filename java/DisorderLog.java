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
 * Base class for log consumer.
 *
 * To use this class:
 * <ul>
 * <li>Subclass it, overriding the methods you're interested in.
 *     By default, all methods do nothing.
 * <li>Create an instance.
 * <li>In a thread, create a {@link
 *     uk.org.greenend.disorder.DisorderServer} and invoke pass the
 *     instance to its <code>log</code> method.  This won't return.
 * <li>The methods of your instance will be called when things happen
 *     in the server.
 * </ul>
 *
 * <p>A number of methods are invoked automatically on startup to allow
 * the caller to know the current state.
 */
public class DisorderLog implements DisorderLogInterface {
    public void adopted(String id, String username) {
    }

    public void completed(String track) {
    }

    public void failed(String track, String error) {
    }

    public void moved(String username) {
    }

    public void playing(String track, String username) {
    }

    public void playlistCreated(String playlist, String sharing) {
    }

    public void playlistDeleted(String playlist) {
    }

    public void playlistModified(String playlist, String sharing) {
    }

    public void queue(TrackInformation entry) {
    }

    public void recentAdded(TrackInformation entry) {
    }

    public void recentRemoved(String id) {
    }

    public void removed(String id, String username) {
    }

    public void rescanned() {
    }

    public void scratched(String track, String username) {
    }

    public void stateCompleted() {
    }

    public void stateEnabled() {
    }

    public void stateDisabled() {
    }

    public void stateRandomEnabled() {
    }

    public void stateRandomDisabled() {
    }

    public void stateFailed() {
    }

    public void statePause() {
    }

    public void statePlaying() {
    }

    public void stateResume() {
    }

    public void stateRightsChanged(String newRights) {
    }

    public void stateScratched() {
    }

    public void userAdd(String username) {
    }

    public void userDelete(String username) {
    }

    public void userEdit(String username, String property) {
    }

    public void userConfirm(String username) {
    }

    public void volume(int left, int right) {
    }
}


/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
