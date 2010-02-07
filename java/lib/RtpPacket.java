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
 * An RTP packet.
 *
 * This only supports as much as Disorder needs.
 */
public class RtpPacket implements Comparable<RtpPacket> {
  /**
   * Version number.
   */
  public int version;

  /**
   * Padding bit.
   */
  public boolean padding;

  /**
   * Marker bit.
   */
  public boolean marker;

  /**
   * Payload type.
   */
  public int payload;

  /**
   * Sequence number.
   */
  public int seq;

  /**
   * Timestamp.
   * Despite the name this is actually counts samples.
   */
  public int timestamp;

  /**
   * Synchronization source.
   */
  public int ssrc;

  /**
   * Offset of first sample within <code>data</code>.
   */
  public int offset;

  /**
   * Total number of bytes in <code>data</code>.
   */
  public int length;

  /**
   * Raw packet data.
   */
  public byte[] data;

  /**
   * Construct an RTP packet from a received datagram.
   * Keeps a reference to <code>packetData</code>.
   *
   * @param packetData RTP packet
   */
  public RtpPacket(byte[] packetData, int packetLength) {
    version = (packetData[0] >> 6) & 0x03;
    padding = ((packetData[0] & 0x20) != 0) ? true : false;
    // TODO extensions
    // TODO CSRCs
    marker = ((packetData[1] & 0x80) != 0) ? true : false;
    payload = packetData[1] & 0x7F;
    seq = ((packetData[2] & 0xFF) << 8)
      + (packetData[3] & 0xFF);
    timestamp = ((packetData[4] & 0xFF) << 24)
      + ((packetData[5] & 0xFF) << 16)
      + ((packetData[6] & 0xFF) << 8)
      + (packetData[7] & 0xFF);
    ssrc = ((packetData[8] & 0xFF) << 24)
      + ((packetData[9] & 0xFF) << 16)
      + ((packetData[10] & 0xFF) << 8)
      + (packetData[11] & 0xFF);
    offset = 12;
    data = packetData;
    length = packetLength;
  }

  /**
   * Comparison method.
   *
   * RTP packets are compared by their timestamp.
   *
   * @param that RTP packet to compare to
   * @return -1, 0 or 1 for <code>this</code> less than, equal to or greater than <code>that</code>
   */
  public int compareTo(RtpPacket that) {
    return compareTimestamp(seq, that.seq);
  }

  /**
   * Comparison method for RTP timestamps.
   *
   * @param a Timestamp
   * @param b Timestamp
   * @return -1, 0 or 1 for <code>a</code> less than, equal to or greater than <code>b</code>
   */
  public static int compareTimestamp(int a, int b) {
    if(a == b)
      return 0;
    if((a - b) < 0)
      return -1;
    else
      return 1;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
