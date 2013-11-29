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
using System.Windows.Shapes;
using System.Threading;
using uk.org.greenend.DisOrder;

namespace DisOrderClient
{
  /// <summary>
  /// Interaction logic for About.xaml
  /// </summary>
  public partial class About : Window
  {
    public Connection Connection { get; set; }

    public About()
    {
      InitializeComponent();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
      ThreadPool.QueueUserWorkItem((_) =>
      {
        string v;
        try {
          Connection.Version(out v);
          Dispatcher.Invoke(() =>
          {
            ServerVersion.Text = string.Format("Server Version {0}", v);
          });
        }
        catch {
          Dispatcher.Invoke(() =>
          {
            ServerVersion.Text = "(cannot retrieve server version)";
          });
        }
      });
    }

    private void VisitHomePage(object sender, System.Windows.Navigation.RequestNavigateEventArgs e)
    {
      System.Diagnostics.Process.Start(link.NavigateUri.ToString());
      e.Handled = true;
    }

    private void OK(object sender, RoutedEventArgs e)
    {
      this.Close();
    }
  }
}
