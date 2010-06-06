/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/speaker-protocol.h
 * @brief Speaker/server protocol support
 *
 * This file defines the protocol by which the main server and the speaker
 * process communicate.
 */

#ifndef SPEAKER_PROTOCOL_H
#define SPEAKER_PROTOCOL_H

#include "byte-order.h"

/** @brief A message from the main server to the speaker, or vica versa */
struct speaker_message {
  /** @brief Message type
   *
   * Messges from the main server:
   * - @ref SM_PLAY
   * - @ref SM_PAUSE
   * - @ref SM_RESUME
   * - @ref SM_CANCEL
   * - @ref SM_RELOAD
   *
   * Messages from the speaker:
   * - @ref SM_PAUSED
   * - @ref SM_FINISHED
   * - @ref SM_PLAYING
   * - @ref SM_UNKNOWN
   * - @ref SM_ARRIVED
   */
  int type;

  /** @brief Message-specific data */
  long data;

  /** @brief Track ID (including 0 terminator) */
  char id[24];                          /* ID including terminator */
};

/* messages from the main DisOrder server */

/** @brief Play track @c id
 *
 * The track must already have been prepared.
 */
#define SM_PLAY 1

/** @brief Pause current track */
#define SM_PAUSE 2

/** @brief Resume current track */
#define SM_RESUME 3

/** @brief Cancel track @c id */
#define SM_CANCEL 4

/** @brief Reload configuration */
#define SM_RELOAD 5

/* messages from the speaker */
/** @brief Paused track @c id, @c data seconds in
 *
 * There is no @c SM_RESUMED, instead @ref SM_PLAYING is sent after the track
 * starts playing again.
 */
#define SM_PAUSED 128

/** @brief Finished playing track @c id */
#define SM_FINISHED 129

/** @brief Never heard of track @c id */
#define SM_UNKNOWN 130

/** @brief Currently track @c id, @c data seconds in
 *
 * This is sent from time to time while a track is playing.
 */
#define SM_PLAYING 131

/** @brief Speaker process is ready
 *
 * This is sent once at startup when the speaker has finished its
 * initialization. */
#define SM_READY 132

/** @brief Cancelled track @c id which wasn't playing */
#define SM_STILLBORN 133

/** @brief A connection for track @c id arrived */
#define SM_ARRIVED 134

void speaker_send(int fd, const struct speaker_message *sm);
/* Send a message. */

int speaker_recv(int fd, struct speaker_message *sm);
/* Receive a message.  Return 0 on EOF, +ve if a message is read, -1 on EAGAIN,
 * terminates on any other error. */

/** @brief One chunk in a stream */
struct stream_header {
  /** @brief Number of bytes */
  uint32_t nbytes;

  /** @brief Frames per second */
  uint32_t rate;

  /** @brief Samples per frames */
  uint8_t channels;

  /** @brief Bits per sample */
  uint8_t bits;

  /** @brief Endianness */
  uint8_t endian;
} attribute((packed));

static inline int formats_equal(const struct stream_header *a,
                                const struct stream_header *b) {
  return (a->rate == b->rate
          && a->channels == b->channels
          && a->bits == b->bits
          && a->endian == b->endian);
}

#endif /* SPEAKER_PROTOCOL_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
