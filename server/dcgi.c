
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

static void act_disable(cgi_sink *output,
			dcgi_state *ds) {
  if(ds->g->client)
    disorder_disable(ds->g->client);
  redirect(output->sink);
}

static void act_enable(cgi_sink *output,
			      dcgi_state *ds) {
  if(ds->g->client)
    disorder_enable(ds->g->client);
  redirect(output->sink);
}

static void act_random_disable(cgi_sink *output,
			       dcgi_state *ds) {
  if(ds->g->client)
    disorder_random_disable(ds->g->client);
  redirect(output->sink);
}

static void act_random_enable(cgi_sink *output,
			      dcgi_state *ds) {
  if(ds->g->client)
    disorder_random_enable(ds->g->client);
  redirect(output->sink);
}

static void act_remove(cgi_sink *output,
		       dcgi_state *ds) {
  const char *id;

  if(!(id = cgi_get("id"))) fatal(0, "missing id argument");
  if(ds->g->client)
    disorder_remove(ds->g->client, id);
  redirect(output->sink);
}

static void act_move(cgi_sink *output,
		     dcgi_state *ds) {
  const char *id, *delta;

  if(!(id = cgi_get("id"))) fatal(0, "missing id argument");
  if(!(delta = cgi_get("delta"))) fatal(0, "missing delta argument");
  if(ds->g->client)
    disorder_move(ds->g->client, id, atoi(delta));
  redirect(output->sink);
}

static void act_scratch(cgi_sink *output,
			dcgi_state *ds) {
  if(ds->g->client)
    disorder_scratch(ds->g->client, cgi_get("id"));
  redirect(output->sink);
}

static void act_play(cgi_sink *output,
		     dcgi_state *ds) {
  const char *track, *dir;
  char **tracks;
  int ntracks, n;
  struct entry *e;

  if((track = cgi_get("file"))) {
    disorder_play(ds->g->client, track);
  } else if((dir = cgi_get("directory"))) {
    if(disorder_files(ds->g->client, dir, 0, &tracks, &ntracks)) ntracks = 0;
    if(ntracks) {
      e = xmalloc(ntracks * sizeof (struct entry));
      for(n = 0; n < ntracks; ++n) {
	e[n].path = tracks[n];
	e[n].sort = trackname_transform("track", tracks[n], "sort");
	e[n].display = trackname_transform("track", tracks[n], "display");
      }
      qsort(e, ntracks, sizeof (struct entry), compare_entry);
      for(n = 0; n < ntracks; ++n)
	disorder_play(ds->g->client, e[n].path);
    }
  }
  /* XXX error handling */
  redirect(output->sink);
}

static int clamp(int n, int min, int max) {
  if(n < min)
    return min;
  if(n > max)
    return max;
  return n;
}

static const char *volume_url(void) {
  char *url;
  
  byte_xasprintf(&url, "%s?action=volume", config->url);
  return url;
}

static void act_volume(cgi_sink *output, dcgi_state *ds) {
  const char *l, *r, *d, *back;
  int nd, changed = 0;;

  if((d = cgi_get("delta"))) {
    lookups(ds, DC_VOLUME);
    nd = clamp(atoi(d), -255, 255);
    disorder_set_volume(ds->g->client,
			clamp(ds->g->volume_left + nd, 0, 255),
			clamp(ds->g->volume_right + nd, 0, 255));
    changed = 1;
  } else if((l = cgi_get("left")) && (r = cgi_get("right"))) {
    disorder_set_volume(ds->g->client, atoi(l), atoi(r));
    changed = 1;
  }
  if(changed) {
    /* redirect back to ourselves (but without the volume-changing bits in the
     * URL) */
    cgi_header(output->sink, "Location",
	       (back = cgi_get("back")) ? back : volume_url());
    header_cookie(output->sink);
    cgi_body(output->sink);
  } else {
    cgi_header(output->sink, "Content-Type", "text/html");
    header_cookie(output->sink);
    cgi_body(output->sink);
    expand(output, "volume", ds);
  }
}

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

static void act_pause(cgi_sink *output,
		      dcgi_state *ds) {
  if(ds->g->client)
    disorder_pause(ds->g->client);
  redirect(output->sink);
}

