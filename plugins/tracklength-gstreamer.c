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

/* Ugh.  It turns out that libxml tries to define a function called
 * `attribute', and it's included by GStreamer for some unimaginable reason.
 * So undefine it here.  We'll want GCC attributes for special effects, but
 * can take care of ourselves.
 */
#undef attribute

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/gstdiscoverer.h>

/* The only application we have for `attribute' is declaring function
 * arguments as being unused, because we have a lot of callback functions
 * which are meant to comply with an externally defined interface.
 */
#ifdef __GNUC__
#  define UNUSED __attribute__((unused))
#endif

#define END ((void *)0)

static GstDiscoverer *disco = 0;

long disorder_tracklength(const char UNUSED *track, const char *path) {
  GError *err = 0;
  gchar *dir = 0, *abs = 0, *uri = 0;
  GstDiscovererInfo *info = 0;
  long length = -1;
  GstClockTime t;

  if (!path) goto end;

  if(!disco) {
    gst_init(0, 0);
    disco = gst_discoverer_new(5*GST_SECOND, &err); if(!disco) goto end;
  }

  if (g_path_is_absolute(path))
    uri = g_filename_to_uri(path, 0, &err);
  else {
    dir = g_get_current_dir();
    abs = g_build_filename(dir, path, END);
    uri = g_filename_to_uri(abs, 0, &err);
  }
  if(!uri) goto end;

  info = gst_discoverer_discover_uri(disco, uri, &err); if(!info) goto end;
  switch(gst_discoverer_info_get_result(info)) {
  case GST_DISCOVERER_OK:
    t = gst_discoverer_info_get_duration(info);
    length = (t + GST_SECOND/2)/GST_SECOND;
    break;
  case GST_DISCOVERER_TIMEOUT:
    disorder_info("discovery timed out probing `%s'", path);
    goto swallow_error;
  case GST_DISCOVERER_MISSING_PLUGINS:
    disorder_info("unrecognized file `%s' (missing plugins?)", path);
    goto swallow_error;
  swallow_error:
    if(err) { g_error_free(err); err = 0; }
    length = 0;
    break;
  default:
    goto end;
  }

end:
  if(err) {
    disorder_error(0, "error probing `%s': %s", path, err->message);
    g_error_free(err);
  }
  if(info) gst_discoverer_info_unref(info);
  g_free(dir); g_free(abs); g_free(uri);
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
