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
 *
 * The basic idea:
 * - we maintain a base time
 * - we calculate from this how many samples SHOULD have been sent by now
 * - we compare this with the number of samples sent so far
 * - we use this to wait until we're ready to send something
 * - it's up to the caller to send nothing, or send 0s, if it's supposed to
 *   be paused
 *
 * An implication of this is that the caller must still call
 * uaudio_schedule_sync() when deactivated (paused) and pretend to send 0s.
 */

#include "common.h"

#include <unistd.h>
#include <time.h>
#include <errno.h>

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
static uint64_t timestamp;

/** @brief Base time
 *
 * This is the base time that corresponds to a timestamp of 0.
 */
struct timeval base;

/** @brief Synchronize playback operations against real time
 * @return Sample number
 *
 */
uint32_t uaudio_schedule_sync(void) {
  const unsigned rate = uaudio_rate * uaudio_channels;
  struct timeval now;

  xgettimeofday(&now, NULL);
  /* If we're just starting then we might as well send as much as possible
   * straight away. */
  if(!base.tv_sec) {
    base = now;
    return timestamp;
  }
  /* Calculate how many microseconds ahead of the base time we are */
  uint64_t us = tvsub_us(now, base);
  /* Calculate how many samples that is */
  uint64_t samples = us * rate / 1000000;
  /* So...
   *
   * We've actually sent 'timestamp' samples so far.
   *
   * We OUGHT to have sent 'samples' samples so far.
   *
   * Suppose it's the SECOND call.  timestamp will be (say) 716.  'samples'
   * will be (say) 10 - there's been a bit of scheduling delay.  So in that
   * case we should wait for 716-10=706 samples worth of time before we can
   * even send one sample.
   *
   * So we wait that long and send our 716 samples.
   *
   * On the next call we'll have timestamp=1432 and samples=726, say.  So we
   * wait and send again.
   *
   * On the next call there's been a bit of a delay.  timestamp=2148 but
   * samples=2200.  So we send our 716 samples immediately.
   *
   * If the delay had been longer we might sent further packets back to back to
   * make up for it.
   *
   * Now timestamp=2864 and samples=2210 (say).  Now we're back to waiting.
   */
  if(samples < timestamp) {
    /* We should delay a bit */
    int64_t wait_samples = timestamp - samples;
    int64_t wait_ns = wait_samples * 1000000000 / rate;
    
    struct timespec ts[1];
    ts->tv_sec = wait_ns / 1000000000;
    ts->tv_nsec = wait_ns % 1000000000;
#if 0
    fprintf(stderr,
            "samples=%8"PRIu64" timestamp=%8"PRIu64" wait=%"PRId64" (%"PRId64"ns)\n",
            samples, timestamp, wait_samples, wait_ns);
#endif
    while(nanosleep(ts, ts) < 0 && errno == EINTR)
      ;
  } else {
#if 0
    fprintf(stderr, "samples=%8"PRIu64" timestamp=%8"PRIu64"\n",
            samples, timestamp);
#endif
  }
  /* If samples >= timestamp then it's time, or gone time, to play the
   * timestamp'th sample.  So we return immediately. */
  return timestamp;
}

/** @brief Report how many samples we actually sent
 * @param nsamples_sent Number of samples sent
 */
void uaudio_schedule_sent(size_t nsamples_sent) {
  timestamp += nsamples_sent;
}

/** @brief Initialize audio scheduling
 *
 * Should be called from your API's @c start callback.
 */
void uaudio_schedule_init(void) {
  /* uaudio_schedule_play() will spot this and choose an initial value */
  base.tv_sec = 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
