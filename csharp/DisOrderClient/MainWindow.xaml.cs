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

namespace DisOrderClient
{
  /// <summary>
  /// Interaction logic for MainWindow.xaml
  /// </summary>
  public partial class MainWindow : Window
  {
    private Configuration Configuration;
    private Connection Connection;
    private Connection LogConnection;

    public MainWindow()
    {
      InitializeComponent();
      Configuration = new Configuration();
      Configuration.Read();
      Connection = new Connection() { Configuration = Configuration };
      LogConnection = new Connection() { Configuration = Configuration };
      ThreadPool.QueueUserWorkItem((_) => { MonitorLog(); });
    }

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
    #endregion

    #region Server Monitoring
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
      try {
        QueueEntry playing = new QueueEntry();
        if (Connection.Playing(playing) == 252) {
          Dispatcher.Invoke(() => { PlayingLabel.Content = playing.Track; });
        }
        else {
          // nothing playing
          Dispatcher.Invoke(() => { PlayingLabel.Content = "(nothing)"; });
        }
      }
      catch(Exception e) {
        Dispatcher.Invoke(() => { PlayingLabel.Content = string.Format("(error: {0}", e.Message); });
        // nom
      }
    }
    #endregion
  }
}
