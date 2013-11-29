using System.Windows;
using System.Windows.Controls;
using uk.org.greenend.DisOrder;

namespace DisOrderClient
{
  /// <summary>
  /// Interaction logic for Options.xaml
  /// </summary>
  public partial class Options : Window
  {
    public Options()
    {
      InitializeComponent();
    }

    #region Properties Set By Caller
    private Configuration _Configuration;
    public Configuration Configuration
    {
      get
      {
        return _Configuration;
      }
      set
      {
        _Configuration = value;
        HostnameEntry.Text = _Configuration.Address;
        PortEntry.Text = string.Format("{0}", _Configuration.Port);
        PasswordEntry.Password = _Configuration.Password;
        SomethingChanged(null, null);
      }
    }
    #endregion

    #region Completion
    private void Cancel(object sender, RoutedEventArgs e)
    {
      this.Close();
    }

    private void OK(object sender, RoutedEventArgs e)
    {
      if (SettingsChanged()) {
        _Configuration.Address = HostnameEntry.Text;
        _Configuration.Port = int.Parse(PortEntry.Text);
        _Configuration.Password = PasswordEntry.Password;
        _Configuration.Write();
        // TODO perhaps should reconnect to server
      }
      this.Close();
    }
    #endregion

    #region Editing Events
    private void SomethingChanged(object sender, TextChangedEventArgs e)
    {
      OKButton.IsEnabled = SettingsValid();
    }

    private bool SettingsValid()
    {
      int port;
      return int.TryParse(PortEntry.Text, out port) && port > 0 && port <= 65535 && HostnameEntry.Text.Length > 0;
    }

    private bool SettingsChanged()
    {
      if (!SettingsValid())
        return false;
      return (_Configuration.Address != HostnameEntry.Text
              || _Configuration.Port != int.Parse(PortEntry.Text)
              || _Configuration.Password != PasswordEntry.Password);
    }
    #endregion
  }
}