static void act_resume(cgi_sink *output,
		       dcgi_state *ds) {
  if(ds->g->client)
    disorder_resume(ds->g->client);
  redirect(output->sink);
}

static void act_login(cgi_sink *output,
		      dcgi_state *ds) {
  const char *username, *password, *back;
  disorder_client *c;

  username = cgi_get("username");
  password = cgi_get("password");
  if(!username || !password
     || !strcmp(username, "guest")/*bodge to avoid guest cookies*/) {
    /* We're just visiting the login page */
    expand_template(ds, output, "login");
    return;
  }
  /* We'll need a new connection as we are going to stop being guest */
  c = disorder_new(0);
  if(disorder_connect_user(c, username, password)) {
    cgi_set_option("error", "loginfailed");
    expand_template(ds, output, "login");
    return;
  }
  if(disorder_make_cookie(c, &login_cookie)) {
    cgi_set_option("error", "cookiefailed");
    expand_template(ds, output, "login");
    return;
  }
  /* Use the new connection henceforth */
  ds->g->client = c;
  ds->g->flags = 0;
  /* We have a new cookie */
  header_cookie(output->sink);
  cgi_set_option("status", "loginok");
  if((back = cgi_get("back")) && *back)
    /* Redirect back to somewhere or other */
    redirect(output->sink);
  else
    /* Stick to the login page */
    expand_template(ds, output, "login");
}

static void act_logout(cgi_sink *output,
		       dcgi_state *ds) {
  disorder_revoke(ds->g->client);
  login_cookie = 0;
  /* Reconnect as guest */
  disorder_cgi_login(ds, output);
  /* Back to the login page */
  cgi_set_option("status", "logoutok");
  expand_template(ds, output, "login");
}

static void act_register(cgi_sink *output,
			 dcgi_state *ds) {
  const char *username, *password, *password2, *email;
  char *confirm, *content_type;
  const char *text, *encoding, *charset;

  username = cgi_get("username");
  password = cgi_get("password1");
  password2 = cgi_get("password2");
  email = cgi_get("email");

  if(!username || !*username) {
    cgi_set_option("error", "nousername");
    expand_template(ds, output, "login");
    return;
  }
  if(!password || !*password) {
    cgi_set_option("error", "nopassword");
    expand_template(ds, output, "login");
    return;
  }
  if(!password2 || !*password2 || strcmp(password, password2)) {
    cgi_set_option("error", "passwordmismatch");
    expand_template(ds, output, "login");
    return;
  }
  if(!email || !*email) {
    cgi_set_option("error", "noemail");
    expand_template(ds, output, "login");
    return;
  }
  /* We could well do better address validation but for now we'll just do the
   * minimum */
  if(!strchr(email, '@')) {
    cgi_set_option("error", "bademail");
    expand_template(ds, output, "login");
    return;
  }
  if(disorder_register(ds->g->client, username, password, email, &confirm)) {
    cgi_set_option("error", "cannotregister");
    expand_template(ds, output, "login");
    return;
  }
  /* Send the user a mail */
  /* TODO templatize this */
  byte_xasprintf((char **)&text,
		 "Welcome to DisOrder.  To active your login, please visit this URL:\n"
		 "\n"
		 "%s?c=%s\n", config->url, urlencodestring(confirm));
  if(!(text = mime_encode_text(text, &charset, &encoding)))
    fatal(0, "cannot encode email");
  byte_xasprintf(&content_type, "text/plain;charset=%s",
		 quote822(charset, 0));
  sendmail("", config->mail_sender, email, "Welcome to DisOrder",
	   encoding, content_type, text); /* TODO error checking  */
  /* We'll go back to the login page with a suitable message */
  cgi_set_option("status", "registered");
  expand_template(ds, output, "login");
}

static void act_confirm(cgi_sink *output,
			dcgi_state *ds) {
  const char *confirmation;

  if(!(confirmation = cgi_get("c"))) {
    cgi_set_option("error", "noconfirm");
    expand_template(ds, output, "login");
  }
  /* Confirm our registration */
  if(disorder_confirm(ds->g->client, confirmation)) {
    cgi_set_option("error", "badconfirm");
    expand_template(ds, output, "login");
  }
  /* Get a cookie */
  if(disorder_make_cookie(ds->g->client, &login_cookie)) {
    cgi_set_option("error", "cookiefailed");
    expand_template(ds, output, "login");
    return;
  }
  /* Discard any cached data JIC */
  ds->g->flags = 0;
  /* We have a new cookie */
  header_cookie(output->sink);
  cgi_set_option("status", "confirmed");
  expand_template(ds, output, "login");
}

