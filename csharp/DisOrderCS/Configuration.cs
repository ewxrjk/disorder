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

    /// <summary>
    /// Local RTP port
    /// </summary>
    public int RtpLocal { get; set; }
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
          sw.WriteLine("username {0}", Utils.Quote(Username));
        }
        if (Password != null) {
          sw.WriteLine("password {0}", Utils.Quote(Password));
        }
        if (Address != null) {
          sw.WriteLine("connect {0} {1}", Utils.Quote(Address), Port);
        }
        if (RtpLocal != 0) {
          sw.WriteLine("rtp_local {0}", RtpLocal);
        }
      }
    }

    private void Read(string path)
    {
      using (StreamReader sr = new StreamReader(path)) {
        string s;
        int line = 1;
        while ((s = sr.ReadLine()) != null) {
          try {
            try {
              ProcessLine(s);
            }
            catch (InvalidStringException e) {
              throw new InvalidConfigurationException(e.Message, e);
            }
          }
          catch (InvalidConfigurationException e) {
            e.Path = path;
            e.Line = line;
            throw;
          }
          ++line;
        }
      }
    }

    private void ProcessLine(string line)
    {
      IList<string> bits = Utils.Split(line, true/*comments*/);
      if (bits.Count() == 0)
        return;
      if (bits[0] == "username") {
        if (bits.Count() != 2)
          throw new InvalidConfigurationException("wrong number of arguments to 'username'");
        Username = bits[1];
      }
      else if (bits[0] == "password") {
        if (bits.Count() != 2)
          throw new InvalidConfigurationException("wrong number of arguments to 'password'");
        Password = bits[1];
      }
      else if (bits[0] == "connect") {
        if (bits.Count() != 3)
          throw new InvalidConfigurationException("wrong number of arguments to 'connect'");
        int port;
        if (!int.TryParse(bits[2], out port))
          throw new InvalidConfigurationException("port number is not a number");
        if (port < 1 || port > 65535)
          throw new InvalidConfigurationException("port number out of range");
        Address = bits[1];
        Port = port;
      }
      else if (bits[0] == "rtp_local") {
        if (bits.Count() != 2)
          throw new InvalidConfigurationException("wrong number of arguments to 'rtp_local'");
        int port;
        if (!int.TryParse(bits[2], out port))
          throw new InvalidConfigurationException("port number is not a number");
        if (port < 1 || port > 65535)
          throw new InvalidConfigurationException("port number out of range");
        RtpLocal = port;
      }
      else
        throw new InvalidConfigurationException("unrecognized configuration command");
    }
  }
}
