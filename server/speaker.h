/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
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
/** @file server/speaker.h
 * @brief Speaker process
 */
#ifndef SPEAKER_H
#define SPEAKER_H

#ifdef WORDS_BIGENDIAN
# define MACHINE_AO_FMT AO_FMT_BIG
#else
# define MACHINE_AO_FMT AO_FMT_LITTLE
#endif

/** @brief How many seconds of input to buffer
 *
 * While any given connection has this much audio buffered, no more reads will
 * be issued for that connection.  The decoder will have to wait.
 */
#define BUFFER_SECONDS 5

/** @brief Frame batch size
 *
 * This controls how many frames are written in one go.
 *
 * For ALSA we request a buffer of three times this size and set the low
 * watermark to this amount.  The goal is then to keep between 1 and 3 times
 * this many frames in play.
 *
 * For all backends we attempt to play up to three times this many frames per
 * shot.  In practice we will often only send much less than this.
 */
#define FRAMES 4096

/** @brief Bytes to send per network packet
 *
 * Don't make this too big or arithmetic will start to overflow.
 */
#define NETWORK_BYTES (1024+sizeof(struct rtp_header))

/** @brief Maximum RTP playahead (ms) */
#define RTP_AHEAD_MS 1000

/** @brief Maximum number of FDs to poll for */
#define NFDS 256

/** @brief Track structure
 *
 * Known tracks are kept in a linked list.  Usually there will be at most two
 * of these but rearranging the queue can cause there to be more.
 */
struct track {
  struct track *next;                   /* next track */
  int fd;                               /* input FD */
  char id[24];                          /* ID */
  size_t start, used;                   /* start + bytes used */
  int eof;                              /* input is at EOF */
  int got_format;                       /* got format yet? */
  ao_sample_format format;              /* sample format */
  unsigned long long played;            /* number of frames played */
  char *buffer;                         /* sample buffer */
  size_t size;                          /* sample buffer size */
  int slot;                             /* poll array slot */
};

/** @brief Structure of a backend */
struct speaker_backend {
  /** @brief Which backend this is
   *
   * @c -1 terminates the list.
   */
  int backend;

  /** @brief Flags
   *
   * Possible values
   * - @ref FIXED_FORMAT
   */
  unsigned flags;
/** @brief Lock to configured sample format */
#define FIXED_FORMAT 0x0001
  
  /** @brief Initialization
   *
   * Called once at startup.  This is responsible for one-time setup
   * operations, for instance opening a network socket to transmit to.
   *
   * When writing to a native sound API this might @b not imply opening the
   * native sound device - that might be done by @c activate below.
   */
  void (*init)(void);

  /** @brief Activation
   * @return 0 on success, non-0 on error
   *
   * Called to activate the output device.
   *
   * After this function succeeds, @ref ready should be non-0.  As well as
   * opening the audio device, this function is responsible for reconfiguring
   * if it necessary to cope with different samples formats (for backends that
   * don't demand a single fixed sample format for the lifetime of the server).
   */
  int (*activate)(void);

  /** @brief Play sound
   * @param frames Number of frames to play
   * @return Number of frames actually played
   */
  size_t (*play)(size_t frames);
  
  /** @brief Deactivation
   *
   * Called to deactivate the sound device.  This is the inverse of
   * @c activate above.
   */
  void (*deactivate)(void);

  /** @brief Called before poll()
   *
   * Called before the call to poll().  Should call addfd() to update the FD
   * array and stash the slot number somewhere safe.
   */
  void (*beforepoll)(void);

  /** @brief Called after poll()
   * @return 0 if we could play, non-0 if not
   *
   * Called after the call to poll().  Should arrange to play some audio if the
   * output device is ready.
   *
   * The return value should be 0 if the device was ready to play, or nonzero
   * if it was not.
   */
  int (*afterpoll)(void);
};

/** @brief Linked list of all prepared tracks */
extern struct track *tracks;

/** @brief Playing track, or NULL */
extern struct track *playing;

#endif /* SPEAKER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
