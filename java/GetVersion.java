import uk.org.greenend.disorder.*;
import java.io.*;

class GetVersion {
  public static void main(String[] args) throws DisorderParseError,
                                                DisorderProtocolError,
                                                IOException {
    DisorderServer d = new DisorderServer();
    String v = d.version();
    System.out.println(v);
  }
}
/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
