using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace uk.org.greenend.DisOrder
{
  /// <summary>
  /// Configuration for DisOrder
  /// </summary>
  /// <remarks>
  /// <para>Only a very small subset of DisOrder configuration is supported: username,
  /// password and host/port to connect to.</para>
  /// <para>Configuration is stored in %APPDATA%/DisOrder/passwd.</para>
  /// <para>You must use the Read() method to load configuration before reading the configuration properties.</para>
  /// <para>If you change the configuration properties you can use Write() to persist the new values.</para>
  /// </remarks>
  public class Configuration
  {
    #region Public Properties
    /// <summary>
    /// Path to user configuration file
    /// </summary>
    public string UserPath { get; set; }

    /// <summary>
    /// Username to pass to server
    /// </summary>
    public string Username { get; set; }

    /// <summary>
    /// Password to pass to server
    /// </summary>
    public string Password { get; set; }

    /// <summary>
    /// Hostname or address of server
    /// </summary>
    public string Address { get; set; }

    /// <summary>
    /// Port number of server
    /// </summary>
    public int Port { get; set; }
    #endregion

    /// <summary>
    /// Construct a configuration object
    /// </summary>
    public Configuration()
    {
      string appdata = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
      UserPath = Path.Combine(appdata, "DisOrder\\passwd");
    }

    /// <summary>
    /// Read the configuration
    /// </summary>
    public void Read()
    {
      // There is currently no support for global configuration
      Read(UserPath);
    }

    /// <summary>
    /// Save user configuration
    /// </summary>
    public void Write()
    {
      if (File.Exists(UserPath)) {
        File.Copy(UserPath, UserPath + ".bak", true);
      }
      using (StreamWriter sw = new StreamWriter(UserPath)) {
        if (Username != null) {
          sw.WriteLine("username {0}", Quote(Username));
        }
        if (Password != null) {
          sw.WriteLine("password {0}", Quote(Password));
        }
        if (Address != null) {
          sw.WriteLine("connect {0} {1}", Quote(Address), Port);
        }
      }
    }

    private void Read(string path)
    {
      using (StreamReader sr = new StreamReader(path)) {
        string s;
        while ((s = sr.ReadLine()) != null) {
          ProcessLine(s);
        }
      }
    }

    private void ProcessLine(string line)
    {
      IList<string> bits = Split(line, true/*comments*/);
      if (bits.Count() == 0)
        return;
      if (bits[0] == "username") {
        if (bits.Count() != 2)
          throw new Exception("wrong number of arguments to 'username'");
        Username = bits[1];
      }
      else if (bits[0] == "password") {
        if (bits.Count() != 2)
          throw new Exception("wrong number of arguments to 'password'");
        Password = bits[1];
      }
      else if (bits[0] == "connect") {
        if (bits.Count() != 3)
          throw new Exception("wrong number of arguments to 'connect'");
        int port;
        if (!int.TryParse(bits[2], out port))
          throw new Exception("cannot parse port number");
        if (port < 1 || port > 65535)
          throw new Exception("port number out of range");
        Address = bits[1];
        Port = port;
      }
      else
        throw new Exception("unrecognized configuration command");
    }

    #region String splitting
    static private bool isspace(char c)
    {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static internal IList<string> Split(string s, bool comments)
    {
      int pos = 0;
      List<string> bits = new List<string>();
      while (pos < s.Length) {
        if (isspace(s[pos])) {
          ++pos;
          continue;
        }
        if (comments && s[pos] == '#') {
          break;
        }
        if (s[pos] == '\'' || s[pos] == '"') {
          StringBuilder sb = new StringBuilder();
          char q = s[pos++];
          while (pos < s.Length && s[pos] != q) {
            if (s[pos] == '\\') {
              ++pos;
              if (pos >= s.Length)
                throw new Exception("unterminated quoted string"); // TODO exception type
              switch (s[pos]) {
                case '\'':
                case '\\':
                case '"':
                  sb.Append(s[pos++]);
                  break;
                case 'n':
                  sb.Append('\n');
                  ++pos;
                  break;
                default:
                  throw new Exception("invalid escape sequence"); // TODO exception type
              }
            }
            else
              sb.Append(s[pos++]);
          }
          bits.Add(sb.ToString());
        }
        else {
          int start = pos;
          while (pos < s.Length && !isspace(s[pos]))
            ++pos;
          bits.Add(s.Substring(start, pos - start));
        }
      }
      return bits;
    }

    internal static string Quote(string s)
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

    #endregion

  }
}
