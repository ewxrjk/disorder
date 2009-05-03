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

/** @file lib/uaudio.h
 * @brief Uniform audio interface
 */

#ifndef UAUDIO_H
#define UAUDIO_H

extern int uaudio_rate;
extern int uaudio_bits;
extern int uaudio_channels;
extern int uaudio_signed;
extern size_t uaudio_sample_size;

/** @brief Callback to get audio data
 * @param buffer Where to put audio data
 * @param max_samples How many samples to supply
 * @param userdata As passed to uaudio_open()
 * @return Number of samples filled
 *
 * This function should not block if possible (better to fill the buffer with
 * 0s) and should definitely not block indefinitely.  This great caution with
 * any locks or syscalls!  In particular avoid it taking a lock that may be
 * held while any of the @ref uaudio members are called.
 *
 * If it's more convenient, it's OK to return less than the maximum number of
 * samples (including 0) provided you expect to be called again for more
 * samples immediately.
 */
typedef size_t uaudio_callback(void *buffer,
                               size_t max_samples,
                               void *userdata);

/** @brief Callback to play audio data
 * @param buffer Pointer to audio buffer
 * @param samples Number of samples to play
 * @param flags Flags word
 * @return Number of samples played
 *
 * Used with uaudio_thread_start() etc.
 *
 * @p flags is a bitmap giving the current pause state and transitions:
 * - @ref UAUDIO_PAUSE if this is the first call of a pause
 * - @ref UAUDIO_RESUME if this is the first call of a resumse
 * - @ref UAUDIO_PLAYING if this is outside a pause
 * - @ref UAUDIO_PAUSED if this is in a pause
 *
 * During a pause, the sample data is guaranteed to be 0.
 */
typedef size_t uaudio_playcallback(void *buffer, size_t samples,
                                   unsigned flags);

/** @brief Start of a pause */
#define UAUDIO_PAUSE 0x0001

/** @brief End of a pause */
#define UAUDIO_RESUME 0x0002

/** @brief Currently playing */
#define UAUDIO_PLAYING 0x0004

/** @brief Currently paused */
#define UAUDIO_PAUSED 0x0008

/** @brief Audio API definition */
struct uaudio {
  /** @brief Name of this API */
  const char *name;

  /** @brief List of options, terminated by NULL */
  const char *const *options;

  /** @brief Do slow setup
   * @param ua Handle returned by uaudio_open()
   * @param callback Called for audio data
   * @param userdata Passed to @p callback
   *
   * This does resource-intensive setup for the output device.
   *
   * For instance it might open mixable audio devices or network sockets.  It
   * will create any background thread required.  However, it must not exclude
   * other processes from outputting sound.
   */
  void (*start)(uaudio_callback *callback,
                void *userdata);

  /** @brief Tear down
   * @param ua Handle returned by uaudio_open()
   *
   * This undoes the effect of @c start.
   */
  void (*stop)(void);

  /** @brief Enable output
   *
   * A background thread will start calling @c callback as set by @c
   * start and playing the audio data received from it.
   */
  void (*activate)(void);

  /** @brief Disable output
   *
   * The background thread will stop calling @c callback.
   */
  void (*deactivate)(void);

  /** @brief Open mixer device */
  void (*open_mixer)(void);

  /** @brief Closer mixer device */
  void (*close_mixer)(void);

  /** @brief Get volume
   * @param left Where to put the left-channel value
   * @param right Where to put the right-channel value
   *
   * 0 is silent and 100 is maximum volume.
   */
  void (*get_volume)(int *left, int *right);

  /** @brief Set volume
   * @param left Pointer to left-channel value (updated)
   * @param right Pointer to right-channel value (updated)
   *
   * The values are updated with those actually set by the underlying system
   * call.
   *
   * 0 is silent and 100 is maximum volume.
   */
  void (*set_volume)(int *left, int *right);

  /** @brief Set configuration */
  void (*configure)(void);
  
};

void uaudio_set_format(int rate, int channels, int samplesize, int signed_);
void uaudio_set(const char *name, const char *value);
char *uaudio_get(const char *name, const char *default_value);
void uaudio_thread_start(uaudio_callback *callback,
			 void *userdata,
			 uaudio_playcallback *playcallback,
			 size_t min,
                         size_t max,
                         unsigned flags);

void uaudio_thread_stop(void);
void uaudio_thread_activate(void);
void uaudio_thread_deactivate(void);
uint32_t uaudio_schedule_sync(void);
void uaudio_schedule_sent(size_t nsamples_sent);
void uaudio_schedule_init(void);
const struct uaudio *uaudio_find(const char *name);

extern uint64_t uaudio_schedule_timestamp;
extern int uaudio_schedule_reactivated;

#if HAVE_COREAUDIO_AUDIOHARDWARE_H
extern const struct uaudio uaudio_coreaudio;
#endif

#if HAVE_ALSA_ASOUNDLIB_H
extern const struct uaudio uaudio_alsa;
#endif

#if HAVE_SYS_SOUNDCARD_H || EMPEG_HOST
extern const struct uaudio uaudio_oss;
#endif

extern const struct uaudio uaudio_rtp;

extern const struct uaudio uaudio_command;

extern const struct uaudio *const uaudio_apis[];

#endif /* UAUDIO_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
