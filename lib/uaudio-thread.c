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
/** @file lib/uaudio-thread.c
 * @brief Background thread for audio processing */
#include "common.h"

#include <pthread.h>
#include <unistd.h>

#include "uaudio.h"
#include "log.h"
#include "mem.h"

/** @brief Number of buffers
 *
 * Must be at least 2 and should normally be at least 3.  We maintain multiple
 * buffers so that we can read new data into one while the previous is being
 * played.
 */
#define UAUDIO_THREAD_BUFFERS 4

/** @brief Buffer data structure */
struct uaudio_buffer {
  /** @brief Pointer to sample data */
  void *samples;

  /** @brief Count of samples */
  size_t nsamples;
};

/** @brief Input buffers
 *
 * This is actually a ring buffer, managed by @ref uaudio_collect_buffer and
 * @ref uaudio_play_buffer.
 *
 * Initially both pointers are 0.  Whenever the pointers are equal, we
 * interpreted this as meaning that there is no data stored at all.  A
 * consequence of this is that maximal occupancy is when the collect point is
 * just before the play point, so at least one buffer is always empty (hence it
 * being good for @ref UAUDIO_THREAD_BUFFERS to be at least 3).
 */
static struct uaudio_buffer uaudio_buffers[UAUDIO_THREAD_BUFFERS];

/** @brief Buffer to read into */
static unsigned uaudio_collect_buffer;

/** @brief Buffer to play from */
static unsigned uaudio_play_buffer;

/** @brief Collection thread ID */
static pthread_t uaudio_collect_thread;

/** @brief Playing thread ID */
static pthread_t uaudio_play_thread;

/** @brief Flags */
static unsigned uaudio_thread_flags;

static uaudio_callback *uaudio_thread_collect_callback;
static uaudio_playcallback *uaudio_thread_play_callback;
static void *uaudio_thread_userdata;
static int uaudio_thread_started;
static int uaudio_thread_collecting;
static pthread_mutex_t uaudio_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t uaudio_thread_cond = PTHREAD_COND_INITIALIZER;

/** @brief Minimum number of samples per chunk */
static size_t uaudio_thread_min;

/** @brief Maximum number of samples per chunk */
static size_t uaudio_thread_max;

/** @brief Set when activated, clear when paused */
static int uaudio_thread_activated;

/** @brief Return number of buffers currently in use */
static int uaudio_buffers_used(void) {
  return (uaudio_collect_buffer - uaudio_play_buffer) % UAUDIO_THREAD_BUFFERS;
}

/** @brief Background thread for audio collection
 *
 * Collects data while activated and communicates its status via @ref
 * uaudio_thread_collecting.
 */
static void *uaudio_collect_thread_fn(void attribute((unused)) *arg) {
  pthread_mutex_lock(&uaudio_thread_lock);
  while(uaudio_thread_started) {
    /* Wait until we're activatd */
    if(!uaudio_thread_activated) {
      pthread_cond_wait(&uaudio_thread_cond, &uaudio_thread_lock);
      continue;
    }
    /* We are definitely active now */
    uaudio_thread_collecting = 1;
    pthread_cond_broadcast(&uaudio_thread_cond);
    while(uaudio_thread_activated) {
      if(uaudio_buffers_used() < UAUDIO_THREAD_BUFFERS - 1) {
        /* At least one buffer is available.  We release the lock while
         * collecting data so that other already-filled buffers can be played
         * without delay.  */
        struct uaudio_buffer *const b = &uaudio_buffers[uaudio_collect_buffer];
        pthread_mutex_unlock(&uaudio_thread_lock);
        //fprintf(stderr, "C%d.", uaudio_collect_buffer);
        
        /* Keep on trying until we get the minimum required amount of data */
        b->nsamples = 0;
        if(uaudio_thread_activated) {
          while(b->nsamples < uaudio_thread_min) {
            b->nsamples += uaudio_thread_collect_callback
              ((char *)b->samples
               + b->nsamples * uaudio_sample_size,
               uaudio_thread_max - b->nsamples,
               uaudio_thread_userdata);
          }
        }
        pthread_mutex_lock(&uaudio_thread_lock);
        /* Advance to next buffer */
        uaudio_collect_buffer = (1 + uaudio_collect_buffer) % UAUDIO_THREAD_BUFFERS;
        /* Awaken player */
        pthread_cond_broadcast(&uaudio_thread_cond);
      } else
        /* No space, wait for player */
        pthread_cond_wait(&uaudio_thread_cond, &uaudio_thread_lock);
    }
    uaudio_thread_collecting = 0;
    pthread_cond_broadcast(&uaudio_thread_cond);
  }
  pthread_mutex_unlock(&uaudio_thread_lock);
  return NULL;
}

/** @brief Background thread for audio playing 
 *
 * This thread plays data as long as there is something to play.  So the
 * buffers will drain to empty before deactivation completes.
 */
