using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using uk.org.greenend.DisOrder;
using System.Threading;
using System.Windows.Interop;
using System.Net.Sockets;
using System.Net;
using System.Net.NetworkInformation;

namespace DisOrderClient
{
  /// <summary>
  /// Interaction logic for MainWindow.xaml
  /// </summary>
  public partial class MainWindow : Window
  {
    private Configuration Configuration = new Configuration();
    private Connection Connection;
    private Connection LogConnection;

    #region Initialization
    public MainWindow()
    {
      InitializeComponent();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
      Configuration.Read();
      Connection = new Connection() { Configuration = Configuration };
      LogConnection = new Connection() { Configuration = Configuration };
      ThreadPool.QueueUserWorkItem((_) => { MonitorLog(); });
      int rc = dxbridge.dxbridgeInit((int)new WindowInteropHelper(this).Handle);
      if (rc != 0)
        MessageBox.Show(string.Format("dxbridge init returned {0}", rc),
                        "dxbridge error",
                        MessageBoxButton.OK,
                        MessageBoxImage.Error);
    }
    #endregion

    #region Commands
    private void About(object sender, RoutedEventArgs e)
    {
      About about = new About()
      {
        Owner = this,
        Connection = Connection
      };
      about.ShowDialog();
    }

    public static RoutedCommand ConnectCommand = new RoutedCommand();
    private void ConnectExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      ThreadPool.QueueUserWorkItem((_) =>
      {
        try {
          Connection.Connect();
        }
        catch {
        }
      });
    }

