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

import java.util.*;

/**
 * Information about a track in the queue or another list.
 *
 * Returned by various methods of {@link DisorderServer DisorderServer}.
 *
 * <p>Queued track data appears in the following contexts:
 * <ul>
 * <li>The playing track
 * <li>The queue
 * <li>Recently played tracks
 * </ul>
 *
 * <p>The same format is used for each although the available
 * information differs slightly.
 *
 * <p>It is possible that future versions of the server will send keys
 * unknown to this class.  The current implementation just ignores
 * them.  However, unknown states and origins will currently generate
 * an exception.
 */
public class TrackInformation {
  /**
   * Possible track origins.
   */
  public enum Origin {
    Adopted,
    Picked,
    Random,
    Scheduled,
    Scratch
  }

  /**
   * Possible track states
   */
  public enum State {
    Failed,
    Ok,
    Scratched,
    Paused,
    Unplayed,
    Quitting
  }

  /**
   * When the track is expected to play.
   *
   * <p>This is only meaningful in the queue and may be <code>null</code>
   * or nonsense in other contexts.
   */
  public Date expected;

  /**
   * Unique ID in the queue.
   *
   * <p>This is always set in returns from the server but could be
   * <code>null</code> in a locally created instance.
   */
  public String id;

  /**
   * When the track was played.
   *
   * <p>This will either be <code>null</code> or <code>Date(0)</code>
   * if the track hasn't been played yet.
   */
  public Date played;

  /**
   * The user that scratched the track.
   *
   * <p>This will be <code>null</code> if the track hasn't been
   * scratched (which is most tracks).  Check <code>state</code> for
   * <code>Scratched</code> rather than just inspecting this field.
   */
  public String scratched;

  /**
   * The origin of the track.
   */
  public Origin origin;

  /**
   * The current state of the track.
   */
  public State state;

  /**
   * The user that submitted the track.
   *
   * <p>This will be <code>null</code> if the track was picked at random.
   */
  public String submitter;

  /**
   * The name of the track.
   */
  public String track;

  /**
   * When the track was added to the queue.
   */
  public Date when;

  /**
   * How much of the track has been played so far.
   *
   * <p>This is only meaningful for the currently playing track.
   */
  public int sofar;

  /**
   * The exit status of the player.
   *
   * <p>Note that this is packed UNIX “wait status” code, not a simple
   * exit status.
   */
  public int wstat;

  /**
   * Construct an empty track information object.
   *
   * <p>The usual way to get one is for it to be returned by some
   * method of DisorderServer.  However, in some cases it may be
   * convenient to create your own and fill it in, so this default
   * constructor is provided.
   */
  public TrackInformation() {
  }

  /**
   * Construct a new track information object from a string.
   *
   * @param s Encoded track information
   * @throws DisorderParseError If the track information is malformed
   */
  TrackInformation(String s) throws DisorderParseError {
    unpack(DisorderMisc.split(s, false), 0);
  }

  /**
   * Construct a new track information object from a parsed response.
   *
   * @param v Response from server
   * @param start Start index of track information
   * @throws DisorderParseError If the track information is malformed
   */
  TrackInformation(Vector<String> v, int start)
    throws DisorderParseError {
    unpack(v, start);
  }

  /**
   * Fill in track information from a parsed response
   *
   * @param v Response from server
   * @param start Start index of track information
   * @throws DisorderParseError If the track information is malformed
   */
  private void unpack(Vector<String> v, int start) throws DisorderParseError {
    if((v.size() - start) % 2 == 1)
      throw new DisorderParseError("odd-length track information");
    int n = start;
    while(n < v.size()) {
      String key = v.get(n++);
      String value = v.get(n++);
      if(key.equals("expected"))
        expected = new Date(Long.parseLong(value));
      else if(key.equals("id"))
        id = value;
      else if(key.equals("played"))
        played = new Date(Long.parseLong(value));
      else if(key.equals("scratched"))
        scratched = value;
      else if(key.equals("origin"))
        try {
          origin = Origin.valueOf(fixCase(value));
        } catch(IllegalArgumentException e) {
          throw new DisorderParseError("unknown origin: " + value);
        }
      else if(key.equals("sofar"))
        sofar = Integer.parseInt(value);
      else if(key.equals("state"))
        try {
          state = State.valueOf(fixCase(value));
        } catch(IllegalArgumentException e) {
          throw new DisorderParseError("unknown state: " + value);
        }
      else if(key.equals("submitter"))
        submitter = value;
      else if(key.equals("track"))
        track = value;
      else if(key.equals("when"))
        when = new Date(Long.parseLong(value));
      else if(key.equals("wstat"))
        wstat = Integer.parseInt(value);
      else {
        /* We interpret unknown keys as coming from the future and
         * ignore them */
      }
    }
  }

  /**
   * Returns a string representation of the object.
   *
   * @return a string representation of the object.
   */
  @SuppressWarnings("super")
  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder();
    sb.append("track: ");
    sb.append(track);
    if(id != null) {
      sb.append(" id: ");
      sb.append(id);
    }
    sb.append(" origin: ");
    sb.append(origin.toString());
    if(played != null) {
      sb.append(" played: ");
      sb.append(played.toString());
    }
    if(expected != null) {
      sb.append(" expected: ");
      sb.append(expected.toString());
    }
    if(scratched != null) {
      sb.append(" scratched by: ");
      sb.append(scratched);
    }
    sb.append(" state: ");
    sb.append(state.toString());
    if(wstat != 0) {
      sb.append(" wstat: ");
      sb.append(wstat);
    }
    return sb.toString();
  }

  /**
   * Uppercase the first letter of a string
   *
   * @param s String to modify
   * @return Same string with first letter converted to upper case
   */
  static private String fixCase(String s) {
    char cs[] = s.toCharArray();
    if(cs.length > 0)
      cs[0] = Character.toUpperCase(cs[0]);
    return new String(cs);
  }

}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
