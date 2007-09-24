/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
/** @file lib/speaker-protocol.h
 * @brief Speaker/server protocol support
 *
 * This file defines the protocol by which the main server and the speaker
 * process communicate.
 */

#ifndef SPEAKER_PROTOCOL_H
#define SPEAKER_PROTOCOL_H

/** @brief A message from the main server to the speaker, or vica versa */
struct speaker_message {
  /** @brief Message type
   *
   * Messges from the main server:
   * - @ref SM_PREPARE
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
   */
  int type;

  /** @brief Message-specific data */
  long data;

  /** @brief Track ID (including 0 terminator) */
  char id[24];                          /* ID including terminator */
};

/* messages from the main DisOrder server */
/** @brief Prepare track @c id
 *
 * This message will include a file descriptor.  The speaker starts buffering
 * audio data read from this file against the time that it must be played.
 */
#define SM_PREPARE 0

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

/** @brief Currently track @c id, @c data seconds in
 *
 * This is sent from time to time while a track is playing.
 */
#define SM_PLAYING 131

void speaker_send(int fd, const struct speaker_message *sm, int datafd);
/* Send a message.  DATAFD is passed too if not -1.  Does not close DATAFD. */

int speaker_recv(int fd, struct speaker_message *sm, int *datafd);
/* Receive a message.  If DATAFD is not null then can receive an FD.  Return 0
 * on EOF, +ve if a message is read, -1 on EAGAIN, terminates on any other
 * error. */

#endif /* SPEAKER_PROTOCOL_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
