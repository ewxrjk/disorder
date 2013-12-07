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
    private TextWriter writer = null;
    private Stream inputStream = null;
    private byte[] inputBuffer = new byte[4096];
    private int inputOffset = 0, inputLimit = 0;
    private CancellationTokenSource inputCancel = new CancellationTokenSource();
    private Socket socket = null;
    private object monitor = new object();
    private UInt64 nextRequest = 0; // ID for next request sent
    private UInt64 nextResponse = 0; // ID for next response received
    private DateTime lastConnect = new DateTime();
    // Careful - nextRequest and nextResponse cannot safely be compared for order;
    // only for equality.  The ID after 0xffffffffffffffff is 0.
    private bool waiting = false; // Some thread is block in Wait()

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
      // Crude rate-limiting of reconnection
      DateTime now = DateTime.UtcNow;
      if (now.Subtract(lastConnect).TotalSeconds < 1)
        Thread.Sleep(1000);
      lastConnect = now;
      socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
      try {
        // Connect to the server
        socket.Connect(Configuration.Address, Configuration.Port);
        inputStream = new NetworkStream(socket);
        inputOffset = 0;
        inputLimit = 0;
        writer = new StreamWriter(inputStream, Encoding);
        // Get protocol version and challenge
        string response;
        int rc = Wait(out response);
        IList<string> greeting;
        try {
          greeting = Utils.Split(response, false);
        }
        catch (InvalidStringException e) {
          throw new InvalidServerResponseException(e.Message, e);
        }
        if (greeting.Count < 1 || greeting[0] != "2")
          throw new InvalidServerResponseException("unrecognized protocol generation");
        if (greeting.Count() != 3)
          throw new InvalidServerResponseException("malformed server greeting");
        string algorithm = greeting[1], challenge = greeting[2];
        // Put together the response
        if (!Hashes.ContainsKey(algorithm))
          throw new InvalidServerResponseException("server requested unrecognized hash algorithm");
        List<byte> input = new List<byte>();
        input.AddRange(Encoding.UTF8.GetBytes(Configuration.Password));
        input.AddRange(Utils.FromHex(challenge));
        using (System.Security.Cryptography.HashAlgorithm hash = System.Security.Cryptography.HashAlgorithm.Create(Hashes[algorithm])) {
          string hashed = Utils.ToHex(hash.ComputeHash(input.ToArray()));
          // Send the response, WaitLocked will throw if user or password wrong
          SendLocked(new object[] { "user", Configuration.Username, hashed });
          Wait(out response);
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
    /// <para>Outstanding requests in other threads will fail.</para>
    /// </remarks>
    public void Disconnect()
    {
      lock (monitor) {
        // If some thread is blocked in input, interrupt it
        if (waiting) {
          inputCancel.Cancel();
          inputCancel = new CancellationTokenSource();
        }
        // Wait until nothing is waiting or queued
        while (waiting || nextRequest != nextResponse) {
          Monitor.Wait(monitor);
        }
        // Actually close the connection
        DisconnectLocked();
      }
    }

    private void DisconnectLocked()
    {
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

    #region Input buffering
    private void InputFill(CancellationToken ct)
    {
      Task<int> t = inputStream.ReadAsync(inputBuffer, 0, inputBuffer.Count(), ct);
      t.Wait(ct);
      if (t.Status == TaskStatus.Canceled)
        throw new OperationCanceledException();
      int bytes = t.Result;
      if (bytes == 0)
        throw new InvalidServerResponseException("server closed connection");
      inputLimit = bytes;
      inputOffset = 0;
    }

    private int InputByte(CancellationToken ct)
    {
      if (inputOffset >= inputLimit)
        InputFill(ct);
      return inputBuffer[inputOffset++];
    }

    private string InputLine(CancellationToken ct)
    {
      List<byte> line = new List<byte>();
      int c;
      while ((c = InputByte(ct)) >= 0 && c != 0x0A)
        line.Add((byte)c);
      return Encoding.UTF8.GetString(line.ToArray());
    }
    #endregion

    #region Protocol IO
    private int Transact(out string response, out IList<string> lines, params object[] command)
    {
      lock (monitor) {
        return TransactLocked(out response, out lines, command);
      }
    }

    private int TransactLocked(out string response, out IList<string> body, object[] command)
    {
      int rc;
      // Don't consume the request ID until after SendLocked has returned,
      // so that the response/request IDs aren't desynchronized on error.
      SendLocked(command);
      Socket expectedSocket = socket;
      UInt64 id = nextRequest++;
      try {
        // Wait for this thread's turn.
        while (nextResponse != id) {
          Monitor.Wait(monitor);
        }
        if (!object.ReferenceEquals(socket, expectedSocket)) {
          // Automatic retries would need to be implemented in here somewhere
          throw new InvalidServerResponseException("lost connection");
        }
        waiting = true;
        Monitor.Exit(monitor);
        try {
          rc = Wait(out response, out body);
        }
        finally {
          Monitor.Enter(monitor);
          waiting = false;
        }
      }
      finally {
        // Having consumed a request ID we must be sure to consume
        // the response ID too.
        nextResponse++;
        // Awaken other threads running Transact or Disconnect().
        Monitor.PulseAll(monitor);
      }
      return rc;
    }

    private class SendAsBody
    {
      public IList<string> Body;
      public SendAsBody(IList<string> s)
      {
        Body = s;
      }
    }

    private void SendLocked(object[] command)
    {
      if (writer == null)
        ConnectLocked();
      StringBuilder sb = new StringBuilder();
      IList<String> body = null;
      for (int i = 0; i < command.Length; ++i) {
        if (i > 0)
          sb.Append(' '); // separator
        object o = command[i];
        if (o is string)
          sb.Append(Utils.Quote((string)o));
        else if (o is int)
          sb.AppendFormat("{0}", (int)o);
        else if (o is DateTime)
          sb.AppendFormat("{0}", Utils.DateTimeToUnix((DateTime)o));
        else if (o is IList<string>) {
          IList<string> l = (IList<string>)o;
          for (int j = 0; j < l.Count; ++j) {
            if (j > 0)
              sb.Append(' '); // separator
            sb.Append(Utils.Quote(l[j]));
          }
        }
        else if(o is SendAsBody)
          body = ((SendAsBody)o).Body;
        else
          throw new NotImplementedException();
      }
      sb.Append('\n'); // Terminator
      if (body != null) {
        foreach (string line in body) {
          if (line.Length > 0 && line[0] == '.')
            sb.Append('.');
          sb.Append(line);
          sb.Append('\n');
        }
        sb.Append(".\n");
      }
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

    private string ReadLine()
    {
      try {
        return InputLine(inputCancel.Token);
      }
      catch {
        // If an IO error occurs, disconnect.
        DisconnectLocked();
        throw;
      }
    }

    private int Wait(out string response)
    {
      IList<string> body;
      return Wait(out response, out body);
    }

    private int Wait(out string response, out IList<string> body)
    {
      string line = ReadLine();
      // Extract the initial code
      int rc;
      if (line.Length >= 3 && int.TryParse(line.Substring(0, 3), out rc)) {
        if (rc / 100 != 2)
          throw new InvalidStringException("error from server: " + line);
        else
          response = line.Substring(4);
        if (rc % 100 == 3)
          body = WaitBody();
        else
          body = null;
        return rc;
      } 
      else
        throw new InvalidServerResponseException("malformed line received from server");
    }

    private IList<string> WaitBody()
    {
      List<string> lines = new List<string>();
      string line;
      while ((line = ReadLine()) != ".") {
        if (line[0] != '.')
          lines.Add(line);
        else
          lines.Add(line.Substring(1));
      }
      return lines;
    }

    static private IList<QueueEntry> BodyToQueue(IList<string> lines)
    {
      List<QueueEntry> queue = new List<QueueEntry>();
      foreach (string line in lines) {
        queue.Add(new QueueEntry(line));
      }
      return queue;
    }

    static private IDictionary<string, string> BodyToPairs(IList<string> lines)
    {
      Dictionary<string, string> pairs = new Dictionary<string, string>();
      foreach (string line in lines) {
        IList<string> bits = Utils.Split(line, false);
        if(bits.Count != 2)
          throw new InvalidServerResponseException("malformed pair received from server");
        pairs.Add(bits[0], bits[1]);
      }
      return pairs;
    }
    #endregion

    /// <summary>
    /// Monitor the server log
    /// </summary>
    /// <param name="consumer">Callback instance</param>
    /// <remarks><para>This method hogs the connection; other attempts to use the same connection will block.</para>
    /// <para>When the connection closes (e.g. due to a network error or the server terminating) this method
    /// will return.  The connection can then be used in the normal way.</para></remarks>
    public void Log(LogConsumer consumer)
    {
      lock (monitor) {
        try {
          string response;
          IList<string> body;
          TransactLocked(out response, out body, new object[] { "log" });
          for (; ; ) {
            string line = ReadLine();
            IList<string> bits;
            try {
              bits = Utils.Split(line, false);
            }
            catch (InvalidStringException e) {
              throw new InvalidServerResponseException(e.Message, e);
            }
            if (bits.Count < 2)
              throw new InvalidServerResponseException("malformed server log response");
            DateTime when = Utils.UnixToDateTime(long.Parse(bits[0], System.Globalization.NumberStyles.HexNumber));
            string what = bits[1];
            if (what == "adopted") consumer.Adopted(when, bits[2], bits[3]);
            else if (what == "completed") consumer.Completed(when, bits[2]);
            else if (what == "failed") consumer.Failed(when, bits[2], bits[3]);
            else if (what == "moved") consumer.Moved(when, bits[2]);
            else if (what == "playing") consumer.Playing(when, bits[2], bits.Count() > 3 ? bits[3] : null);
            else if (what == "queue") consumer.Queue(when, new QueueEntry(bits, 2));
            else if (what == "recent_added") consumer.RecentAdded(when, new QueueEntry(bits, 2));
            else if (what == "recent_removed") consumer.RecentRemoved(when, bits[2]);
            else if (what == "removed") consumer.Removed(when, bits[2], bits.Count() > 3 ? bits[3] : null);
            else if (what == "scratched") consumer.Scratched(when, bits[2], bits[3]);
            else if (what == "state") consumer.State(when, bits[2]);
            else if (what == "user_add") consumer.UserAdd(when, bits[2]);
            else if (what == "user_delete") consumer.UserDelete(when, bits[2]);
            else if (what == "user_edit") consumer.UserEdit(when, bits[2], bits[3]);
            else if (what == "user_confirm") consumer.UserConfirm(when, bits[2]);
            else if (what == "volume") consumer.Volume(when, int.Parse(bits[2]), int.Parse(bits[3]));
            // ...unknown entries ignored...
          }
        }
        catch {
          // Mustn't exit in a state where the server is still sending log messages
          DisconnectLocked();
          throw;
        }
      }
    }

    #region Dispose logic
    public void Dispose()
    {
      Disconnect();
    }
    #endregion
  }
}
