package uk.org.greenend.disorder;

/**
 * Protocol error exception class
 */
public class DisorderProtocolError extends Exception {
  /**
   * Constructs a new protocol error with an error message.
   *
   * @param serverName The server that reported the error
   * @param message The error message
   */
  public DisorderProtocolError(String serverName, String message) {
    super(serverName + ": " + message);
  }
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
