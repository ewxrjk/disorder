using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Threading;

namespace uk.org.greenend.DisOrder
{
  /// <summary>
  /// A connection to a DisOrder server
  /// </summary>
  /// <remarks><para>
  /// You can use a connection object from multiple threads safely.  Requests are pipelined,
  /// so concurrent usage is relatively efficient.</para></remarks>
  public partial class Connection: IDisposable
  {
    #region Public properties
    /// <summary>
    /// Configuration object to use
    /// </summary>
    /// <remarks><para>Optional; if one is not set then one will be created when necessary.</para></remarks>
    public Configuration Configuration { get; set; }
    #endregion

    #region Private state
    private TextReader reader = null;
    private TextWriter writer = null;
    private Socket socket = null;
    private object monitor = new object();
    private UInt64 nextRequest = 0; // ID for next request sent
    private UInt64 nextResponse = 0; // ID for next response received
    // Careful - nextRequest and nextResponse cannot safely be compared for order;
    // only for equality.  The ID after 0xffffffffffffffff is 0.

    private Dictionary<string, string> Hashes = new Dictionary<string, string>()
    {
      { "sha1", "SHA1" },
      { "SHA1", "SHA1" },
      { "sha256", "SHA256" },
      { "SHA256", "SHA256" },
      { "sha384", "SHA384" },
      { "SHA384", "SHA384" },
      { "sha512", "SHA512" },
      { "SHA512", "SHA512" },
    };

    private Encoding Encoding = new UTF8Encoding(false);
    #endregion

    #region Connection Management
    /// <summary>
    /// Connect to the server
    /// </summary>
    /// <remarks>
    /// <para>If already connected, does nothing.</para>
    /// <para>This is automatically called when necessary.</para></remarks>
    public void Connect()
    {
      lock (monitor) {
        ConnectLocked();
      }
    }

    private void ConnectLocked()
    {
      if (writer != null)
        return;
      if (Configuration == null) {
        Configuration = new Configuration();
        Configuration.Read();
      }
      socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
      try {
        // Connect to the server
        socket.Connect(Configuration.Address, Configuration.Port);
        NetworkStream ns = new NetworkStream(socket);
        reader = new StreamReader(ns, Encoding);
        writer = new StreamWriter(ns, Encoding);
        // Get protocol version and challenge
        IList<string> greeting = Configuration.Split(WaitLocked(), false);
        if (greeting.Count < 1 || greeting[0] != "2")
          throw new Exception("unrecognized protocol generation");
        if (greeting.Count() != 3)
          throw new Exception("malformed server greeting");
        string algorithm = greeting[1], challenge = greeting[2];
        // Put together the response
        if (!Hashes.ContainsKey(algorithm))
          throw new Exception("server requested unrecognized hash algorithm");
        List<byte> input = new List<byte>();
        input.AddRange(Encoding.UTF8.GetBytes(Configuration.Password));
        input.AddRange(FromHex(challenge));
        using (System.Security.Cryptography.HashAlgorithm hash = System.Security.Cryptography.HashAlgorithm.Create(Hashes[algorithm])) {
          string response = ToHex(hash.ComputeHash(input.ToArray()));
          // Send the response, WaitLocked will throw if user or password wrong
          SendLocked(new object[] { "user", Configuration.Username, response });
          WaitLocked();
        }
      } catch {
        // If anything went wrong, tear it all down
        DisconnectLocked();
        throw;
      }
    }

    /// <summary>
    /// Disconnect from the server
    /// </summary>
    /// <remarks>
    /// <para>If not connected, does nothing.</para>
    /// <para>If any requests are outstanding, blocks until they are complete.</para>
    /// </remarks>
    public void Disconnect()
    {
      lock (monitor) {
        while (nextRequest != nextResponse) {
          Monitor.Wait(monitor);
        }
        DisconnectLocked();
      }
    }