    public static RoutedCommand DisconnectCommand = new RoutedCommand();
    private void DisconnectExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      ThreadPool.QueueUserWorkItem((_) =>
      {
        try {
          Connection.Disconnect();
        }
        catch {
        }
      });
    }

    public static RoutedCommand OptionsCommand = new RoutedCommand();
    private void OptionsExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      Options options = new Options()
      {
        Owner = this,
        Configuration = Configuration,
        Reconfigure = () =>
        {
          // Tear down connections if user changes config
          Connection.Disconnect();
          LogConnection.Disconnect();
        },
      };
      options.ShowDialog();
    }

    private void CloseExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      Connection.Disconnect();
      this.Close();
    }

    private void RtpCheck(object sender, RoutedEventArgs e)
    {
      StartNetworkPlay();
    }
    private void RtpUncheck(object sender, RoutedEventArgs e)
    {
      StopNetworkPlay();
    }
    #endregion

    #region Server Monitoring
    string CurrentTrack = null;
    private void MonitorLog()
    {
      LogConsumer lc = new LogConsumer()
      {
        //OnState would be better but the server doesn't do what the protocol does imply (at least as of 5.1.1)
        OnPlaying = (when, track, username) => { RecheckState(); },
        OnCompleted = (when, track) => { RecheckState(); },
        OnScratched = (when, track, username) => { RecheckState(); },
        OnFailed = (when, track, error) => { RecheckState(); },
      };
      for (; ; )
      {
        RecheckState();
        try {
          LogConnection.Log(lc);
        }
        catch {
          // nom
        }
      }
    }

    private void RecheckState()
    {
      string newTrack = null;
      try {
        QueueEntry playing = new QueueEntry();
        if (Connection.Playing(playing) == 252) {
          newTrack = playing.Track;
        }
      }
      catch(Exception) {
        // nom
      }
      if (newTrack == null) {
        Dispatcher.Invoke(() =>
        {
          CurrentTrack = null;
          ArtistLabel.Content = "";
          AlbumLabel.Content = "";
          TitleLabel.Content = "";
        });
      } else {
        Dispatcher.Invoke(() =>
        {
          if (CurrentTrack == null || CurrentTrack != newTrack) {
            CurrentTrack = newTrack;
            ArtistLabel.Content = "";
            AlbumLabel.Content = "";
            TitleLabel.Content = "";
            Connection.PartAsync(newTrack, "display", "artist", (rc, part) =>
            {
              Dispatcher.Invoke(() =>
              {
                if (CurrentTrack == newTrack) {
                  ArtistLabel.Content = part;
                }
              });
            });
            Connection.PartAsync(newTrack, "display", "album", (rc, part) =>
            {
              Dispatcher.Invoke(() =>
              {
                if (CurrentTrack == newTrack) {
                  AlbumLabel.Content = part;
                }
              });
            });
            Connection.PartAsync(newTrack, "display", "title", (rc, part) =>
            {
              Dispatcher.Invoke(() =>
              {
                if (CurrentTrack == newTrack) {
                  TitleLabel.Content = part;
                }
              });
            });
          }
        });
      }
    }
    #endregion

    #region Network Play
    // Return true if A is a 'better' address than B
    static bool BetterAddress(UnicastIPAddressInformation a,
                              UnicastIPAddressInformation b)
    {
      // Exists is better than doesn't
      if (a == null)
        return false;
      if (b == null)
        return true;
      // IPv4 is better than IPv6 (at least for now)
      if (a.Address.AddressFamily != b.Address.AddressFamily)
        return a.Address.AddressFamily == AddressFamily.InterNetwork;
      // Persistent is better than transient
      if (a.IsTransient != b.IsTransient)
        return b.IsTransient;
      // Longer lifetimes are better than short
      if (a.AddressPreferredLifetime != b.AddressPreferredLifetime)
        return a.AddressPreferredLifetime > b.AddressPreferredLifetime;
      if (a.AddressValidLifetime != b.AddressValidLifetime)
        return a.AddressValidLifetime > b.AddressValidLifetime;
      if (a.DhcpLeaseLifetime != b.DhcpLeaseLifetime)
        return a.DhcpLeaseLifetime > b.DhcpLeaseLifetime;
      // Unicast is better than link-local
      if (a.Address.IsIPv6LinkLocal != b.Address.IsIPv6LinkLocal)
        return b.Address.IsIPv6LinkLocal;
      if (a.Address.IsIPv6Teredo != b.Address.IsIPv6Teredo)
        return b.Address.IsIPv6Teredo;
      return false;
    }

    // Find the most suitable local address
    static IPAddress GetLocalAddress()
    {
      UnicastIPAddressInformation addr = null;
      foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces()) {
        // Reject unsuitable interfaces
        if (ni.OperationalStatus != OperationalStatus.Up)
          continue;
        if (!ni.Supports(NetworkInterfaceComponent.IPv4)
           && !ni.Supports(NetworkInterfaceComponent.IPv6))
          continue;
        if (ni.NetworkInterfaceType == NetworkInterfaceType.Loopback)
          continue;
        IPInterfaceProperties ip = ni.GetIPProperties();
        foreach (UnicastIPAddressInformation candidate in ip.UnicastAddresses) {
          // Reject completely unsuitable addresses
          if (candidate.Address.AddressFamily != AddressFamily.InterNetwork
             && candidate.Address.AddressFamily != AddressFamily.InterNetworkV6)
            continue;
          if (candidate.Address.IsIPv6Multicast
             || candidate.Address.IsIPv4MappedToIPv6)
            continue;
          if (BetterAddress(candidate, addr)) {
            /*if(addr != null)
              Console.WriteLine("{0} > {1}", candidate.Address, addr.Address);*/
            addr = candidate;
          }
          else {
            /*if(addr != null)
              Console.WriteLine("{0} < {1}", candidate.Address, addr.Address);*/
          }
        }
      }
      return addr.Address;
    }

    private Socket NetworkPlaySocket = null;
    private byte[] NetworkPlayBuffer = new byte[4096 * 4];
    private uint NetworkPlayOffset;
    private bool NetworkPlayOffsetSet;
    private void StartNetworkPlay()
    {
      if (NetworkPlaySocket != null)
        return;
      NetworkPlaySocket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
      // Ramp up the input buffer size
      NetworkPlaySocket.ReceiveBufferSize = 44100 * 4;
      // Bind to some local address.  By default we use the same port as the server
      // but rtp_local can override this.
      IPAddress localAddress = GetLocalAddress();
      IPEndPoint localEndpoint = new IPEndPoint(localAddress, 0);
      if (Configuration.RtpLocal != 0)
        localEndpoint.Port = Configuration.RtpLocal;
      else
        localEndpoint.Port = Configuration.Port;
      NetworkPlaySocket.Bind(localEndpoint);
      // Await incoming traffic
      NetworkPlayOffsetSet = false;
      NetworkPlayWait();
      // Enable inbound data
      IPEndPoint endpoint = (IPEndPoint)NetworkPlaySocket.LocalEndPoint;
      string address = endpoint.Address.ToString();
      string port = string.Format("{0}", endpoint.Port);
      Connection.RtpRequestAsync(address, port, null, (error) => { Dispatcher.Invoke(() => { StopNetworkPlay(); }); });
    }

    void NetworkPlayWait()
    {
      NetworkPlaySocket.BeginReceive(NetworkPlayBuffer, 0, NetworkPlayBuffer.Length, SocketFlags.None, NetworkPlayReceive, null);
    }

    unsafe void NetworkPlayReceive(IAsyncResult ar) {
      try {
        int n = NetworkPlaySocket.EndReceive(ar);
        if (n > 0) {
          uint timestamp = ((uint)NetworkPlayBuffer[4] << 24)
            + ((uint)NetworkPlayBuffer[5] << 16)
            + ((uint)NetworkPlayBuffer[6] << 8)
            + (uint)NetworkPlayBuffer[7];
          // Initial timestamp should be 0
          bool first = false;
          if (!NetworkPlayOffsetSet) {
            NetworkPlayOffset = timestamp;
            NetworkPlayOffsetSet = true;
            first = true;
          }
          timestamp -= NetworkPlayOffset;
          if (BitConverter.IsLittleEndian) {
            // Byte order is wrong
            for (int i = 12; i < n; i += 2) {
              byte t = NetworkPlayBuffer[i];
              NetworkPlayBuffer[i] = NetworkPlayBuffer[i + 1];
              NetworkPlayBuffer[i + 1] = t;
            }
          }
          fixed (byte* p = NetworkPlayBuffer) {
            dxbridge.dxbridgeBuffer(2 * timestamp, (IntPtr)p + 12, (IntPtr)n - 12);
          }
          if (first)
            dxbridge.dxbridgePlay();
        }
        NetworkPlayWait();
      }
      catch {
        // nom
      }
    }

    private void StopNetworkPlay()
    {
      if (NetworkPlaySocket == null)
        return;
      Connection.RtpCancelAsync();
      NetworkPlaySocket.Close(0);
      NetworkPlaySocket = null;
      dxbridge.dxbridgeStop();
    }

    #endregion
  }
}
