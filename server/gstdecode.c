/*
 * This file is part of DisOrder
 * Copyright (C) 2013 Mark Wooding
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
/** @file server/gstdecode.c
 * @brief Decode compressed audio files, and apply ReplayGain.
 */

#include "disorder-server.h"

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
#define N(v) (sizeof(v)/sizeof(*(v)))

static FILE *fp;
static const char *file;
static GstAppSink *appsink;
static GstElement *pipeline;
static GMainLoop *loop;
static unsigned flags = 0;
#define f_stream 1u

#define MODES(_) _("off", OFF) _("track", TRACK) _("album", ALBUM)
enum {
#define DEFENUM(name, tag) tag,
  MODES(DEFENUM)
#undef DEFENUM
  NMODES
};
static const char *const modes[] = {
#define DEFNAME(name, tag) name,
  MODES(DEFNAME)
#undef DEFNAME
  0
};

static const char *const dithers[] = {
  "none", "rpdf", "tpdf", "tpdf-hf", 0
};

static const char *const shapes[] = {
  "none", "error-feedback", "simple", "medium", "high", 0
};

static int dither = -1;
static int mode = ALBUM;
static int quality = -1;
static int shape = -1;
static gdouble fallback = 0.0;

static struct stream_header hdr;

/* Report the pads of an element ELT, as iterated by IT; WHAT is an adjective
 * phrase describing the pads for use in the output.
 */
static void report_element_pads(const char *what, GstElement *elt,
                                GstIterator *it)
{
  gchar *cs;
#ifdef HAVE_GSTREAMER_0_10
  gpointer pad;
#else
  GValue gv;
  GstPad *pad;
  GstCaps *caps;
#endif

  for(;;) {
#ifdef HAVE_GSTREAMER_0_10
    switch(gst_iterator_next(it, &pad)) {
#else
    switch(gst_iterator_next(it, &gv)) {
#endif
    case GST_ITERATOR_DONE:
      goto done;
    case GST_ITERATOR_OK:
#ifdef HAVE_GSTREAMER_0_10
      cs = gst_caps_to_string(gst_pad_get_caps(pad));
#else
      assert(G_VALUE_HOLDS(&gv, GST_TYPE_PAD));
      pad = g_value_get_object(&gv);
      caps = gst_pad_query_caps(pad, 0);
      cs = gst_caps_to_string(caps);
      g_object_unref(caps);
#endif
      disorder_error(0, "  `%s' %s pad: %s", GST_OBJECT_NAME(elt), what, cs);
      g_free(cs);
#ifdef HAVE_GSTREAMER_0_10
      g_object_unref(pad);
#endif
      break;
    case GST_ITERATOR_RESYNC:
      gst_iterator_resync(it);
      break;
    case GST_ITERATOR_ERROR:
      disorder_error(0, "<failed to enumerate `%s' %s pads>",
                     GST_OBJECT_NAME(elt), what);
      goto done;
    }
  }

done:
  gst_iterator_free(it);
}

/* Link together two elements; fail with an approximately useful error
 * message if it didn't work.
 */
static void link_elements(GstElement *left, GstElement *right)
{
  /* Try to link things together. */
  if(gst_element_link(left, right)) return;

  /* If this didn't work, it's probably for some really hairy reason, so
   * provide a bunch of debugging information.
   */
  disorder_error(0, "failed to link GStreamer elements `%s' and `%s'",
                 GST_OBJECT_NAME(left), GST_OBJECT_NAME(right));
  report_element_pads("source", left, gst_element_iterate_src_pads(left));
  report_element_pads("source", right, gst_element_iterate_sink_pads(right));
  disorder_fatal(0, "can't decode `%s'", file);
}

/* The `decoderbin' element (DECODE) has deigned to announce a new PAD.
 * Maybe we should attach the tag end of our pipeline (starting with the
 * element U) to it.
 */
static void decoder_pad_arrived(GstElement *decode, GstPad *pad, gpointer u)
{
  GstElement *tail = u;
#ifdef HAVE_GSTREAMER_0_10
  GstCaps *caps = gst_pad_get_caps(pad);
#else
  GstCaps *caps = gst_pad_get_current_caps(pad);
#endif
  GstStructure *s;
  guint i, n;
  const gchar *name;

  /* The input file could be more or less anything, so this could be any kind
   * of pad.  We're only interested if it's audio, so let's go check.
   */
  for(i = 0, n = gst_caps_get_size(caps); i < n; i++) {
    s = gst_caps_get_structure(caps, i);
    name = gst_structure_get_name(s);
#ifdef HAVE_GSTREAMER_0_10
    if(strncmp(name, "audio/x-raw-", 12) == 0)
#else
    if(strcmp(name, "audio/x-raw") == 0)
#endif
      goto match;
  }
#ifndef HAVE_GSTREAMER_0_10
  g_object_unref(caps);
#endif
  return;

match:
  /* Yes, it's audio.  Link the two elements together. */
  link_elements(decode, tail);

  /* If requested using the environemnt variable `GST_DEBUG_DUMP_DOT_DIR',
   * write a dump of the now-completed pipeline.
   */
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            "disorder-gstdecode");
}