static void act_edituser(cgi_sink *output,
			 dcgi_state *ds) {
  const char *email = cgi_get("email"), *password = cgi_get("changepassword1");
  const char *password2 = cgi_get("changepassword2");
  int newpassword = 0;
  disorder_client *c;

  if((password && *password) || (password && *password2)) {
    if(!password || !password2 || strcmp(password, password2)) {
      cgi_set_option("error", "passwordmismatch");
      expand_template(ds, output, "login");
      return;
    }
  } else
    password = password2 = 0;
  
  if(email) {
    if(disorder_edituser(ds->g->client, disorder_user(ds->g->client),
			 "email", email)) {
      cgi_set_option("error", "badedit");
      expand_template(ds, output, "login");
      return;
    }
  }
  if(password) {
    if(disorder_edituser(ds->g->client, disorder_user(ds->g->client),
			 "password", password)) {
      cgi_set_option("error", "badedit");
      expand_template(ds, output, "login");
      return;
    }
    newpassword = 1;
  }
  if(newpassword) {
    login_cookie = 0;			/* it'll be invalid now */
    /* This is a bit duplicative of act_login() */
    c = disorder_new(0);
    if(disorder_connect_user(c, disorder_user(ds->g->client), password)) {
      cgi_set_option("error", "loginfailed");
      expand_template(ds, output, "login");
      return;
    }
    if(disorder_make_cookie(c, &login_cookie)) {
      cgi_set_option("error", "cookiefailed");
      expand_template(ds, output, "login");
      return;
    }
    /* Use the new connection henceforth */
    ds->g->client = c;
    ds->g->flags = 0;
    /* We have a new cookie */
    header_cookie(output->sink);
  }
  cgi_set_option("status", "edited");
  expand_template(ds, output, "login");  
}

static void act_reminder(cgi_sink *output,
			 dcgi_state *ds) {
  const char *const username = cgi_get("username");

  if(!username || !*username) {
    cgi_set_option("error", "nousername");
    expand_template(ds, output, "login");
    return;
  }
  if(disorder_reminder(ds->g->client, username)) {
    cgi_set_option("error", "reminderfailed");
    expand_template(ds, output, "login");
    return;
  }
  cgi_set_option("status", "reminded");
  expand_template(ds, output, "login");  
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

static void exp_search(int nargs,
		       char **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u, substate;
  char **tracks;
  const char *q, *context, *part, *template;
  int ntracks, n, m;
  struct result *r;

  switch(nargs) {
  case 2:
    part = args[0];
    context = "sort";
    template = args[1];
    break;
  case 3:
    part = args[0];
    context = args[1];
    template = args[2];
    break;
  default:
    assert(!"should never happen");
    part = context = template = 0;	/* quieten compiler */
  }
  if(ds->tracks == 0) {
    /* we are the top level, let's get some search results */
    if(!(q = cgi_get("query"))) return;	/* no results yet */
    if(disorder_search(ds->g->client, q, &tracks, &ntracks)) return;
    if(!ntracks) return;
  } else {
    tracks = ds->tracks;
    ntracks = ds->ntracks;
  }
  assert(ntracks != 0);
  /* sort tracks by the appropriate part */
  r = xmalloc(ntracks * sizeof *r);
  for(n = 0; n < ntracks; ++n) {
    r[n].track = tracks[n];
    if(disorder_part(ds->g->client, (char **)&r[n].sort,
		     tracks[n], context, part))
      fatal(0, "disorder_part() failed");
  }
  qsort(r, ntracks, sizeof (struct result), compare_result);
  /* expand the 2nd arg once for each group.  We re-use the passed-in tracks
   * array as we know it's guaranteed to be big enough and isn't going to be
   * used for anything else any more. */
  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  substate.first = 1;
  n = 0;
  while(n < ntracks) {
    substate.tracks = tracks;
    substate.ntracks = 0;
    m = n;
    while(m < ntracks
	  && !strcmp(r[m].sort, r[n].sort))
      tracks[substate.ntracks++] = r[m++].track;
    substate.last = (m == ntracks);
    expandstring(output, template, &substate);
    substate.index++;
    substate.first = 0;
    n = m;
  }
  assert(substate.last != 0);
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

static void exp_isfiles(int attribute((unused)) nargs,
			char attribute((unused)) **args,
			cgi_sink *output,
			void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_FILES);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->nfiles));
}

