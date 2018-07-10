/*
 * This file is part of DisOrder.
 * Copyright (C) 2018 Mark Wooding
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
/** @file plugins/tracklength-gstreamer.c
 * @brief Plugin to compute track lengths using GStreamer
 */
#include "tracklength.h"

#include "speaker-protocol.h"

/* Ugh.  It turns out that libxml tries to define a function called
 * `attribute', and it's included by GStreamer for some unimaginable reason.
 * So undefine it here.  We'll want GCC attributes for special effects, but
 * can take care of ourselves.
 */
#undef attribute

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>

/* The only application we have for `attribute' is declaring function
 * arguments as being unused, because we have a lot of callback functions
 * which are meant to comply with an externally defined interface.
 */
#ifdef __GNUC__
#  define UNUSED __attribute__((unused))
#endif

#define END ((void *)0)

static int inited_gstreamer = 0;

enum { ST_PAUSE, ST_PAUSED, ST_PLAY, ST_EOS, ST_BOTCHED = -1 };

struct tracklength_state {
  const char *path;
  int state;
  GMainLoop *loop;
  GstElement *pipeline;
  GstElement *sink;
};

static void decoder_pad_arrived(GstElement *elt, GstPad *pad, gpointer p) {
  /* The `decodebin' element dreamed up a pad.  If it's for audio output,
   * attach it to a dummy sink so that it doesn't become sad.
   */

  struct tracklength_state *state = p;
  GstCaps *caps = 0;
  GstStructure *s;
  gchar *padname = 0;
  guint i, n;

#ifdef HAVE_GSTREAMER_0_10
  caps = gst_pad_get_caps(pad); if(!caps) goto end;
#else
  caps = gst_pad_get_current_caps(pad); if(!caps) goto end;
#endif
  for(i = 0, n = gst_caps_get_size(caps); i < n; i++) {
    s = gst_caps_get_structure(caps, i);
    if(!strncmp(gst_structure_get_name(s), "audio/", 6)) goto match;
  }
  goto end;

match:
  padname = gst_pad_get_name(pad);
  if(!padname) {
    disorder_error(0, "error checking `%s': "
                   "failed to get GStreamer pad name (out of memory?)",
                   state->path);
    goto botched;
  }
  if(!gst_element_link_pads(elt, padname, state->sink, "sink")) {
    disorder_error(0, "error checking `%s': "
                   "failed to link GStreamer elements `%s' and `sink'",
                   state->path, GST_OBJECT_NAME(elt));
    goto botched;
  }
  goto end;

botched:
  state->state = ST_BOTCHED;
  g_main_loop_quit(state->loop);
end:
  if(caps) gst_caps_unref(caps);
  if(padname) g_free(padname);
}

static void bus_message(GstBus UNUSED *bus, GstMessage *msg, gpointer p) {
  /* Our bus sent us a message.  If it's interesting, maybe switch state, and
   * wake the main loop up.
   */

  struct tracklength_state *state = p;
  GstState newstate;

  switch(GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_ERROR:
    /* An error.  Mark the operation as botched and wake up the main loop. */
    disorder_error(0, "error checking `%s': %s", state->path,
                   gst_structure_get_string(gst_message_get_structure(msg),
                                            "debug"));
    state->state = ST_BOTCHED;
    g_main_loop_quit(state->loop);
    break;
  case GST_MESSAGE_STATE_CHANGED:
    /* A state change.  If we've reached `PAUSED', then notify the main loop.
     * This turns out to be a point at which the duration becomes available.
     * If not, then the main loop will set us playing.
     */
    gst_message_parse_state_changed(msg, 0, &newstate, 0);
    if(state->state == ST_PAUSE && newstate == GST_STATE_PAUSED &&
       GST_MESSAGE_SRC(msg) == GST_OBJECT(state->pipeline)) {
      g_main_loop_quit(state->loop);
      state->state = ST_PAUSED;
    }
    break;
  case GST_MESSAGE_EOS:
    /* The end of the stream.  Wake up the loop, and set the state to mark this
     * as being the end of the line.
     */
    state->state = ST_EOS;
    g_main_loop_quit(state->loop);
    break;
  case GST_MESSAGE_DURATION_CHANGED:
    /* Something thinks it knows a duration.  Wake up the main loop just in
     * case.
     */
    break;
  default:
    /* Something else happened.  Whatevs. */
    break;
  }
}