/* Prepare the GStreamer pipeline, ready to decode the given FILE.  This sets
 * up the variables `appsink' and `pipeline'.
 */
static void prepare_pipeline(void)
{
  GstElement *source = gst_element_factory_make("filesrc", "file");
  GstElement *decode = gst_element_factory_make("decodebin", "decode");
  GstElement *resample = gst_element_factory_make("audioresample",
                                                  "resample");
  GstElement *convert = gst_element_factory_make("audioconvert", "convert");
  GstElement *sink = gst_element_factory_make("appsink", "sink");
  GstElement *tail = sink;
  GstElement *gain;
  GstCaps *caps;
  const struct stream_header *fmt = &config->sample_format;

#ifndef HAVE_GSTREAMER_0_10
  static const struct fmttab {
    const char *fmt;
    unsigned bits;
    unsigned endian;
  } fmttab[] = {
    { "S8", 8, ENDIAN_BIG },
    { "S8", 8, ENDIAN_LITTLE },
    { "S16BE", 16, ENDIAN_BIG },
    { "S16LE", 16, ENDIAN_LITTLE },
    { 0 }
  };
  const struct fmttab *ft;
#endif

  /* Set up the global variables. */
  pipeline = gst_pipeline_new("pipe");
  appsink = GST_APP_SINK(sink);

  /* Configure the various simple elements. */
  g_object_set(source, "location", file, END);
  g_object_set(sink, "sync", FALSE, END);

  /* Configure the resampler and converter.  Leave things as their defaults
   * if the user hasn't made an explicit request.
   */
  if(quality >= 0) g_object_set(resample, "quality", quality, END);
  if(dither >= 0) g_object_set(convert, "dithering", dither, END);
  if(shape >= 0) g_object_set(convert, "noise-shaping", shape, END);

  /* Set up the sink's capabilities from the configuration. */
#ifdef HAVE_GSTREAMER_0_10
  caps = gst_caps_new_simple("audio/x-raw-int",
                             "width", G_TYPE_INT, fmt->bits,
                             "depth", G_TYPE_INT, fmt->bits,
                             "channels", G_TYPE_INT, fmt->channels,
                             "signed", G_TYPE_BOOLEAN, TRUE,
                             "rate", G_TYPE_INT, fmt->rate,
                             "endianness", G_TYPE_INT,
                               fmt->endian == ENDIAN_BIG ?
                                 G_BIG_ENDIAN : G_LITTLE_ENDIAN,
                             END);
#else
  for (ft = fmttab; ft->fmt; ft++)
    if (ft->bits == fmt->bits && ft->endian == fmt->endian) break;
  if(!ft->fmt) {
    disorder_fatal(0, "unsupported sample format: bits=%"PRIu32", endian=%u",
                   fmt->bits, fmt->endian);
  }
  caps = gst_caps_new_simple("audio/x-raw",
                             "format", G_TYPE_STRING, ft->fmt,
                             "channels", G_TYPE_INT, fmt->channels,
                             "rate", G_TYPE_INT, fmt->rate,
                             END);
#endif
  gst_app_sink_set_caps(appsink, caps);

  /* Add the various elements into the pipeline.  We'll stitch them together
   * in pieces, because the pipeline is somewhat dynamic.
   */
  gst_bin_add_many(GST_BIN(pipeline),
                   source, decode,
                   resample, convert, sink, END);

  /* Link audio conversion stages onto the front.  The rest of DisOrder
   * doesn't handle much of the full panoply of exciting audio formats.
   */
  link_elements(convert, tail); tail = convert;
  link_elements(resample, tail); tail = resample;

  /* If we're meant to do ReplayGain then insert it into the pipeline before
   * the converter.
   */
  if(mode != OFF) {
    gain = gst_element_factory_make("rgvolume", "gain");
    g_object_set(gain,
                 "album-mode", mode == ALBUM,
                 "fallback-gain", fallback,
                 END);
    gst_bin_add(GST_BIN(pipeline), gain);
    link_elements(gain, tail); tail = gain;
  }

  /* Link the source and the decoder together.  The `decodebin' is annoying
   * and doesn't have any source pads yet, so the best we can do is make two
   * halves of the chain, and add a hook to stitch them together later.
   */
  link_elements(source, decode);
  g_signal_connect(decode, "pad-added",
                   G_CALLBACK(decoder_pad_arrived), tail);
}