static void *uaudio_play_thread_fn(void attribute((unused)) *arg) {
  int resync = 1;
  unsigned last_flags = 0;
  unsigned char zero[uaudio_thread_max * uaudio_sample_size];
  memset(zero, 0, sizeof zero);

  while(uaudio_thread_started) {
    // If we're paused then just play silence
    if(!uaudio_thread_activated) {
      pthread_mutex_unlock(&uaudio_thread_lock);
      unsigned flags = UAUDIO_PAUSED;
      if(last_flags & UAUDIO_PLAYING)
        flags |= UAUDIO_PAUSE;
      uaudio_thread_play_callback(zero, uaudio_thread_max,
                                  last_flags = flags);
      /* We expect the play callback to block for a reasonable period */
      pthread_mutex_lock(&uaudio_thread_lock);
      continue;
    }
    const int used = uaudio_buffers_used();
    int go;

    if(resync)
      go = (used == UAUDIO_THREAD_BUFFERS - 1);
    else
      go = (used > 0);
    if(go) {
      /* At least one buffer is filled.  We release the lock while playing so
       * that more collection can go on. */
      struct uaudio_buffer *const b = &uaudio_buffers[uaudio_play_buffer];
      pthread_mutex_unlock(&uaudio_thread_lock);
      //fprintf(stderr, "P%d.", uaudio_play_buffer);
      size_t played = 0;
      while(played < b->nsamples) {
        unsigned flags = UAUDIO_PLAYING;
        if(last_flags & UAUDIO_PAUSED)
          flags |= UAUDIO_RESUME;
        played += uaudio_thread_play_callback((char *)b->samples
                                              + played * uaudio_sample_size,
                                              b->nsamples - played,
                                              last_flags = flags);
      }
      pthread_mutex_lock(&uaudio_thread_lock);
      /* Move to next buffer */
      uaudio_play_buffer = (1 + uaudio_play_buffer) % UAUDIO_THREAD_BUFFERS;
      /* Awaken collector */
      pthread_cond_broadcast(&uaudio_thread_cond);
      resync = 0;
    } else {
      /* Insufficient data to play, wait for collector */
      pthread_cond_wait(&uaudio_thread_cond, &uaudio_thread_lock);
      /* (Still) re-synchronizing */
      resync = 1;
    }
  }
  pthread_mutex_unlock(&uaudio_thread_lock);
  return NULL;
}

/** @brief Create background threads for audio processing 
 * @param callback Callback to collect audio data
 * @param userdata Passed to @p callback
 * @param playcallback Callback to play audio data
 * @param min Minimum number of samples to play in a chunk
 * @param max Maximum number of samples to play in a chunk
 * @param flags Flags (not currently used)
 *
 * @p callback will be called multiple times in quick succession if necessary
 * to gather at least @p min samples.  Equally @p playcallback may be called
 * repeatedly in quick succession to play however much was received in a single
 * chunk.
 */
void uaudio_thread_start(uaudio_callback *callback,
			 void *userdata,
			 uaudio_playcallback *playcallback,
			 size_t min,
                         size_t max,
                         unsigned flags) {
  int e;
  uaudio_thread_collect_callback = callback;
  uaudio_thread_userdata = userdata;
  uaudio_thread_play_callback = playcallback;
  uaudio_thread_min = min;
  uaudio_thread_max = max;
  uaudio_thread_flags = flags;
  uaudio_thread_started = 1;
  uaudio_thread_activated = 0;
  for(int n = 0; n < UAUDIO_THREAD_BUFFERS; ++n)
    uaudio_buffers[n].samples = xcalloc_noptr(uaudio_thread_max,
                                              uaudio_sample_size);
  uaudio_collect_buffer = uaudio_play_buffer = 0;
  if((e = pthread_create(&uaudio_collect_thread,
                         NULL,
                         uaudio_collect_thread_fn,
                         NULL)))
    fatal(e, "pthread_create");
  if((e = pthread_create(&uaudio_play_thread,
                         NULL,
                         uaudio_play_thread_fn,
                         NULL)))
    fatal(e, "pthread_create");
}

/** @brief Shut down background threads for audio processing */
void uaudio_thread_stop(void) {
  void *result;

  pthread_mutex_lock(&uaudio_thread_lock);
  uaudio_thread_activated = 0;
  uaudio_thread_started = 0;
  pthread_cond_broadcast(&uaudio_thread_cond);
  pthread_mutex_unlock(&uaudio_thread_lock);
  pthread_join(uaudio_collect_thread, &result);
  pthread_join(uaudio_play_thread, &result);
  for(int n = 0; n < UAUDIO_THREAD_BUFFERS; ++n)
    xfree(uaudio_buffers[n].samples);
}

/** @brief Activate audio output */
void uaudio_thread_activate(void) {
  pthread_mutex_lock(&uaudio_thread_lock);
  uaudio_thread_activated = 1;
  pthread_cond_broadcast(&uaudio_thread_cond);
  pthread_mutex_unlock(&uaudio_thread_lock);
}

/** @brief Deactivate audio output */
void uaudio_thread_deactivate(void) {
  pthread_mutex_lock(&uaudio_thread_lock);
  uaudio_thread_activated = 0; 
  pthread_cond_broadcast(&uaudio_thread_cond);
  pthread_mutex_unlock(&uaudio_thread_lock);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
