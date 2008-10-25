/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/rtp.h
 * @brief RTP packet header format
 *
 * See <a href="http://www.ietf.org/rfc/rfc1889.txt">RFC1889</a>.
 */

#ifndef RTP_H
#define RTP_H

/** @brief RTP packet header format
 *
 * See <a href="http://www.ietf.org/rfc/rfc1889.txt">RFC1889</a> (now obsoleted
 * by <a href="http://www.ietf.org/rfc/rfc3550.txt">RFC3550</a>).
 */
struct attribute((packed)) rtp_header {
  /** @brief Version, padding, extension and CSRC
   *
   * Version is bits 6 and 7; currently always 2.
   *
   * Padding is bit 5; if set frame includes padding octets.
   *
   * eXtension is bit 4; if set there is a header extension.
   *
   * CSRC Count is bits 0-3 and is the number of CSRC identifiers following the
   * header.
   */
  uint8_t vpxcc;

  /** @brief Marker and payload type
   *
   * Marker is bit 7.  Profile-defined.
   *
   * Payload Type is bits 0-6.  Profile defined.  
   */
  uint8_t mpt;

  /** @brief Sequence number */
  uint16_t seq;

  /** @brief Timestamp */
  uint32_t timestamp;

  /** @brief Synchronization source */
  uint32_t ssrc;
};

#endif /* RTP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
