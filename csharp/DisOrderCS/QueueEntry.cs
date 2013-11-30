using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace uk.org.greenend.DisOrder
{
  public class QueueEntry
  {
    #region Public Properties
    public DateTime? Expected { get; set; }
    public string Id { get; set; }
    public DateTime? Played { get; set; }
    public string Scratched { get; set; }
    public string Origin { get; set; }
    public string State { get; set; }
    public string Submitter { get; set; }
    public string Track { get; set; }
    public DateTime? When { get; set; }
    public int? Wstat { get; set; }
    public IDictionary<string, string> Other { get; set; }
    #endregion

    public QueueEntry()
    {
      Clear();
    }

    public QueueEntry(string s)
    {
      Set(s);
    }

    public QueueEntry(IList<string> s, int offset = 0)
    {
      Set(s, offset);
    }

    public void Clear()
    {
      Expected = null;
      Id = null;
      Played = null;
      Scratched = null;
      Origin = null;
      State = null;
      Submitter = null;
      Track = null;
      When = null;
      Wstat = null;
      Other = new Dictionary<string, string>();
    }

    public void Set(string s)
    {
      Set(Utils.Split(s, false));
    }

    public void Set(IList<string> bits, int offset = 0)
    {
      if ((bits.Count - offset) % 2 != 0)
        throw new Exception("invalid track information"); // TODO exception type
      Clear();
      for (int i = offset; i < bits.Count(); i += 2) {
        SetBit(bits[i], bits[i + 1]);
      }
    }

    private void SetBit(string name, string value)
    {
      if (name == "expected") Expected = Utils.UnixToDateTime(double.Parse(value));
      else if (name == "id") Id = value;
      else if (name == "played") Played = Utils.UnixToDateTime(double.Parse(value));
      else if (name == "scratched") Scratched = value;
      else if (name == "origin") Origin = value;
      else if (name == "state") State = value;
      else if (name == "submitter") Submitter = value;
      else if (name == "track") Track = value;
      else if (name == "when") When = Utils.UnixToDateTime(double.Parse(value));
      else if (name == "wstat") Wstat = int.Parse(value);
      else Other[name] = value;
    }
  }
}
