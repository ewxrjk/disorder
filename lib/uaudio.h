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
 */
typedef size_t uaudio_callback(void *buffer,
                               size_t max_samples,
                               void *userdata);

/** @brief Callback to play audio data
 * @param buffer Pointer to audio buffer
 * @param samples Number of samples to play
 * @return Number of samples played
 *
 * Used with uaudio_thread_start() etc.
 */
typedef size_t uaudio_playcallback(void *buffer, size_t samples);

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

};

void uaudio_set_format(int rate, int channels, int samplesize, int signed_);
void uaudio_set(const char *name, const char *value);
char *uaudio_get(const char *name);
void uaudio_thread_start(uaudio_callback *callback,
			 void *userdata,
			 uaudio_playcallback *playcallback,
			 size_t min,
                         size_t max);
void uaudio_thread_stop(void);
void uaudio_thread_activate(void);
void uaudio_thread_deactivate(void);
void uaudio_schedule_synchronize(void);
void uaudio_schedule_update(size_t written_samples);
void uaudio_schedule_init(void);

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

extern const struct uaudio *uaudio_apis[];

#endif /* UAUDIO_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
