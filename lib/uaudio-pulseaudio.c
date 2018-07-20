/*
 * This file is part of DisOrder.
 * Copyright (C) 2013 Richard Kettlewell, 2018 Mark Wooding
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

#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/thread-mainloop.h>
#include <pulse/stream.h>

#include "mem.h"
#include "log.h"
#include "uaudio.h"
#include "configuration.h"

static const char *const pulseaudio_options[] = {
  "application",
  NULL
};

static pa_threaded_mainloop *loop;
static pa_context *ctx;
static pa_stream *str;

#define PAERRSTR pa_strerror(pa_context_errno(ctx))

/** @brief Callback: wake up main loop when the context is ready. */
static void cb_ctxstate(attribute((unused)) pa_context *xctx,
                        attribute((unused)) void *p) {
  pa_context_state_t st = pa_context_get_state(ctx);
  switch(st) {
  case PA_CONTEXT_READY:
    pa_threaded_mainloop_signal(loop, 0);
    break;
  default:
    if(!PA_CONTEXT_IS_GOOD(st))
      disorder_fatal(0, "pulseaudio failed: %s", PAERRSTR);
  }
}

/** @brief Callback: wake up main loop when the stream is ready. */
static void cb_strstate(attribute((unused)) pa_stream *xstr,
                        attribute((unused)) void *p) {
  pa_stream_state_t st = pa_stream_get_state(str);
  switch(st) {
  case PA_STREAM_READY:
    pa_threaded_mainloop_signal(loop, 0);
    break;
  default:
    if(!PA_STREAM_IS_GOOD(st))
      disorder_fatal(0, "pulseaudio failed: %s", PAERRSTR);
  }
}

/** @brief Callback: wake up main loop when there's output buffer space. */
static void cb_wakeup(attribute((unused)) pa_stream *xstr,
                      attribute((unused)) size_t sz,
                      attribute((unused)) void *p)
  { pa_threaded_mainloop_signal(loop, 0); }

/** @brief Open the PulseAudio sound device */
static void pulseaudio_open() {
  /* Much of the following is cribbed from the PulseAudio `simple' source. */

  pa_sample_spec ss;

  /* Set up the sample format. */
  ss.format = -1;
  switch(uaudio_bits) {
  case 8: if(!uaudio_signed) ss.format = PA_SAMPLE_U8; break;
  case 16: if(uaudio_signed) ss.format = PA_SAMPLE_S16NE; break;
  case 32: if(uaudio_signed) ss.format = PA_SAMPLE_S32NE; break;
  }
  if(ss.format == -1)
    disorder_fatal(0, "unsupported uaudio format (%d, %d)",
                   uaudio_bits, uaudio_signed);
  ss.channels = uaudio_channels;
  ss.rate = uaudio_rate;

  /* Create the random PulseAudio pieces. */
  loop = pa_threaded_mainloop_new();
  if(!loop) disorder_fatal(0, "failed to create pulseaudio main loop");
  pa_threaded_mainloop_lock(loop);
  ctx = pa_context_new(pa_threaded_mainloop_get_api(loop),
                       uaudio_get("application", "DisOrder"));
  if(!ctx) disorder_fatal(0, "failed to create pulseaudio context");
  pa_context_set_state_callback(ctx, cb_ctxstate, 0);
  if(pa_context_connect(ctx, 0, 0, 0) < 0)
    disorder_fatal(0, "failed to connect to pulseaudio server: %s",
                   PAERRSTR);

  /* Set the main loop going. */
  if(pa_threaded_mainloop_start(loop) < 0)
    disorder_fatal(0, "failed to start pulseaudio main loop");
  while(pa_context_get_state(ctx) != PA_CONTEXT_READY)
    pa_threaded_mainloop_wait(loop);

  /* Set up my stream. */
  str = pa_stream_new(ctx, "DisOrder", &ss, 0);
  if(!str)
    disorder_fatal(0, "failed to create pulseaudio stream: %s", PAERRSTR);
  pa_stream_set_write_callback(str, cb_wakeup, 0);
  if(pa_stream_connect_playback(str, 0, 0,
                                PA_STREAM_ADJUST_LATENCY,
                                0, 0))
    disorder_fatal(0, "failed to connect pulseaudio stream for playback: %s",
                   PAERRSTR);
  pa_stream_set_state_callback(str, cb_strstate, 0);

  /* Wait until the stream is ready. */
  while(pa_stream_get_state(str) != PA_STREAM_READY)
    pa_threaded_mainloop_wait(loop);

  /* All done. */
  pa_threaded_mainloop_unlock(loop);
}

/** @brief Close the PulseAudio sound device */
static void pulseaudio_close(void) {
  if(loop) pa_threaded_mainloop_stop(loop);
  if(str) { pa_stream_unref(str); str = 0; }
  if(ctx) { pa_context_disconnect(ctx); pa_context_unref(ctx); ctx = 0; }
  if(loop) { pa_threaded_mainloop_free(loop); loop = 0; }
}

/** @brief Actually play sound via PulseAudio */
static size_t pulseaudio_play(void *buffer, size_t samples,
                              attribute((unused)) unsigned flags) {
  unsigned char *p = buffer;
  size_t n, sz = samples*uaudio_sample_size;

  pa_threaded_mainloop_lock(loop);
  while(sz) {

    /* Wait until some output space becomes available. */
    while(!(n = pa_stream_writable_size(str)))
      pa_threaded_mainloop_wait(loop);
    if(n > sz) n = sz;
    if(pa_stream_write(str, p, n, 0, 0, PA_SEEK_RELATIVE) < 0)
      disorder_fatal(0, "failed to write pulseaudio data: %s", PAERRSTR);
    p += n; sz -= n;
  }
  pa_threaded_mainloop_unlock(loop);
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
