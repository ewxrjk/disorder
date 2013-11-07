/*
 * This file is part of DisOrder.
 * Copyright (C) 2013 Richard Kettlewell
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
/** @file lib/uaudio-pulseaudio.c
 * @brief Support for PulseAudio backend */
#include "common.h"

#if HAVE_PULSEAUDIO

#include <pulse/simple.h>
#include <pulse/error.h>

#include "mem.h"
#include "log.h"
#include "uaudio.h"
#include "configuration.h"

static const char *const pulseaudio_options[] = {
  "application",
  NULL
};

static pa_simple *pulseaudio_simple_handle;

/** @brief Open the PulseAudio sound device */
static void pulseaudio_open() {
  pa_sample_spec ss;
  int error;
  ss.format = -1;
  switch(uaudio_bits) {
  case 8:
    if(!uaudio_signed)
      ss.format = PA_SAMPLE_U8;
    break;
  case 16:
    if(uaudio_signed)
      ss.format = PA_SAMPLE_S16NE;
    break;
  case 32:
    if(uaudio_signed)
      ss.format = PA_SAMPLE_S32NE;
    break;
  }
  if(ss.format == -1)
    disorder_fatal(0, "unsupported uaudio format (%d, %d)",
                   uaudio_bits, uaudio_signed);
  ss.channels = uaudio_channels;
  ss.rate = uaudio_rate;
  pulseaudio_simple_handle = pa_simple_new(NULL,
                                           uaudio_get("application", "DisOrder"),
                                           PA_STREAM_PLAYBACK,
                                           NULL,
                                           "DisOrder",
                                           &ss,
                                           NULL,
                                           NULL,
                                           &error);
  if(!pulseaudio_simple_handle)
    disorder_fatal(0, "pa_simple_new: %s", pa_strerror(error));
}

/** @brief Close the PulseAudio sound device */
static void pulseaudio_close(void) {
  pa_simple_free(pulseaudio_simple_handle);
  pulseaudio_simple_handle = NULL;
}

/** @brief Actually play sound via PulseAudio */
static size_t pulseaudio_play(void *buffer, size_t samples,
                              unsigned attribute((unused)) flags) {
  int error;
  int ret = pa_simple_write(pulseaudio_simple_handle,
                            buffer,
                            samples * uaudio_sample_size,
                            &error);
  if(ret < 0)
    disorder_fatal(0, "pa_simple_write: %s", pa_strerror(error));
  return samples;
}

static void pulseaudio_start(uaudio_callback *callback,
                             void *userdata) {
  pulseaudio_open();
  uaudio_thread_start(callback, userdata, pulseaudio_play,
                      32 / uaudio_sample_size,
                      4096 / uaudio_sample_size,
                      0);
}

static void pulseaudio_stop(void) {
  uaudio_thread_stop();
  pulseaudio_close();
}

static void pulseaudio_open_mixer(void) {
  disorder_error(0, "no pulseaudio mixer support yet");
}

static void pulseaudio_close_mixer(void) {
}

static void pulseaudio_get_volume(int *left, int *right) {
  *left = *right = 0;
}

static void pulseaudio_set_volume(int *left, int *right) {
  *left = *right = 0;
}

static void pulseaudio_configure(void) {
}

const struct uaudio uaudio_pulseaudio = {
  .name = "pulseaudio",
  .options = pulseaudio_options,
  .start = pulseaudio_start,
  .stop = pulseaudio_stop,
  .activate = uaudio_thread_activate,
  .deactivate = uaudio_thread_deactivate,
  .open_mixer = pulseaudio_open_mixer,
  .close_mixer = pulseaudio_close_mixer,
  .get_volume = pulseaudio_get_volume,
  .set_volume = pulseaudio_set_volume,
  .configure = pulseaudio_configure,
  .flags = UAUDIO_API_CLIENT,
};

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
