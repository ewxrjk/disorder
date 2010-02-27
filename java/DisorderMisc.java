/*
 * This file is part of DisOrder.
 * Copyright (C) 2010 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
package uk.org.greenend.disorder;

import java.util.*;

/**
 * Miscellaneous utility code
 *
 */
class DisorderMisc {
  /**
   * Test for a whitespace character
   *
   * Returns true for space, tab, newline and carriage return.
   */
  private static boolean space(char c) {
    return (c == ' '
	    || c == '\t'
	    || c == '\n'
	    || c == '\r');
  }

  /**
   * Split up a string according to DisOrder's usual splitting rules
   *
   * @param s String to split
   * @param allowComments true if comments should be detected, otherwise false
   * @return The string split into fields
   */
  static Vector<String> split(String s,
                              boolean allowComments) throws DisorderParseException {
    Vector<String> v = new Vector<String>();
    int n = 0;
    while(n < s.length()) {
      char c = s.charAt(n++);
      if(space(c)) {
	continue;
      }
      if(allowComments && c == '#')
	break;
      if(c == '"' || c == '\'') {
	// Quoted string; terminated by quote character, some
	// escapes allowed.
	StringBuilder e = new StringBuilder();
	final char quoteChar = c;
	while(n < s.length() && s.charAt(n) != quoteChar) {
	  c = s.charAt(n++);
	  if(c == '\\') {
	    if(n >= s.length())
	      break;
	    c = s.charAt(n++);
	    switch(c) {
	    case '\\':
	    case '"':
	    case '\'':
	      e.append(c);
	    break;
	    case 'n':
	      e.append('\n');
	      break;
	    default:
	      throw new DisorderParseException("invalid escape sequence");
	    }
	  } else {
	    e.append(c);
	  }
	}
	if(n == s.length())
	  throw new DisorderParseException("unterminated quoted string");
        ++n;
	v.add(e.toString());
      } else {
	// Unquoted string; terminated by space or end of
	// string, no escapes.
        --n;
	int m;
	for(m = n; m < s.length() && !space(s.charAt(m)); ++m) {
        }
	v.add(s.substring(n, m));
	n = m;
      }
    }
    return v;
  }

  /**
   * Convert a string from hex to a byteblock.
   *
   * @param s String to convert
   * @return Byteblock
   * @throw DisorderParseException If the input string is malformed
   */
  static byte[] fromHex(String s) throws DisorderParseException {
    if(s.length() % 2 == 1)
      throw new DisorderParseException("hex string has odd length");
    if(!s.matches("^[0-9a-fA-F]*$"))
      throw new DisorderParseException("invalid hex string");
    final int len = s.length() / 2;
    byte[] bytes = new byte[len];
    for(int n = 0; n < len; ++n) {
      String ss = s.substring(n * 2, n * 2 + 2);
      bytes[n] = (byte)Integer.parseInt(ss, 16);
    }
    return bytes;
  }

  /**
   * Convert a byte block to hex.
   *
   * @param b Bytes to convert
   * @return String
   */
  static String toHex(byte b[]) {
    final char[] digits = { '0','1','2','3','4','5','6','7',
                            '8','9','a','b','c','d','e','f'};
    StringBuilder s = new StringBuilder(b.length * 2);
    for(int n = 0; n < b.length; ++n) {
      int i = b[n] & 0xFF;
      s.append(digits[i >> 4]);
      s.append(digits[i & 15]);
    }
    return s.toString();
  }
}

/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
