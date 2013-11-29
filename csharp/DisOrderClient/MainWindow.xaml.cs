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

namespace DisOrderClient
{
  /// <summary>
  /// Interaction logic for MainWindow.xaml
  /// </summary>
  public partial class MainWindow : Window
  {
    private Configuration Configuration;
    private Connection Connection;

    public MainWindow()
    {
      InitializeComponent();
      Configuration = new Configuration();
      Configuration.Read();
      Connection = new Connection() { Configuration = Configuration };
    }

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

    }

    public static RoutedCommand DisconnectCommand = new RoutedCommand();
    private void DisconnectExecuted(object sender, ExecutedRoutedEventArgs e)
    {

    }

    public static RoutedCommand OptionsCommand = new RoutedCommand();
    private void OptionsExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      Options options = new Options()
      {
        Owner = this,
        Configuration = Configuration
      };
      options.ShowDialog();
    }

    private void CloseExecuted(object sender, ExecutedRoutedEventArgs e)
    {
      this.Close();
    }
  }
}
