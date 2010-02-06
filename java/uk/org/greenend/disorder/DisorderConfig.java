package uk.org.greenend.disorder;

import java.io.*;
import java.util.*;

/** 
 * Container for local DisOrder configuration.
 *
 * Only configuration settings relevant to the client are supported.  Everything
 * else is ignored.
 */
public class DisorderConfig {
  /**
   * Address family to connect to.
   *
   * <ul>
   * <li>0 to use either IPv4 or IPv6
   * <li>4 to use only IPv4
   * <li>6 to use only IPv6
   * </ul>
   */
  public int addressFamily;

  /**
   * Server hostname.
   */
  public String serverName;

  /**
   * Server port number.
   */
  public int serverPort;

  /**
   * Username.
   */
  public String user;

  /**
   * Password.
   */
  public String password;

  /**
   * Constructs a new configuration from configuration files.
   *
   * The configuration files read are as follows:
   * <ul>
   * <li><tt>/etc/disorder/config</tt> - the global configuration
   * <li><tt>~/.disorder/password</tt> - the user's personal configuration
   * </ul>
   *
   * @throws DisorderParseError If a configuration file contains a syntax error
   * @throws IOException If an error occurs reading a configuration file
   */
  public DisorderConfig() throws DisorderParseError,
                                 IOException {
    // TODO cope with nonstandard installation location
    // TODO cope with windows locations
    // TODO read from registry if windows?
    readConfig("/etc/disorder/config");
    String home = System.getenv("HOME");
    if(home != null)
      readConfig(home + "/.disorder/passwd");
    addressFamily = 0;
  }

  /**
   * Read a configuration file.
   *
   * @param path Path to configuration file to read
   * @throws DisorderParseError If a configuration file contains a syntax error
   * @throws IOException If an error occurs reading a configuration file
   */
  private void readConfig(String path) throws DisorderParseError,
                                              IOException {
    BufferedReader f;
    try {
      f = new BufferedReader(new FileReader(path));
    } catch(FileNotFoundException ex) {
      // Ignore files that don't exist
      return;
    }
    String l;
    int line = 0;
    while((l = f.readLine()) != null) {
      ++line;
      try {
        Vector<String> v = DisorderMisc.split(l, true/*comments*/);
        if(v.size() > 0)
          processLine(v);
      } catch(DisorderParseError e) {
        // Insert the error location into the error message
        throw new DisorderParseError(path + ":" + line + ": " + e.toString());
      }
    }
    f.close();
  }

  /**
   * Process one directive in a configuration file.
   *
   * @param v A <b>nonempty</b> parsed line
   * @throws DisorderParseError If a configuration file contains a syntax error
   */
  private void processLine(Vector<String> v) throws DisorderParseError {
    String cmd = v.get(0);
    if(cmd.equals("connect")) {
      // connect [-4|-6] HOST PORT
      switch(v.size()) {
      case 3:
        addressFamily = 0;
        serverName = v.get(1);
        serverPort = Integer.parseInt(v.get(2));
        break;
      case 4:
        if(v.get(1).equals("-4"))
          addressFamily = 4;
        else if(v.get(1).equals("-6"))
          addressFamily = 6;
        else if(v.get(1).equals("-"))
          addressFamily = 0;
        else
          throw new DisorderParseError("invalid address family");
        serverName = v.get(2);
        serverPort = Integer.parseInt(v.get(3));
        break;
      default:
        throw new DisorderParseError("syntax error");
      }
    } else if(cmd.equals("password")) {
      // password PASSWORD
      if(v.size() != 2)
        throw new DisorderParseError("syntax error");
      password = v.get(1);
    } else if(cmd.equals("username")) {
      // username USERNAME
      if(v.size() != 2)
        throw new DisorderParseError("syntax error");
      user = v.get(1);
    }
    // Anything else is ignored.
  }
}
/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
