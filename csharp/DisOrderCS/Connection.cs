using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace uk.org.greenend.DisOrder
{
  /// <summary>
  /// A connection to a DisOrder server
  /// </summary>
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
        IList<string> greeting = Configuration.Split(Wait(), false);
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
          // Send the response, Transact will throw if user or password wrong
          Transact("user", Configuration.Username, response);
        }
      } catch {
        // If anything went wrong, tear it all down
        Disconnect();
        throw;
      }
    }

    /// <summary>
    /// Disconnect from the server
    /// </summary>
    /// <remarks><para>If not connected, does nothing.</para></remarks>
    public void Disconnect()
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
      if (writer == null)
        Connect();
      for (int i = 0; i < command.Length; ++i) {
        if (i > 0)
          writer.Write(' '); // separator
        object o = command[i];
        if (o is string)
          writer.Write(Quote((string)o));
        else if (o is int)
          writer.Write((int)o);
        else if (o is DateTime)
          writer.Write(DateTimeToUnix((DateTime)o));
        else
          throw new NotImplementedException();
      }
      writer.Write('\n'); // terminator
      writer.Flush();
      return Wait();
    }

    private string Wait()
    {
      StringBuilder sb = new StringBuilder();
      int c;
      while ((c = reader.Read()) != -1 && c != '\n') {
        sb.Append((char)c);
      }
      if (c == -1)
        throw new Exception("unterminated line received from server");
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