static int query_track_length(struct tracklength_state *state,
                              long *length_out)
{
  /* Interrogate the pipeline to find the track length.  Return zero on
   * success, or -1 on failure.  This is annoying and nonportable.
   */

  gint64 t;
#ifdef HAVE_GSTREAMER_0_10
  GstFormat fmt = GST_FORMAT_TIME;
#endif

#ifdef HAVE_GSTREAMER_0_10
  if(!gst_element_query_duration(state->pipeline, &fmt, &t) ||
     fmt != GST_FORMAT_TIME)
    return -1;
#else
  if(!gst_element_query_duration(state->pipeline, GST_FORMAT_TIME, &t))
    return -1;
#endif
  *length_out = (t + 500000000)/1000000000;
  return 0;
}

long disorder_tracklength(const char UNUSED *track, const char *path) {
  /* Discover the length of a track. */

  struct tracklength_state state;
  GstElement *source, *decode, *sink;
  GstBus *bus = 0;
  int running = 0;
  long length = -1;

  /* Fill in the state structure. */
  state.path = path;
  state.state = ST_PAUSE;
  state.pipeline = 0;
  state.loop = 0;

  /* Set up the GStreamer machinery. */
  if(!inited_gstreamer) gst_init(0, 0);

  /* Create the necessary pipeline elements. */
  source = gst_element_factory_make("filesrc", "file");
  decode = gst_element_factory_make("decodebin", "decode");
  sink = state.sink = gst_element_factory_make("fakesink", "sink");
  state.pipeline = gst_pipeline_new("pipe");
  if(!source || !decode || !sink) {
    disorder_error(0, "failed to create GStreamer elements: "
                   "need base and good plugins");
    goto end;
  }
  g_object_set(source, "location", path, END);

  /* Add the elements to the pipeline.  It will take over responsibility for
   * them.
   */
  gst_bin_add_many(GST_BIN(state.pipeline), source, decode, sink, END);

  /* Link the elements together as far as we can.  Arrange to link the decoder
   * onto our (dummy) sink when it's ready to produce output.
   */
  if(!gst_element_link(source, decode)) {
    disorder_error(0, "error checking `%s': "
                   "failed to link GStreamer elements `file' and `decode'",
                   path);
    goto end;
  }
  g_signal_connect(decode, "pad-added",
                   G_CALLBACK(decoder_pad_arrived), &state);

  /* Fetch the bus and listen for messages. */
  bus = gst_pipeline_get_bus(GST_PIPELINE(state.pipeline));
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(bus_message), &state);

  /* Turn the handle until we have an answer.  The message handler will wake us
   * up if: the pipeline reports that the duration has changed (suggesting that
   * something might know what it is); we successfully reach the initial
   * `paused' state; or we hit the end of the stream (by which point we ought
   * to know where we are).
   */
  state.loop = g_main_loop_new(0, FALSE);
  gst_element_set_state(state.pipeline, GST_STATE_PAUSED); running = 1;
  for(;;) {
    g_main_loop_run(state.loop);
    if(!query_track_length(&state, &length)) goto end;
    switch(state.state) {
    case ST_BOTCHED: goto end;
    case ST_EOS:
      disorder_error(0, "error checking `%s': "
                     "failed to fetch duration from GStreamer pipeline",
                     path);
      goto end;
    case ST_PAUSED:
      gst_element_set_state(state.pipeline, GST_STATE_PLAYING);
      state.state = ST_PLAY;
      break;
    }
  }

end:
  /* Well, it's too late to worry about anything now. */
  if(running) gst_element_set_state(state.pipeline, GST_STATE_NULL);
  if(bus) {
    gst_bus_remove_signal_watch(bus);
    gst_object_unref(bus);
  }
  if(state.pipeline)
    gst_object_unref(state.pipeline);
  else {
    if(source) gst_object_unref(source);
    if(decode) gst_object_unref(decode);
    if(sink) gst_object_unref(sink);
  }
  if(state.loop) g_main_loop_unref(state.loop);
  return length;
}

#ifdef STANDALONE
int main(int argc, char *argv[]) {
  int i;

  for(i = 1; i < argc; i++)
    printf("%s: %ld\n", argv[i], disorder_tracklength(0, argv[i]));
  return (0);
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
