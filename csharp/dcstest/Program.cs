using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using uk.org.greenend.DisOrder;

namespace dcstest
{
  class Program
  {
    static void Main(string[] args)
    {
      Connection c = new Connection();
      string v;
      c.Version(out v);
      Console.WriteLine("version: {0}", v);
    }
  }
}