static void exp_isdirectories(int attribute((unused)) nargs,
			      char attribute((unused)) **args,
			      cgi_sink *output,
			      void *u) {
  dcgi_state *ds = u;

  lookups(ds, DC_DIRS);
  sink_printf(output->sink, "%s", bool2str(!!ds->g->ndirs));
}

static void exp_choose(int attribute((unused)) nargs,
		       char **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  int nfiles, n;
  char **files;
  struct entry *e;
  const char *type, *what = expandarg(args[0], ds);

  if(!strcmp(what, "files")) {
    lookups(ds, DC_FILES);
    files = ds->g->files;
    nfiles = ds->g->nfiles;
    type = "track";
  } else if(!strcmp(what, "directories")) {
    lookups(ds, DC_DIRS);
    files = ds->g->dirs;
    nfiles = ds->g->ndirs;
    type = "dir";
  } else {
    error(0, "unknown @choose@ argument '%s'", what);
    return;
  }
  e = xmalloc(nfiles * sizeof (struct entry));
  for(n = 0; n < nfiles; ++n) {
    e[n].path = files[n];
    e[n].sort = trackname_transform(type, files[n], "sort");
    e[n].display = trackname_transform(type, files[n], "display");
  }
  qsort(e, nfiles, sizeof (struct entry), compare_entry);
  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  substate.first = 1;
  for(n = 0; n < nfiles; ++n) {
    substate.last = (n == nfiles - 1);
    substate.index = n;
    substate.entry = &e[n];
    expandstring(output, args[1], &substate);
    substate.first = 0;
  }
}

static void exp_file(int attribute((unused)) nargs,
		     char attribute((unused)) **args,
		     cgi_sink *output,
		     void *u) {
  dcgi_state *ds = u;

  if(ds->entry)
    cgi_output(output, "%s", ds->entry->path);
  else if(ds->track)
    cgi_output(output, "%s", ds->track->track);
  else if(ds->tracks)
    cgi_output(output, "%s", ds->tracks[0]);
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

static void exp_files(int attribute((unused)) nargs,
		      char **args,
		      cgi_sink *output,
		      void *u) {
  dcgi_state *ds = u;
  dcgi_state substate;
  const char *nfiles_arg, *directory;
  int nfiles, numfile;
  struct kvp *k;

  memset(&substate, 0, sizeof substate);
  substate.g = ds->g;
  if((directory = cgi_get("directory"))) {
    /* Prefs for whole directory. */
    lookups(ds, DC_FILES);
    /* Synthesize args for the file list. */
    nfiles = ds->g->nfiles;
    for(numfile = 0; numfile < nfiles; ++numfile) {
      k = xmalloc(sizeof *k);
      byte_xasprintf((char **)&k->name, "%d_file", numfile);
      k->value = ds->g->files[numfile];
      k->next = cgi_args;
      cgi_args = k;
    }
  } else {
    /* Args already present. */
    if((nfiles_arg = cgi_get("files"))) nfiles = atoi(nfiles_arg);
    else nfiles = 1;
  }
  for(numfile = 0; numfile < nfiles; ++numfile) {
    substate.index = numfile;
    expandstring(output, args[0], &substate);
  }
}

static void exp_nfiles(int attribute((unused)) nargs,
		       char attribute((unused)) **args,
		       cgi_sink *output,
		       void *u) {
  dcgi_state *ds = u;
  const char *files_arg;

  if(cgi_get("directory")) {
    lookups(ds, DC_FILES);
    cgi_output(output, "%d", ds->g->nfiles);
  } else if((files_arg = cgi_get("files")))
    cgi_output(output, "%s", files_arg);
  else
    cgi_output(output, "1");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
