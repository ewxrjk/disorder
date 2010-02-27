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

public class DisorderModel {

  private DisorderServer conn;

  private DisorderServer log;

  private Thread logger;

  private DisorderQueueModel queue;

  private DisorderQueueModel recent;

  private DisorderQueueModel new_;

  private Map<String,DisorderQueueModel> playlists;

  class LogInterface extends DisorderLog {
  }

  public DisorderModel(DisorderServer conn) {
    this.conn = conn;

    // Set everything up
    queue = new DisorderQueueModel(this);
    recent = new DisorderQueueModel(this);
    new_ = new DisorderQueueModel(this);
    playlists = new HashMap<String,DisorderQueueModel>();

    // TODO fill in initial queues, users, state, ...

    // Set off a logger on a separate connection
    final LogInterface li = new LogInterface();
    logger = new Thread() {
        void run() {
          new DisorderServer(conn).log(li);
        }
      };
    // TODO we need to think about how we cancel it...
  }

  public void registerMonitor(DisorderMonitor monitor) {
  }

  public DisorderQueueModel getQueue() {
    return queue;
  }

  public DisorderQueueModel getRecent() {
    return recent;
  }

  public DisorderQueueModel getNew() {
    return new_;
  }

  public synchronized DisorderQueueModel getPlaylist(String playlist) {
    return playlists.get(playlist);
  }

  public TrackInformation getPlaying() {
  }

}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
