/*
 * This file is part of DisOrder.
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file lib/uaudio-schedule.c
 * @brief Scheduler for RTP and command backends
 *
 * These functions ensure that audio is only written at approximately the rate
 * it should play at, allowing pause to function properly.
 *
 * OSS and ALSA we expect to be essentially synchronous (though we could use
 * this code if they don't play nicely).  Core Audio sorts out its own timing
 * issues itself.
 *
 * The sequence numbers are intended for RTP's use but it's more convenient to
 * maintain them here.
 */

#include "common.h"

#include <unistd.h>
#include <gcrypt.h>

#include "uaudio.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"
#include "timeval.h"

/** @brief Sample timestamp
 *
 * This is the timestamp that will be used on the next outbound packet.
 *
 * The timestamp in an RTP packet header is only 32 bits wide.  With 44100Hz
 * stereo, that only gives about half a day before wrapping, which is not
 * particularly convenient for certain debugging purposes.  Therefore the
 * timestamp is maintained as a 64-bit integer, giving around six million years
 * before wrapping, and truncated to 32 bits when transmitting.
 */
uint64_t uaudio_schedule_timestamp;

/** @brief Actual time corresponding to @ref uaudio_schedule_timestamp
 *
 * This is the time, on this machine, at which the sample at @ref
 * uaudio_schedule_timestamp ought to be sent, interpreted as the time the last
 * packet was sent plus the time length of the packet. */
static struct timeval uaudio_schedule_timeval;

/** @brief Set when we (re-)activate, to provoke timestamp resync */
int uaudio_schedule_reactivated;

/** @brief Delay threshold in microseconds
 *
 * uaudio_schedule_play() never attempts to introduce a delay shorter than this.
 */
static int64_t uaudio_schedule_delay_threshold;

/** @brief Time for current packet */
static struct timeval uaudio_schedule_now;

/** @brief Synchronize playback operations against real time
 *
 * This function sleeps as necessary to rate-limit playback operations to match
 * the actual playback rate.  It also maintains @ref uaudio_schedule_timestamp
 * as an arbitrarily-based sample counter, for use by RTP.
 *
 * You should call this in your API's @ref uaudio_playcallback before writing
 * and call uaudio_schedule_update() afterwards.
 */
void uaudio_schedule_synchronize(void) {
retry:
  xgettimeofday(&uaudio_schedule_now, NULL);
  if(uaudio_schedule_reactivated) {
    /* We've been deactivated for some unknown interval.  We need to advance
     * rtp_timestamp to account for the dead air. */
    /* On the first run through we'll set the start time. */
    if(!uaudio_schedule_timeval.tv_sec)
      uaudio_schedule_timeval = uaudio_schedule_now;
    /* See how much time we missed.
     *
     * This will be 0 on the first run through, in which case we'll not modify
     * anything.
     *
     * It'll be negative in the (rare) situation where the deactivation
     * interval is shorter than the last packet we sent.  In this case we wait
     * for that much time and then return having sent no samples, which will
     * cause uaudio_play_thread_fn() to retry.
     *
     * In the normal case it will be positive.
     */
    const int64_t delay = tvsub_us(uaudio_schedule_now,
                                   uaudio_schedule_timeval); /* microseconds */
    if(delay < 0) {
      usleep(-delay);
      goto retry;
    }
    /* Advance the RTP timestamp to the present.  With 44.1KHz stereo this will
     * overflow the intermediate value with a delay of a bit over 6 years.
     * This seems acceptable. */
    uint64_t update = (delay * uaudio_rate * uaudio_channels) / 1000000;
    /* Don't throw off channel synchronization */
    update -= update % uaudio_channels;
    /* We log nontrivial changes */
    if(update)
      info("advancing uaudio_schedule_timeval by %"PRIu64" samples", update);
    uaudio_schedule_timestamp += update;
    uaudio_schedule_timeval = uaudio_schedule_now;
    uaudio_schedule_reactivated = 0;
  } else {
    /* Chances are we've been called right on the heels of the previous packet.
     * If we just sent packets as fast as we got audio data we'd get way ahead
     * of the player and some buffer somewhere would fill (or at least become
     * unreasonably large).
     *
     * First find out how far ahead of the target time we are.
     */
    const int64_t ahead = tvsub_us(uaudio_schedule_timeval,
                                   uaudio_schedule_now); /* microseconds */
    /* Only delay at all if we are nontrivially ahead. */
    if(ahead > uaudio_schedule_delay_threshold) {
      /* Don't delay by the full amount */
      usleep(ahead - uaudio_schedule_delay_threshold / 2);
      /* Refetch time (so we don't get out of step with reality) */
      xgettimeofday(&uaudio_schedule_now, NULL);
    }
  }
}

/** @brief Update schedule after writing
 *
 * Called by your API's @ref uaudio_playcallback after sending audio data (to a
 * subprocess or network or whatever).  A separate function so that the caller
 * doesn't have to know how many samples they're going to write until they've
 * done so.
 */
void uaudio_schedule_update(size_t written_samples) {
  /* uaudio_schedule_timestamp and uaudio_schedule_timestamp are supposed to
   * refer to the first sample of the next packet */
  uaudio_schedule_timestamp += written_samples;
  const unsigned usec = (uaudio_schedule_timeval.tv_usec
                         + 1000000 * written_samples / (uaudio_rate
                                                            * uaudio_channels));
  /* ...will only overflow 32 bits if one packet is more than about half an
   * hour long, which is not plausible. */
  uaudio_schedule_timeval.tv_sec += usec / 1000000;
  uaudio_schedule_timeval.tv_usec = usec % 1000000;
}

/** @brief Initialize audio scheduling
 *
 * Should be called from your API's @c start callback.
 */
void uaudio_schedule_init(void) {
  gcry_create_nonce(&uaudio_schedule_timestamp,
                    sizeof uaudio_schedule_timestamp);
  /* uaudio_schedule_play() will spot this and choose an initial value */
  uaudio_schedule_timeval.tv_sec = 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