/* Respond to a message from the BUS.  The only thing we need worry about
 * here is errors from the pipeline.
 */
static void bus_message(GstBus UNUSED *bus, GstMessage *msg,
                        gpointer UNUSED u)
{
  switch(msg->type) {
  case GST_MESSAGE_ERROR:
#ifdef HAVE_GSTREAMER_0_10
    disorder_fatal(0, "%s",
                   gst_structure_get_string(msg->structure, "debug"));
#else
    disorder_fatal(0, "%s",
                   gst_structure_get_string(gst_message_get_structure(msg),
                                            "debug"));
#endif
  default:
    break;
  }
}

/* End of stream.  Stop polling the main loop. */
static void cb_eos(GstAppSink UNUSED *sink, gpointer UNUSED u)
  { g_main_loop_quit(loop); }

/* Preroll buffers are prepared when the pipeline moves to the `paused'
 * state, so that they're ready for immediate playback.  Conveniently, they
 * also carry format information, which is what we want here.  Stash the
 * sample format information in the `stream_header' structure ready for
 * actual buffers of interesting data.
 */
static GstFlowReturn cb_preroll(GstAppSink *sink, gpointer UNUSED u)
{
#ifdef HAVE_GSTREAMER_0_10
  GstBuffer *buf = gst_app_sink_pull_preroll(sink);
  GstCaps *caps = GST_BUFFER_CAPS(buf);
#else
  GstSample *samp = gst_app_sink_pull_preroll(sink);
  GstCaps *caps = gst_sample_get_caps(samp);
#endif

#ifdef HAVE_GST_AUDIO_INFO_FROM_CAPS

  /* Parse the audio format information out of the caps.  There's a handy
   * function to do this in later versions of gst-plugins-base, so use that
   * if it's available.  Once we no longer care about supporting such old
   * versions we can delete the version which does the job the hard way.
   */

  GstAudioInfo ai;

  if(!gst_audio_info_from_caps(&ai, caps))
    disorder_fatal(0, "can't decode `%s': failed to parse audio info", file);
  hdr.rate = ai.rate;
  hdr.channels = ai.channels;
  hdr.bits = ai.finfo->width;
  hdr.endian = ai.finfo->endianness == G_BIG_ENDIAN ?
    ENDIAN_BIG : ENDIAN_LITTLE;

#else

  GstStructure *s;
  const char *ty;
  gint rate, channels, bits, endian;
  gboolean signedp;

  /* Make sure that the caps is basically the right shape. */
  if(!GST_CAPS_IS_SIMPLE(caps)) disorder_fatal(0, "expected simple caps");
  s = gst_caps_get_structure(caps, 0);
  ty = gst_structure_get_name(s);
  if(strcmp(ty, "audio/x-raw-int") != 0)
    disorder_fatal(0, "unexpected content type `%s'", ty);

  /* Extract fields from the structure. */
  if(!gst_structure_get(s,
                        "rate", G_TYPE_INT, &rate,
                        "channels", G_TYPE_INT, &channels,
                        "width", G_TYPE_INT, &bits,
                        "endianness", G_TYPE_INT, &endian,
                        "signed", G_TYPE_BOOLEAN, &signedp,
                        END))
    disorder_fatal(0, "can't decode `%s': failed to parse audio caps", file);
  hdr.rate = rate; hdr.channels = channels; hdr.bits = bits;
  hdr.endian = endian == G_BIG_ENDIAN ? ENDIAN_BIG : ENDIAN_LITTLE;

#endif

#ifdef HAVE_GSTREAMER_0_10
  gst_buffer_unref(buf);
#else
  gst_sample_unref(samp);
#endif
  return GST_FLOW_OK;
}

