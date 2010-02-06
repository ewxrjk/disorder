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
 */
public class DisorderServer {
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

  /** Connect to server.
   *
   * If the connection is already established then nothing happens.
   *
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public void connect() throws IOException,
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
      send("user %s %s\n", config.user, response);
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
   * Get the server version.
   *
   * @return Server version string
   * @throws IOException If a network IO error occurs
   * @throws DisorderParseError If a malformed response was received
   * @throws DisorderProtocolError If the server sends an error response
   */
  public String version() throws IOException,
                                 DisorderParseError,
                                 DisorderProtocolError {
    connect();
    send("version\n");
    String response = getResponse();
    Vector<String> v = DisorderMisc.split(response, false/*comments*/);
    int rc = responseCode(v);
    if(rc / 100 != 2)
      throw new DisorderProtocolError(config.serverName, response);
    return v.get(1);
  }

  private void send(String format, Object ... args) {
    if(formatter == null)
      formatter = new Formatter();
    formatter.format(format, args);
    StringBuilder sb = (StringBuilder)formatter.out();
    String s = sb.toString();
    if(debugging) {
      System.err.print("SEND: ");
      System.err.print(s);
    }
    output.print(s);
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
  private int responseCode(Vector<String> v) throws DisorderParseError {
    if(v.size() < 1)
      throw new DisorderParseError("empty response");
    int rc = Integer.parseInt(v.get(0));
    if(rc < 0 || rc > 999)
      throw new DisorderParseError("invalid response code " + v.get(0));
    return rc;
  }
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
