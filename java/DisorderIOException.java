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
 * IO error exception class.
 *
 * Thrown when an IO error occurs on the connection to the server, for
 * instance if it the server fails or there is a network problem.
 * This is a normal part of the lifetime of a connection to a server.
 */
public class DisorderIOException extends Exception {
  private Throwable error;

  /**
   * Constructs a new IO error wrapping an IO error.
   *
   * @param serverName Name of the server
   * @param e The underlying error
   */
  public DisorderIOException(String serverName, Throwable e) {
    super(serverName + ": " + e.toString());
  }

  /**
   * Construct a new IO error with no contained exception
   *
   * @param serverName Name of the server
   */
  public DisorderIOException(String serverName) {
    super(serverName + ": I/O error");
    error = null;
  }

  /**
   * Return the underlying error.
   *
   * @return The underlying exception, or <code>null</code> if there wasn't one.
   */
  public Throwable getError() {
    return error;
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