/* A new buffer of sample data has arrived, so we should pass it on with
 * appropriate framing.
 */
static GstFlowReturn cb_buffer(GstAppSink *sink, gpointer UNUSED u)
{
#ifdef HAVE_GSTREAMER_0_10
  GstBuffer *buf = gst_app_sink_pull_buffer(sink);
#else
  GstSample *samp = gst_app_sink_pull_sample(sink);
  GstBuffer *buf = gst_sample_get_buffer(samp);
  GstMemory *mem;
  GstMapInfo map;
  gint i, n;
#endif

  /* Make sure we actually have a grip on the sample format here. */
  if(!hdr.rate) disorder_fatal(0, "format unset");

  /* Write out a frame of audio data. */
#ifdef HAVE_GSTREAMER_0_10
  hdr.nbytes = GST_BUFFER_SIZE(buf);
  if((!(flags&f_stream) && fwrite(&hdr, sizeof(hdr), 1, fp) != 1) ||
     fwrite(GST_BUFFER_DATA(buf), 1, hdr.nbytes, fp) != hdr.nbytes)
    disorder_fatal(errno, "output");
#else
  for(i = 0, n = gst_buffer_n_memory(buf); i < n; i++) {
    mem = gst_buffer_peek_memory(buf, i);
    if(!gst_memory_map(mem, &map, GST_MAP_READ))
      disorder_fatal(0, "failed to map sample buffer");
    hdr.nbytes = map.size;
    if((!(flags&f_stream) && fwrite(&hdr, sizeof(hdr), 1, fp) != 1) ||
       fwrite(map.data, 1, map.size, fp) != map.size)
      disorder_fatal(errno, "output");
    gst_memory_unmap(mem, &map);
  }
#endif

  /* And we're done. */
#ifdef HAVE_GSTREAMER_0_10
  gst_buffer_unref(buf);
#else
  gst_sample_unref(samp);
#endif
  return GST_FLOW_OK;
}

static GstAppSinkCallbacks callbacks = {
  .eos = cb_eos,
  .new_preroll = cb_preroll,
#ifdef HAVE_GSTREAMER_0_10
  .new_buffer = cb_buffer
#else
  .new_sample = cb_buffer
#endif
};

/* Decode the audio file.  We're already set up for everything. */
static void decode(void)
{
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

  /* Set up the message bus and main loop. */
  gst_bus_add_signal_watch(bus);
  loop = g_main_loop_new(0, FALSE);
  g_signal_connect(bus, "message", G_CALLBACK(bus_message), 0);

  /* Tell the sink to call us when interesting things happen. */
  gst_app_sink_set_max_buffers(appsink, 16);
  gst_app_sink_set_drop(appsink, FALSE);
  gst_app_sink_set_callbacks(appsink, &callbacks, 0, 0);

  /* Set the ball rolling. */
  gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

  /* And wait for the miracle to come. */
  g_main_loop_run(loop);

  /* Shut down the pipeline.  This isn't strictly necessary, since we're
   * about to exit very soon, but it's kind of polite.
   */
  gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
}

