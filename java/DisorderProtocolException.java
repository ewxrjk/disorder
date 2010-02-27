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
 * Protocol error exception class.
 *
 * <p>Thrown when the server returns an error.  Examples include:
 *
 * <ul>
 * <li>Authentication failure
 * <li>Authenticated user lacks sufficient privilege
 * <li>Access a nonexistent track
 * </ul>
 */
public class DisorderProtocolException extends Exception {
  /**
   * Constructs a new protocol error with an error message.
   *
   * @param serverName The server that reported the error
   * @param message The error message
   */
  public DisorderProtocolException(String serverName, String message) {
    super(serverName + ": " + message);
  }

  private static final long serialVersionUID = 0;
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
