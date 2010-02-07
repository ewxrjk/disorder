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

import java.io.*;
import java.net.*;
import java.util.*;
import java.security.*;
import java.nio.charset.*;

/**
 * A synchronous connection to a DisOrder server.
 *
 * <p>Having created a connection, use the various methods to play
 * tracks, list the queue, etc.  Most methods have exactly the same
 * name as the underlying protocol command, the exceptions being:
 *
 * <ul>
 * <li>Where there is a dash in the command, in which case a camelcase
 * construction is used instead, e.g. {@link #playlistGet(String)
 * playlistGet}.
 * <li>Where there is a clash with a Java reserved word (e.g. {@link
 * #newTracks(int) newTracks}).
 * </ul>
 *
 * <p>Certain classes of commands are not implemented:
 * <ul>
 * <li>Commands that require admin rights are not implemented as they
 * only work on local connections, and this class always uses a TCP
 * connection.
 * <li>Commands that are only used by the web interface are
 * also not implemented, but there's not inherent reason why they
 * couldn't be in a future version.
 * <li>The <code>schedule-</code> commands are not implemented.
 * Again there's no reason they couldn't be.
 * </ul>
 *
 * <p>Command methods can throw the following exceptions:
 * <ul>
 * <li>{@link IOException}.  This is thrown if a network IO error occurs.
 * <li>{@link DisorderProtocolError}.  This is thrown if the server
 *     sends back an error.
 * <li>{@link DisorderParseError}.  This is thrown if a malformed message
 *     is received from the server.
 * </ul>
 *
 * <p>It is safe to use one <code>DisorderServer</code> from multiple
 * threads.
 *
 * <p>Set the system property <code>DISORDER_DEBUG</code> (to
 * anything) to turn on debugging.  Debug messages are sent to
 * <code>System.err</code>.
 */
public class DisorderServer {
  /* TODO: Currently we handle parallelization by making all public
   * methods synchronized.  The effect is to completely serialized all
   * commands, preventing any pipelining.  This might not perform well
   * in practice. */

  /* TODO: If the connection to the server fails we should
   * automatically reconnect and retry, within reason and perhaps
   * configurably.  This wouldn't change the interface - IOException
   * would still be possible when we give up - but would allow
   * long-running user interfaces to recover from transient outages
   * (e.g. server restarts) more transparently.  */

  private DisorderConfig config;
  private boolean connected;
  private Socket conn;
  private BufferedReader input;
  private PrintWriter output;
  private boolean debugging;
  private Formatter formatter;

  /**
   * Construct a server connection from a given configuration.
   *
   * <p>Creating a connection does not connect it.  Instead the
   * underlying connection is established on demand.
   *
   * @param c Configuration to use
   */
  public DisorderServer(DisorderConfig c) {
    if(System.getProperty("DISORDER_DEBUG") != null)
      debugging = true;
    config = c;
    connected = false;
  }

  /**
   * Construct a server connection from the default configuration.
   *
   * <p>Creating a connection does not connect it.  Instead the
   * underlying connection is established on demand.
   *
   * <p>Note that if the default configuration does not include the
   * password then won't be possible to supply it by some other means
   * using this constructor.  In that case you must create
   * <code>DisorderConfig</code> and fill in the missing bits manually
   * and then use {@link #DisorderServer(DisorderConfig)}.
   *
   * @throws DisorderParseError If a configuration file contains a syntax error
   * @throws IOException If an error occurs reading a configuration file
   */
  public DisorderServer() throws DisorderParseError,
                                 IOException {
    if(System.getProperty("DISORDER_DEBUG") != null)
      debugging = true;
    config = new DisorderConfig();
    connected = false;
  }

