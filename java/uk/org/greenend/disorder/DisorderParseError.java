package uk.org.greenend.disorder;

/**
 * Parse error exception class
 */
public class DisorderParseError extends Exception {
  /**
   * Constructs a new parse error with an error message.
   *
   * @param message The error message
   */
  public DisorderParseError(String message) {
    super(message);
  }
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