static int getenum(const char *what, const char *s, const char *const *tags)
{
  int i;

  for(i = 0; tags[i]; i++)
    if(strcmp(s, tags[i]) == 0) return i;
  disorder_fatal(0, "unknown %s `%s'", what, s);
}

static double getfloat(const char *what, const char *s)
{
  double d;
  char *q;

  errno = 0;
  d = strtod(s, &q);
  if(*q || errno) disorder_fatal(0, "invalid %s `%s'", what, s);
  return d;
}

static int getint(const char *what, const char *s, int min, int max)
{
  long i;
  char *q;

  errno = 0;
  i = strtol(s, &q, 10);
  if(*q || errno || min > i || i > max)
    disorder_fatal(0, "invalid %s `%s'", what, s);
  return (int)i;
}

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "dither", required_argument, 0, 'd' },
  { "fallback-gain", required_argument, 0, 'f' },
  { "noise-shape", required_argument, 0, 'n' },
  { "quality", required_argument, 0, 'q' },
  { "replay-gain", required_argument, 0, 'r' },
  { "stream", no_argument, 0, 's' },
  { 0, 0, 0, 0 }
};

static void help(void)
{
  xprintf("Usage:\n"
          "  disorder-gstdecode [OPTIONS] PATH\n"
          "Options:\n"
          "  --help, -h                 Display usage message\n"
          "  --version, -V              Display version number\n"
          "  --config PATH, -c PATH     Set configuration file\n"
          "  --dither TYPE, -d TYPE     TYPE is `none', `rpdf', `tpdf', or "
                                                "`tpdf-hf'\n"
          "  --fallback-gain DB, -f DB  For tracks without ReplayGain data\n"
          "  --noise-shape TYPE, -n TYPE  TYPE is `none', `error-feedback',\n"
          "                                     `simple', `medium' or `high'\n"
          "  --quality QUAL, -q QUAL    Resampling quality: 0 poor, 10 good\n"
          "  --replay-gain MODE, -r MODE  MODE is `off', `track' or `album'\n"
          "  --stream, -s               Output raw samples, without framing\n"
          "\n"
          "Alternative audio decoder for DisOrder.  Only intended to be\n"
          "used by speaker process, not for normal users.\n");
  xfclose(stdout);
  exit(0);
}

/* Main program. */
int main(int argc, char *argv[])
{
  int n;
  const char *e;

  /* Initial setup. */
  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "calling setlocale");

  /* Parse command line. */
  while((n = getopt_long(argc, argv, "hVc:d:f:n:q:r:s", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-gstdecode");
    case 'c': configfile = optarg; break;
    case 'd': dither = getenum("dither type", optarg, dithers); break;
    case 'f': fallback = getfloat("fallback gain", optarg); break;
    case 'n': shape = getenum("noise-shaping type", optarg, shapes); break;
    case 'q': quality = getint("resample quality", optarg, 0, 10); break;
    case 'r': mode = getenum("ReplayGain mode", optarg, modes); break;
    case 's': flags |= f_stream; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(optind >= argc) disorder_fatal(0, "missing filename");
  file = argv[optind++];
  if(optind < argc) disorder_fatal(0, "excess arguments");
  if(config_read(1, 0)) disorder_fatal(0, "cannot read configuration");

  /* Set up the GStreamer machinery. */
  gst_init(0, 0);
  prepare_pipeline();

  /* Set up the output file. */
  if((e = getenv("DISORDER_RAW_FD")) != 0) {
    if((fp = fdopen(atoi(e), "wb")) == 0) disorder_fatal(errno, "fdopen");
  } else
    fp = stdout;

  /* Let's go. */
  decode();

  /* And now we're done. */
  xfclose(fp);
  return (0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:77
indent-tabs-mode:nil
End:
*/
