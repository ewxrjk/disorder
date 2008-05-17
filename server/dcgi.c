
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <pcre.h>
#include <assert.h>

#include "client.h"
#include "mem.h"
#include "vector.h"
#include "sink.h"
#include "server-cgi.h"
#include "log.h"
#include "configuration.h"
#include "table.h"
#include "queue.h"
#include "plugin.h"
#include "split.h"
#include "wstat.h"
#include "kvp.h"
#include "syscalls.h"
#include "printf.h"
#include "regsub.h"
#include "defs.h"
#include "trackname.h"
#include "charset.h"
#include "dcgi.h"
#include "url.h"
#include "mime.h"
#include "sendmail.h"
#include "base64.h"

struct entry {
  const char *path;
  const char *sort;
  const char *display;
};

static int compare_entry(const void *a, const void *b) {
  const struct entry *ea = a, *eb = b;

  return compare_tracks(ea->sort, eb->sort,
			ea->display, eb->display,
			ea->path, eb->path);
}

static const char *front_url(void) {
  char *url;
  const char *mgmt;

  /* preserve management interface visibility */
  if((mgmt = cgi_get("mgmt")) && !strcmp(mgmt, "true")) {
    byte_xasprintf(&url, "%s?mgmt=true", config->url);
    return url;
  }
  return config->url;
}

static void redirect(struct sink *output) {
  const char *back;

  back = cgi_get("back");
  cgi_header(output, "Location", back && *back ? back : front_url());
  header_cookie(output);
  cgi_body(output);
}

static void expand_template(dcgi_state *ds, cgi_sink *output,
			    const char *action) {
  cgi_header(output->sink, "Content-Type", "text/html");
  header_cookie(output->sink);
  cgi_body(output->sink);
  expand(output, action, ds);
}

/* actions ********************************************************************/

static void act_prefs_errors(const char *msg,
			     void attribute((unused)) *u) {
  fatal(0, "error splitting parts list: %s", msg);
}

static const char *numbered_arg(const char *argname, int numfile) {
  char *fullname;

  byte_xasprintf(&fullname, "%d_%s", numfile, argname);
  return cgi_get(fullname);
}

static void process_prefs(dcgi_state *ds, int numfile) {
  const char *file, *name, *value, *part, *parts, *current, *context;
  char **partslist;

  if(!(file = numbered_arg("file", numfile)))
    /* The first file doesn't need numbering. */
    if(numfile > 0 || !(file = cgi_get("file")))
      return;
  if((parts = numbered_arg("parts", numfile))
     || (parts = cgi_get("parts"))) {
    /* Default context is display.  Other contexts not actually tested. */
    if(!(context = numbered_arg("context", numfile))) context = "display";
    partslist = split(parts, 0, 0, act_prefs_errors, 0);
    while((part = *partslist++)) {
      if(!(value = numbered_arg(part, numfile)))
	continue;
      /* If it's already right (whether regexps or db) don't change anything,
       * so we don't fill the database up with rubbish. */
      if(disorder_part(ds->g->client, (char **)&current,
		       file, context, part))
	fatal(0, "disorder_part() failed");
      if(!strcmp(current, value))
	continue;
      byte_xasprintf((char **)&name, "trackname_%s_%s", context, part);
      disorder_set(ds->g->client, file, name, value);
    }
    if((value = numbered_arg("random", numfile)))
      disorder_unset(ds->g->client, file, "pick_at_random");
    else
      disorder_set(ds->g->client, file, "pick_at_random", "0");
    if((value = numbered_arg("tags", numfile))) {
      if(!*value)
	disorder_unset(ds->g->client, file, "tags");
      else
	disorder_set(ds->g->client, file, "tags", value);
    }
    if((value = numbered_arg("weight", numfile))) {
      if(!*value || !strcmp(value, "90000"))
	disorder_unset(ds->g->client, file, "weight");
      else
	disorder_set(ds->g->client, file, "weight", value);
    }
  } else if((name = cgi_get("name"))) {
    /* Raw preferences.  Not well supported in the templates at the moment. */
    value = cgi_get("value");
    if(value)
      disorder_set(ds->g->client, file, name, value);
    else
      disorder_unset(ds->g->client, file, name);
  }
}

static void act_prefs(cgi_sink *output, dcgi_state *ds) {
  const char *files;
  int nfiles, numfile;

  if((files = cgi_get("files"))) nfiles = atoi(files);
  else nfiles = 1;
  for(numfile = 0; numfile < nfiles; ++numfile)
    process_prefs(ds, numfile);
  cgi_header(output->sink, "Content-Type", "text/html");
  header_cookie(output->sink);
  cgi_body(output->sink);
  expand(output, "prefs", ds);
}
/* expansions *****************************************************************/

static void exp_label(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void attribute((unused)) *u) {
  cgi_output(output, "%s", cgi_label(args[0]));
}

struct trackinfo_state {
  dcgi_state *ds;
  const struct queue_entry *q;
  long length;
  time_t when;
};

struct result {
  char *track;
  const char *sort;
};

static int compare_result(const void *a, const void *b) {
  const struct result *ra = a, *rb = b;
  int c;

  if(!(c = strcmp(ra->sort, rb->sort)))
    c = strcmp(ra->track, rb->track);
  return c;
}

static void exp_stats(int attribute((unused)) nargs,
		      char attribute((unused)) **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;
  char **v;

  cgi_opentag(output->sink, "pre", "class", "stats", (char *)0);
  if(!disorder_stats(ds->g->client, &v, 0)) {
    while(*v)
      cgi_output(output, "%s\n", *v++);
  }
  cgi_closetag(output->sink, "pre");
}

static char *expandarg(const char *arg, dcgi_state *ds) {
  struct dynstr d;
  cgi_sink output;

  dynstr_init(&d);
  output.quote = 0;
  output.sink = sink_dynstr(&d);
  expandstring(&output, arg, ds);
  dynstr_terminate(&d);
  return d.vec;
}

static void exp_navigate(int attribute((unused)) nargs,
			 char **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  const char *path = expandarg(args[0], ds);
  const char *ptr;
  int dirlen;

  if(*path) {
    memset(&substate, 0, sizeof substate);
    substate.g = ds->g;
    ptr = path + 1;			/* skip root */
    dirlen = 0;
    substate.nav_path = path;
    substate.first = 1;
    while(*ptr) {
      while(*ptr && *ptr != '/')
	++ptr;
      substate.last = !*ptr;
      substate.nav_len = ptr - path;
      substate.nav_dirlen = dirlen;
      expandstring(output, args[1], &substate);
      dirlen = substate.nav_len;
      if(*ptr) ++ptr;
      substate.first = 0;
    }
  }
}

static void exp_fullname(int attribute((unused)) nargs,
			 char attribute((unused)) **args,
			 cgi_sink *output,
			 void *u) {
  dcgi_state *ds = u;
  cgi_output(output, "%.*s", ds->nav_len, ds->nav_path);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