  /** Connect to the server.
   *
   * <p>Establishes a TCP connection to the server and authenticates
   * using the username and password from the configuration chosen at
   * construction time.
   *
   * <p>Does nothing if already connected.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  private void connect() throws IOException,
                                DisorderParseError,
                                DisorderProtocolError {
    if(connected)
      return;
    conn = new Socket(config.serverName, config.serverPort);
    input = new BufferedReader(
		new InputStreamReader(conn.getInputStream(),
				      Charset.forName("UTF-8")));
    output = new PrintWriter(
		new BufferedWriter(
		    new OutputStreamWriter(conn.getOutputStream(),
					   Charset.forName("UTF-8"))));
    // Field the initial greeting and attempt to log in
    {
      String greeting = getResponse();
      Vector<String> v = DisorderMisc.split(greeting, false/*comments*/);
      int rc = responseCode(v);
      if(rc != 231)
        throw new DisorderProtocolError(config.serverName, greeting);
      if(!v.get(1).equals("2"))
        throw new DisorderParseError("unknown protocol generation: " + v.get(1));
      String alg = v.get(2);
      String challenge = v.get(3);
      // Identify the hashing algorithm to use
      MessageDigest md;
      try {
        if(alg.equalsIgnoreCase("sha1"))
          md = MessageDigest.getInstance("SHA-1");
        else if(alg.equalsIgnoreCase("sha256"))
          md = MessageDigest.getInstance("SHA-256");
        else if(alg.equalsIgnoreCase("sha384"))
          md = MessageDigest.getInstance("SHA-384");
        else if(alg.equalsIgnoreCase("sha512"))
          md = MessageDigest.getInstance("SHA-512");
        else
          throw new DisorderParseError("unknown authentication algorithm: " + alg);
      } catch(NoSuchAlgorithmException e) {
        throw new RuntimeException("message digest " + alg + " not available", e);
      }
      // Construct the response
      md.update(config.password.getBytes());
      md.update(DisorderMisc.fromHex(challenge));
      String response = DisorderMisc.toHex(md.digest());
      send("user %s %s", config.user, response);
    }
    // Handle the response to our login attempt
    {
      String response = getResponse();
      Vector<String> v = DisorderMisc.split(response, false/*comments*/);
      int rc = responseCode(v);
      if(rc / 100 != 2)
        throw new DisorderProtocolError(config.serverName,
                                        "authentication failed: " + response);
    }
    connected = true;
  }

  /**
   * Send a command to the server.
   *
   * A newline is appended to the command automatically.
   *
   * @param format Format string
   * @param args Arguments to <code>format</code>
   */
  private void send(String format, Object ... args) {
    if(formatter == null)
      formatter = new Formatter();
    formatter.format(format, args);
    StringBuilder sb = (StringBuilder)formatter.out();
    String s = sb.toString();
    if(debugging) {
      System.err.print("SEND: ");
      System.err.println(s);
    }
    output.print(s);
    output.print('\n');         // no, not println!
    output.flush();
    sb.delete(0, s.length());
  }

  /**
   * Read a response form the server.
   *
   * Reads one line from the server and returns it as a string.  Note
   * that the trailing newline character is not included.
   *
   * @return Line from server, or null at end of input.
   * @throws IOException If a network IO error occurs
   */
  private String getResponse() throws IOException {
    StringBuilder sb = new StringBuilder();
    int n;
    while((n = input.read()) != -1) {
      char c = (char)n;
      if(c == '\n') {
        String s = sb.toString();
        if(debugging) 
          System.err.println("RECV: " + s);
        return s;
      }
      sb.append(c);
    }
    // We reached end of file
    return null;
  }

  /**
   * Pick out a response code.
   *
   * @return Response code in the range 0-999.
   * @throws DisorderParseError If the response is malformed
   */
  private int responseCode(List<String> v) throws DisorderParseError {
    if(v.size() < 1)
      throw new DisorderParseError("empty response");
    int rc = Integer.parseInt(v.get(0));
    if(rc < 0 || rc > 999)
      throw new DisorderParseError("invalid response code " + v.get(0));
    return rc;
  }

  /**
   * Await a response and check it's OK.
   *
   * Any 2xx response counts as success.  Anything else will generate
   * a DisorderProtocolError.
   *
   * @return Split response
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  private Vector<String> getPositiveResponse() throws IOException,
                                                      DisorderParseError,
                                                      DisorderProtocolError {
    String response = getResponse();
    Vector<String> v = DisorderMisc.split(response, false/*comments*/);
    int rc = responseCode(v);
    if(rc / 100 != 2)
      throw new DisorderProtocolError(config.serverName, response);
    return v;
  }

  /**
   * Get a string response
   *
   * <p>Any 2xx response with exactly one extra field will return the
   * value of that field.
   *
   * <p>If <code>optional<code> is <code>true</code> then a 555
   * response is also accepted, with a return value of
   * <code>null</code>.
   *
   * <p>Otherwise there will be a DisorderProtocolError or
   * DisorderParseError.
   *
   * @param optional Whether 555 responses are acceptable
   * @return Response value or <code>null</code>
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  String getStringResponse(boolean optional) throws IOException,
                                                    DisorderParseError,
                                                    DisorderProtocolError {
    String response = getResponse();
    Vector<String> v = DisorderMisc.split(response, false/*comments*/);
    int rc = responseCode(v);
    if(optional && rc == 555)
      return null;
    if(rc / 100 != 2)
      throw new DisorderProtocolError(config.serverName, response);
    if(v.size() != 2)
      throw new DisorderParseError("malformed response: " + response);
    return v.get(1);
  }

  /**
   * Get a boolean response
   *
   * Any 2xx response with the next field being "yes" or "no" counts
   * as success.  Anything else will generate a DisorderProtocolError
   * or DisorderParseError.
   *
   * @return Response truth value
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  boolean getBooleanResponse() throws IOException,
                                      DisorderParseError,
                                      DisorderProtocolError {
    String s = getStringResponse(false);
    if(s.equals("yes"))
      return true;
    else if(s.equals("no"))
      return false;
    else
      throw new DisorderParseError("expxected 'yes' or 'no' in response");
  }

  /**
   * Quote a string for use in a Disorder command
   *
   * @param s String to quote
   * @return Quoted string (possibly the same object as <code>s</code>)
   */
  static private String quote(String s) {
    if(s.length() == 0 || s.matches("[^\"\'\\#\0-\37 ]")) {
      StringBuilder sb = new StringBuilder();
      sb.append('"');
      for(int n = 0; n < s.length(); ++n) {
        char c = s.charAt(n);
        switch(c) {
        case '"':
        case '\\':
          sb.append('\\');
          // fall through
        default:
          sb.append(c);
          break;
        case '\n':
          sb.append('\\');
          sb.append('\n');
          break;
        }
      }
      sb.append('"');
      return sb.toString();
    } else
      return s;
  }

  /**
   * Get a response body.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   */
  List<String> getResponseBody() throws IOException,
                                        DisorderParseError {
    Vector<String> r = new Vector<String>();
    String s;
    while((s = getResponse()) != null
          && !s.equals("."))
      if(s.length() > 0 && s.charAt(0) == '.')
        r.add(s.substring(1));
      else
        r.add(s);
    if(s == null)
      throw new DisorderParseError("unterminated response body");
    return r;
  }

  // TODO adduser not implemented because only works on local connections

  /**
   * Adopt a track.
   *
   * Adopts a randomly picked track, leaving it in the same state as
   * if it was picked by the calling user.
   *
   * @param id Track to adopt
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void adopt(String id) throws IOException,
                                                   DisorderParseError,
                                                   DisorderProtocolError {
    connect();
    send("adopt %s", quote(id));
    getPositiveResponse();
  }

  /**
   * Get a list of files and directories.
   *
   * Returns a list of the files and directories immediately below
   * <code>path</code>.  If <code>regexp</code> is not
   * <code>null</code> then only matching names are returned.
   *
   * <p>If you need to tell files and directories apart, use {@link
   * #dirs(String,String) dirs()} and {@link #files(String,String)
   * files()} instead.
   *
   * @param path Parent directory
   * @param regexp Regular expression to filter results, or <code>null</code>
   * @return List of files and directiories
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> allfiles(String path,
                                            String regexp)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    if(regexp != null)
      send("allfiles %s %s", quote(path), quote(regexp));
    else
      send("allfiles %s", quote(path));
    getPositiveResponse();
    return getResponseBody();
  }

  // TODO confirm not implemented because only used by CGI
  // TODO cookie not implemented because only used by CGI
  // TODO deluser not implemented because only works on local connections

  /**
   * Get a list of directories.
   *
   * Returns a list of the directories immediately below
   * <code>path</code>.  If <code>regexp</code> is not
   * <code>null</code> then only matching names are returned.
   *
   * @param path Parent directory
   * @param regexp Regular expression to filter results, or <code>null</code>
   * @return List of directories
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> dirs(String path,
                                        String regexp)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    if(regexp != null)
      send("dirs %s %s", quote(path), quote(regexp));
    else
      send("dirs %s", quote(path));
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Disable further playing.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void disable() throws IOException,
                                            DisorderParseError,
                                            DisorderProtocolError {
    connect();
    send("disable");
    getPositiveResponse();
  }

  /**
   * Set a user property.
   *
   * @param username User to edit
   * @param property Property to set
   * @param value New value for property
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void edituser(String username,
                                    String property,
                                    String value) throws IOException,
                                                         DisorderParseError,
                                                         DisorderProtocolError {
    connect();
    send("edituser %s %s %s", quote(username), quote(property), quote(value));
    getPositiveResponse();
  }

  /**
   * Enable play.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void enable() throws IOException,
                                           DisorderParseError,
                                           DisorderProtocolError {
    connect();
    send("enable");
    getPositiveResponse();
  }

  /**
   * Test whether play is enabled.
   *
   * @return <code>true</code> if play is enabled, otherwise <code>false</code>
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized boolean enabled() throws IOException,
                                               DisorderParseError,
                                               DisorderProtocolError {
    connect();
    send("enabled");
    return getBooleanResponse();
  }

  /**
   * Test whether a track exists.
   *
   * @param track Track to check
   * @return <code>true</code> if the track exists, otherwise <code>false</code>
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized boolean exists(String track)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("exists %s", quote(track));
    return getBooleanResponse();
  }

  /**
   * Get a list of files.
   *
   * Returns a list of the files immediately below
   * <code>path</code>.  If <code>regexp</code> is not
   * <code>null</code> then only matching names are returned.
   *
   * @param path Parent directory
   * @param regexp Regular expression to filter results, or <code>null</code>
   * @return List of files
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> files(String path,
                                         String regexp)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    if(regexp != null)
      send("files %s %s", quote(path), quote(regexp));
    else
      send("files %s", quote(path));
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Get a track preference value.
   *
   * @param track Track to look up
   * @param pref Preference name
   * @return Preference value, or <code>null</code> if it's not set
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String get(String track,
                                 String pref) throws IOException,
                                                     DisorderParseError,
                                                     DisorderProtocolError {
    connect();
    send("get %s %s", quote(track), quote(pref));
    return getStringResponse(true);
  }

  /**
   * Get a global preference value
   *
   * @param key Preference name
   * @return Preference value, or <code>null</code> if it's not set
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String getGlobal(String key)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("get-global %s %s", quote(key));
    return getStringResponse(true);
  }

  /**
   * Get a track's length.
   *
   * @param track Track to look up
   * @return Track length in seconds
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized int length(String track) throws IOException,
                                                      DisorderParseError,
                                                      DisorderProtocolError {
    connect();
    send("length %s", quote(track));
    return Integer.parseInt(getStringResponse(false));
  }

  /**
   * Monitor the server log.
   *
   * <p>This method doesn't return.  (It might thrown an exception,
   * though.)  Instead it receives event log messages from the server
   * and calls appropriate methods on <code>l</code>.  You should
   * subclass <code>DisorderLog</code>, overriding those methods that
   * correspond to events you're interested in.
   *
   * @param l Log consumer
   */
  public synchronized void log(DisorderLog l)
     throws IOException,
            DisorderParseError,
            DisorderProtocolError {
    for(;;) {
      connect();
      send("log");
      getPositiveResponse();
      String r;
      while((r = getResponse()) != null) {
        Vector<String> v = DisorderMisc.split(r, false);
        String e = v.get(0);
        if(e.equals("adopted")) l.adopted(v.get(1), v.get(2));
        else if(e.equals("completed")) l.completed(v.get(1));
        else if(e.equals("failed")) l.failed(v.get(1), v.get(2));
        else if(e.equals("moved")) l.moved(v.get(1));
        else if(e.equals("playing")) l.playing(v.get(1),
                                               v.size() > 2 ? v.get(2) : null);
        else if(e.equals("playlist_created"))
          l.playlistCreated(v.get(1), v.get(2));
        else if(e.equals("playlist_deleted")) l.playlistDeleted(v.get(1));
        else if(e.equals("playlist_modified"))
          l.playlistModified(v.get(1), v.get(2));
        else if(e.equals("queue")) l.queue(new TrackInformation(v, 1));
        else if(e.equals("recent_added"))
          l.recentAdded(new TrackInformation(v, 1));
        else if(e.equals("recent_removed")) l.recentRemoved(v.get(1));
        else if(e.equals("removed")) l.removed(v.get(1),
                                               v.size() > 2 ? v.get(2) : null);
        else if(e.equals("rescanned")) l.rescanned();
        else if(e.equals("scratched")) l.scratched(v.get(1), v.get(2));
        else if(e.equals("state")) {
          e = v.get(1);
          if(e.equals("completed")) l.stateCompleted();
          else if(e.equals("enable_play")) l.stateEnabled();
          else if(e.equals("disable_play")) l.stateDisabled();
          else if(e.equals("enable_random")) l.stateRandomEnabled();
          else if(e.equals("disable_random")) l.stateRandomDisabled();
          else if(e.equals("failed")) l.stateFailed();
          else if(e.equals("pause")) l.statePause();
          else if(e.equals("playing")) l.statePlaying();
          else if(e.equals("resume")) l.stateResume();
          else if(e.equals("rights_changed")) l.stateRightsChanged(v.get(2));
          else if(e.equals("scratched")) l.stateScratched();
        }
        else if(e.equals("user_add")) l.userAdd(v.get(1));
        else if(e.equals("user_delete")) l.userDelete(v.get(1));
        else if(e.equals("user_edit")) l.userEdit(v.get(1), v.get(2));
        else if(e.equals("user_confirm")) l.userConfirm(v.get(1));
        else if(e.equals("volume"))
          l.volume(Integer.parseInt(v.get(1)),
                   Integer.parseInt(v.get(2)));
      }
      connected = false;
      // Go round again
    }
  }

  // TODO make-cookie not implemented because only used by CGI

  /**
   * Move a track.
   *
   * <p>If <code>delta</code> is positive then the track is moved
   * towards the head of the queue.  If it is negative then it is
   * moved towards the tail of the queue.
   *
   * <p>Tracks be identified by ID or name but ID is preferred.
   *
   * @param track Track ID or name
   * @param delta How far to move track
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void move(String track,
                                int delta) throws IOException,
                                                  DisorderParseError,
                                                  DisorderProtocolError {
    connect();
    send("move %s %d", quote(track), delta);
    getPositiveResponse();
  }

  /**
   * Move multiple tracks.
   *
   * ALl of the listed track IDs will be moved so that they appear
   * just after the target, but retaining their relative ordering
   * within themselves.
   *
   * @param target Target track ID
   * @param tracks Track IDs to move
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void moveafter(String target,
                                     String... tracks)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    StringBuilder sb = new StringBuilder();
    for(int n = 0; n < tracks.length; ++n) {
      sb.append(' ');
      sb.append(quote(tracks[n]));
    }
    send("moveafter %s %s", quote(target), sb.toString());
    getPositiveResponse();
  }

  /**
   * Get a list of newly added tracks.
   *
   * If <code>max</code> is positive then at most that many tracks
   * will be sent.  Otherwise the server will choose the maximum to
   * send (and in any case it may impose an upper limit).
   *
   * @param max Maximum number of tracks to get, or 0
   * @return List of new tracks
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> newTracks(int max)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    if(max > 0)
      send("new %d", max);
    else
      send("new");
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Do nothing.
   *
   * <p>Typically used to ensure that a failed network connection is
   * quickly detected.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void nop() throws IOException,
                                        DisorderParseError,
                                        DisorderProtocolError {
    connect();
    send("nop");
    getPositiveResponse();
  }

  /**
   * Get a track name part.
   *
   * <p><code>context</code> should either <code>"sort"</code> or
   * <code>"display"</code>.  <code>part</code> is generally either
   * <code>"artist"</code>, <code>"album"</code> or <code>"title"</code>.
   *
   * @param track Track to look up
   * @param context Context for the lookup
   * @param part Part of the track name to get
   * @return Track name part or the empty string
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String part(String track,
                                  String context,
                                  String part) throws IOException,
                                                      DisorderParseError,
                                                      DisorderProtocolError {
    connect();
    send("part %s %s %s", quote(track), quote(context), quote(part));
    return getStringResponse(false);
  }

  /**
   * Pause play.
   *
   * The opposite of {@link #resume()}.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void pause() throws IOException,
                                          DisorderParseError,
                                          DisorderProtocolError {
    connect();
    send("pause");
    getPositiveResponse();
  }

  /**
   * Add a track to the queue.
   *
   * @param track Track to queue
   * @return Track ID in queue
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String play(String track) throws IOException,
                                                       DisorderParseError,
                                                       DisorderProtocolError {
    connect();
    send("play %s", quote(track));
    return getStringResponse(false);
  }

  /**
   * Play multiple tracks.
   *
   * All of the listed tracks will be insered into the qeueu just
   * after the target, retaining their relative ordering within
   * themselves.
   *
   * <p><b>Note</b>: in current server implementations the underlying
   * command does not return new track IDs.  This may change in the
   * future.  In that case a future implementation of this method will
   * return a list of track IDs.  However in the mean time callers
   * <u>must</u> be able to cope with a <code>null</code> return from
   * this method.
   *
   * @param target Target track ID
   * @param tracks Tracks to add to the queue
   * @return Currently <code>null</code>, but see note above
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> playafter(String target,
                                             String... tracks)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    StringBuilder sb = new StringBuilder();
    for(int n = 0; n < tracks.length; ++n) {
      sb.append(' ');
      sb.append(quote(tracks[n]));
    }
    send("playafter %s %s", quote(target), sb.toString());
    getPositiveResponse();
    return null;
  }

  /**
   * Get the playing track.
   *
   * @return Information for the playing track or <code>null</code> if nothing is playing
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized TrackInformation playing()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playing");
    Vector<String> v = getPositiveResponse();
    if(v.get(0).equals("252"))
      return new TrackInformation(v, 1);
    else
      return null;
  }

  /**
   * Delete a playlist.
   *
   * @param playlist Playlist to delete
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void playlistDelete(String playlist)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-delete %s", quote(playlist));
    getPositiveResponse();
  }

  /**
   * Get the contents of a playlist.
   *
   * @param playlist Playlist to get
   * @return Playlist contents
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> playlistGet(String playlist)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-get %s", quote(playlist));
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Get the sharing status of a playlist.
   *
   * Possible sharing statuses are <code>"public"</code>,
   * <code>"private"</code> and <code>"shared"</code>.
   *
   * @param playlist Playlist to get sharing status for
   * @return Sharing status of playlist.
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String playlistGetShare(String playlist)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-get-share %s", quote(playlist));
    return getStringResponse(false);
  }

  /**
   * Lock a playlist.
   *
   * @param playlist Playlist to lock
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void playlistLock(String playlist)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-lock %s", quote(playlist));
    getPositiveResponse();
  }

  /**
   * Set the contents of a playlist.
   *
   * The playlist must be locked.
   *
   * @param playlist Playlist to modify
   * @param contents New contents
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void playlistSet(String playlist,
                                       List<String> contents)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-set %s", quote(playlist));
    for(String s: contents)
      send("%s%s", s.charAt(0) == '.' ? "." : "", s);
    send(".");
    // TODO send() will flush after every line which isn't very efficient
    getPositiveResponse();
  }

  /**
   * Set the sharing status of a playlist.
   *
   * The playlist must be locked.
   *
   * Possible sharing statuses are <code>"public"</code>,
   * <code>"private"</code> and <code>"shared"</code>.
   *
   * @param playlist Playlist to delete
   * @param share New sharing status
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void playlistSetShare(String playlist,
                                            String share)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-set-share %s %s", quote(playlist), quote(share));
    getPositiveResponse();
  }

  /**
   * Unlock the locked playlist.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void playlistUnlock(String playlist)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlist-unlock");
    getPositiveResponse();
  }

  /**
   * Get a list of playlists.
   *
   * Private playlists belonging to other users aren't included in the
   * result.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> playlists()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("playlists");
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Get all preferences for a track.
   *
   * @param track Track to get preferences for
   * @return Dictionary relating preference keys to values
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized Dictionary<String,String> prefs(String track)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("prefs %s", quote(track));
    getPositiveResponse();
    List<String> prefs = getResponseBody();
    Hashtable<String, String> r = new Hashtable<String, String>(prefs.size());
    for(String line: prefs) {
      Vector<String> v = DisorderMisc.split(line, false);
      String key = v.get(0);
      String value = v.get(1);
      r.put(key, value);
    }
    return r;
  }

  /**
   * Get the queue.
   *
   * <p>The queue is returned as a list of {@link TrackInformation}
   * objects.  The first element of the list is the head of the queue,
   * i.e. the track that will be played next (if the queue is not
   * modified).  The queue might be empty.
   *
   * @return A list representing the queue
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<TrackInformation> queue()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("queue");
    getPositiveResponse();
    List<String> queue = getResponseBody();
    Vector<TrackInformation> r = new Vector<TrackInformation>(queue.size());
    for(String line: queue)
      r.add(new TrackInformation(line));
    return r;
  }

  /**
   * Disable random play.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void randomDisable()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("random-disable");
    getPositiveResponse();
  }

  /**
   * Enable random play.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void randomEnable()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("random-enable");
    getPositiveResponse();
  }

  /**
   * Test whether random play is enabled.
   *
   * @return <code>true</code> if random play is enabled, otherwise <code>false</code>
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized boolean randomEnabled() throws IOException,
                                                     DisorderParseError,
                                                     DisorderProtocolError {
    connect();
    send("random-enabled");
    return getBooleanResponse();
  }

  /**
   * Get the recently played list.
   *
   * <p>The recently played is returned as a list of {@link
   * TrackInformation} objects.  The <u>last</u> element of the list
   * is the most recently played track.  It might be empty.
   *
   * @return The recently played list
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<TrackInformation> recent()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("recent");
    getPositiveResponse();
    List<String> recent = getResponseBody();
    Vector<TrackInformation> r = new Vector<TrackInformation>(recent.size());
    for(String line: recent)
      r.add(new TrackInformation(line));
    return r;
  }

  // TODO reconfigure not implemented because it only works on local connections
  // TODO register not implemented because it's only used by the CGI
  // TODO reminder not implemented because it's only used by the CGI

  
  /**
   * Remove a track form the queue
   *
   * @param id ID of track to remove
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void remove(String id)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("remove %s", quote(id));
    getPositiveResponse();
  }

  // TODO rescan not implemented


  /**
   * Resolve a track name.
   *
   * Returns the canonical name of <code>track</code>, i.e. if it's an
   * alias then returns target of the alisas.
   *
   * @param track Track to resolve
   * @return Canonical name of track
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String resolve(String track)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("resolve %s", quote(track));
    return getStringResponse(false);
  }

  /**
   * Resume play.
   *
   * The opposite of {@link #pause()}.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void resume()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("resume");
    getPositiveResponse();
  }

  // TODO revoke not implemented because only used by the CGI

  /**
   * An RTP broadcast address.
   *
   * This is used for the return value from {@link #rtpAddress()}.
   */
  public class RtpAddress {
    /**
     * The hostname or IP address.
     */
    public String host;

    /**
     * The port number
     */
    public int port;

    /**
     * Construct an RTP address from a host and port.
     */
    RtpAddress(String h, int p) {
      host = h;
      port = p;
    }
  }
  
  /**
   * Get the RTP address.
   *
   * @return A string representing the RTP broadcast address
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized RtpAddress rtpAddress()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("rtp-address");
    Vector<String> v = getPositiveResponse();
    String host = v.get(1);
    int port = Integer.parseInt(v.get(2));
    return new RtpAddress(host, port);
  }

  /**
   * Scratch a track.
   *
   * <p>If <code>id</code> is not <code>null</code> then that track is
   * scratched.  Otherwise the currently playing track is scratched.
   *
   * @param id Track to scratch or <code>null</code>
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void scratch(String id)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    if(id != null)
      send("scratch %s", quote(id));
    else
      send("scratch");
    getPositiveResponse();
  }

  // TODO schedule-* not implemented due to dubious utility

  /**
   * Search for tracks.
   *
   * <p>Search terms are broken up by spaces, with quoting allowed as
   * for the configuration file and commands.  Terms of the form
   * <code>tag:TAG</code> match tags, other terms match words in the
   * track name.  Terms are combined with “and” logic, i.e. only
   * tracks which match all terms are returned.
   *
   * @param terms Search terms
   * @return List of tracks matching search terms
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> search(String terms)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("search %s", quote(terms));
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Set a track preference value.
   *
   * @param track Track to modify
   * @param pref Preference name
   * @param value New preference value
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void set(String track,
                               String pref,
                               String value)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("set %s %s %s", quote(track), quote(pref), quote(value));
    getPositiveResponse();
  }

  /**
   * Set a global preference value.
   *
   * @param pref Preference name
   * @param value New preference value
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void setGlobal(String pref,
                                     String value)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("set-global %s %s", quote(pref), quote(value));
    getPositiveResponse();
  }

  /**
   * Get server statistics.
   *
   * @return List of server statistics
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> stats()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("stats");
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Get all tags.
   *
   * @return List of tags
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> tags()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("tags");
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Unset a track preference value.
   *
   * @param track Track to modify
   * @param pref Preference name
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void unset(String track,
                                 String pref)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("set %s %s", quote(track), quote(pref));
    getPositiveResponse();
  }

  /**
   * Unset a global preference value.
   *
   * @param pref Preference name
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized void unsetGlobal(String pref)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("unset-global %s", quote(pref));
    getPositiveResponse();
  }

  /**
   * Get a user property.
   *
   * @param user User to look up
   * @param property Property name
   * @return Property value, or <code>null</code> if it's not set
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String userinfo(String user,
                                      String property)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("userinfo %s %s", quote(user), quote(property));
    return getStringResponse(true);
  }

  /**
   * Get a list of users.
   *
   * @return List of users
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized List<String> users()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("users");
    getPositiveResponse();
    return getResponseBody();
  }

  /**
   * Get the server version.
   *
   * <p>The version string will generally be in one of the following formats:
   * <ul>
   * <li><code>x.y</code> - a major release
   * <li><code>x.y.z</code> - a minor release
   * <li><code>x.y+</code> - an intermediate development version
   * </ul>
   *
   * @return Server version string
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized String version()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("version");
    return getPositiveResponse().get(1);
  }

  /**
   * Get the volume.
   *
   * <p>The returned array has the left and right channel volumes in
   * that order.
   *
   * @return The current volume setting
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized int[] volume()
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("volume");
    Vector<String> v = getPositiveResponse();
    int[] r = new int[2];
    r[0] = Integer.parseInt(v.get(1));
    r[1] = Integer.parseInt(v.get(2));
    return r;
  }

  /**
   * Set the volume.
   *
   * <p>The returned array has the left and right channel volumes in
   * that order.
   *
   * @return The current volume setting
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public synchronized int[] volume(int left, int right)
    throws IOException,
           DisorderParseError,
           DisorderProtocolError {
    connect();
    send("volume %d %d", left, right);
    Vector<String> v = getPositiveResponse();
    int[] r = new int[2];
    r[0] = Integer.parseInt(v.get(1));
    r[1] = Integer.parseInt(v.get(2));
    return r;
  }
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