    private void DisconnectLocked()
    {
      if (reader != null) {
        reader.Dispose();
        reader = null;
      }
      if (writer != null) {
        writer.Dispose();
        writer = null;
      }
      if (socket != null) {
        socket.Dispose();
        socket = null;
      }
    }
    #endregion

    #region Protocol IO
    private string Transact(params object[] command)
    {
      string response;
      lock (monitor) {
        // Don't consume the request ID until after SendLocked has returned,
        // so that the response/request IDs aren't desynchronized on error.
        SendLocked(command);
        UInt64 id = nextRequest++;
        try {
          // Wait for this thread's turn.
          while (nextResponse != id) {
            Monitor.Wait(monitor);
          }
          response = WaitLocked();
        }
        finally {
          // Having consumed a request ID we must be sure to consume
          // the response ID too.
          nextResponse++;
          // Awaken other threads running Transact or Disconnect().
          Monitor.PulseAll(monitor);
        }
      }
      return response;
    }

    private void SendLocked(object[] command)
    {
      if (writer == null)
        ConnectLocked();
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < command.Length; ++i) {
        if (i > 0)
          sb.Append(' '); // separator
        object o = command[i];
        if (o is string)
          sb.Append(Quote((string)o));
        else if (o is int)
          sb.AppendFormat("{0}", (int)o);
        else if (o is DateTime)
          sb.AppendFormat("{0}", DateTimeToUnix((DateTime)o));
        else
          throw new NotImplementedException();
      }
      sb.Append('\n'); // Terminator
      try {
        writer.Write(sb.ToString());
        writer.Flush();
      }
      catch {
        // If an IO error occurs, disconnect.
        DisconnectLocked();
        throw;
      }
    }

    private string WaitLocked()
    {
      StringBuilder sb = new StringBuilder();
      int c;
      try {
        while ((c = reader.Read()) != -1 && c != '\n') {
          sb.Append((char)c);
        }
        if (c == -1)
          throw new Exception("unterminated line received from server");
      } catch {
        // If an IO error occurs, disconnect.
        DisconnectLocked();
        throw;
      }
      string response = sb.ToString();
      // Extract the initial code
      int rc;
      if (response.Length >= 3 && int.TryParse(response.Substring(0, 3), out rc)) {
        if (rc / 100 != 2)
          throw new Exception("error from server: " + response);
        else if (rc % 10 == 9)
          return null;
        else
          return response.Substring(4);
      } 
      else
        throw new Exception("malformed line received from server");
    }

    private static string Quote(string s)
    {
      bool needQuote = (s.Length == 0);
      foreach (char c in s) {
        if (c <= ' ' || c == '"' || c == '\\' || c == '\'' || c == '#') {
          needQuote = true;
          break;
        }
      }
      if (!needQuote)
        return s;
      StringBuilder sb = new StringBuilder();
      foreach (char c in s) {
        switch (c) {
          case '"':
          case '\\':
            sb.Append('\\');
            sb.Append(c);
            break;
          case '\n':
            sb.Append("\\n");
            break;
          default:
            sb.Append(c);
            break;
        }
      }
      return sb.ToString();
    }

    private static long DateTimeToUnix(DateTime dt)
    {
      DateTime zero = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
      return (long)(dt - zero).TotalSeconds;
    }
    #endregion

    #region Hexadecimal Conversion
    private IList<byte> FromHex(string s)
    {
      List<byte> bytes = new List<byte>();
      for (int i = 0; i < s.Length; i += 2) {
        bytes.Add((byte)int.Parse(s.Substring(i, 2), System.Globalization.NumberStyles.HexNumber));
      }
      return bytes;
    }

    private string ToHex(byte[] bytes)
    {
      StringBuilder sb = new StringBuilder();
      foreach (byte b in bytes) {
        sb.Append(string.Format("{0:x2}", b));
      }
      return sb.ToString();
    }
    #endregion

    #region Dispose logic
    public void Dispose()
    {
      Disconnect();
    }
    #endregion
  }
}
