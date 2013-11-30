using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace uk.org.greenend.DisOrder
{
  internal class Utils
  {

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
          if(pos >= s.Length)
            throw new Exception("unterminated quoted string");
          ++pos;
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

    #region Time and Date
    private static DateTime zerodate = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

    internal static long DateTimeToUnix(DateTime dt)
    {
      return (long)(dt - zerodate).TotalSeconds;
    }

    internal static DateTime UnixToDateTime(double ut)
    {
      DateTime dt = zerodate;
      dt.AddSeconds(ut);
      return dt;
    }
    #endregion

    #region Hexadecimal Conversion
    static internal IList<byte> FromHex(string s)
    {
      List<byte> bytes = new List<byte>();
      for (int i = 0; i < s.Length; i += 2) {
        bytes.Add((byte)int.Parse(s.Substring(i, 2), System.Globalization.NumberStyles.HexNumber));
      }
      return bytes;
    }

    static internal string ToHex(byte[] bytes)
    {
      StringBuilder sb = new StringBuilder();
      foreach (byte b in bytes) {
        sb.Append(string.Format("{0:x2}", b));
      }
      return sb.ToString();
    }
    #endregion

  }
}
