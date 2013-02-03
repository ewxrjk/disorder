/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2009, 2011 Richard Kettlewell
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
/** @file server/plugin.c
 * @brief Server plugin interface
 */
#include "disorder-server.h"

#include <dlfcn.h>

/* generic plugin support *****************************************************/

#ifndef SOSUFFIX
# define SOSUFFIX ".so"
#endif

/** @brief A loaded plugin */
struct plugin {
  /** @brief Next plugin */
  struct plugin *next;

  /** @brief Handle returned from dlopen() */
  void *dlhandle;

  /** @brief Plugin name */
  const char *name;
};

static struct plugin *plugins;

const struct plugin *open_plugin(const char *name,
				 unsigned flags) {
  void *h = 0;
  char *p;
  int n;
  struct plugin *pl;

  for(pl = plugins; pl && strcmp(pl->name, name); pl = pl->next)
    ;
  if(pl) return pl;
  /* Search the plugin path */
  for(n = 0; n <= config->plugins.n; ++n) {
    byte_xasprintf(&p, "%s/%s" SOSUFFIX,
		   n == config->plugins.n ? pkglibdir : config->plugins.s[n],
		   name);
    if(access(p, R_OK) == 0) {
      h = dlopen(p, RTLD_NOW);
      if(!h) {
	disorder_error(0, "error opening %s: %s", p, dlerror());
	continue;
      }
      pl = xmalloc(sizeof *pl);
      pl->dlhandle = h;
      pl->name = xstrdup(name);
      pl->next = plugins;
      plugins = pl;
      return pl;
    }
  }
  (flags & PLUGIN_FATAL ? disorder_fatal : disorder_error)
    (0, "cannot find plugin '%s'", name);
  return 0;
}

function_t *get_plugin_function(const struct plugin *pl,
				const char *symbol) {
  function_t *f;

  f = (function_t *)dlsym(pl->dlhandle, symbol);
  if(!f)
    disorder_fatal(0, "error looking up function '%s' in '%s': %s",
		   symbol, pl->name, dlerror());
  return f;
}

const void *get_plugin_object(const struct plugin *pl,
			      const char *symbol) {
  void *o;

  o = dlsym(pl->dlhandle, symbol);
  if(!o)
    disorder_fatal(0, "error looking up object '%s' in '%s': %s",
		   symbol, pl->name, dlerror());
  return o;
}

/* specific plugin interfaces *************************************************/

typedef long tracklength_fn(const char *track, const char *path);

/** Compute the length of a track
 * @param plugin plugin to use, as configured
 * @param track UTF-8 name of track
 * @param path file system path or 0
 * @return length of track in seconds, 0 for unknown, -1 for error
 */
long tracklength(const char *plugin, const char *track, const char *path) {
  tracklength_fn *f = 0;

  f = (tracklength_fn *)get_plugin_function(open_plugin(plugin,
							PLUGIN_FATAL),
					    "disorder_tracklength");
  return (*f)(track, path);
}

typedef void scan_fn(const char *root);

void scan(const char *module, const char *root) {
  ((scan_fn *)get_plugin_function(open_plugin(module, PLUGIN_FATAL),
				  "disorder_scan"))(root);
}

typedef int check_fn(const char *root, const char *path);


int check(const char *module, const char *root, const char *path) {
  return ((check_fn *)get_plugin_function(open_plugin(module, PLUGIN_FATAL),
					  "disorder_check"))(root, path);
}

typedef void notify_play_fn(const char *track, const char *submitter);

void notify_play(const char *track,
		 const char *submitter) {
  static notify_play_fn *f;

  if(!f)
    f = (notify_play_fn *)get_plugin_function(open_plugin("notify",
							  PLUGIN_FATAL),
					      "disorder_notify_play");
  (*f)(track, submitter);
}

typedef void notify_scratch_fn(const char *track,
			       const char *submitter,
			       const char *scratcher,
			       int seconds);

void notify_scratch(const char *track,
		    const char *submitter,
		    const char *scratcher,
		    int seconds) {
  static notify_scratch_fn *f;

  if(!f)
    f = (notify_scratch_fn *)get_plugin_function(open_plugin("notify",
							     PLUGIN_FATAL),
						 "disorder_notify_scratch");
  (*f)(track, submitter, scratcher, seconds);
}

typedef void notify_not_scratched_fn(const char *track,
				     const char *submitter);

void notify_not_scratched(const char *track,
			  const char *submitter) {
  static notify_not_scratched_fn *f;

  if(!f)
    f = (notify_not_scratched_fn *)get_plugin_function
      (open_plugin("notify",
		   PLUGIN_FATAL),
       "disorder_notify_not_scratched");
  (*f)(track, submitter);
}

typedef void notify_queue_fn(const char *track,
			     const char *submitter);

void notify_queue(const char *track,
		  const char *submitter) {
  static notify_queue_fn *f;

  if(!f)
    f = (notify_queue_fn *)get_plugin_function(open_plugin("notify",
							   PLUGIN_FATAL),
					       "disorder_notify_queue");
  (*f)(track, submitter);
}

void notify_queue_remove(const char *track,
			 const char *remover) {
  static notify_queue_fn *f;
  
  if(!f)
    f = (notify_queue_fn *)get_plugin_function(open_plugin("notify",
							   PLUGIN_FATAL),
					       "disorder_notify_queue_remove");
  (*f)(track, remover);
}

void notify_queue_move(const char *track,
		       const char *mover) {
  static notify_queue_fn *f;
  
  if(!f)
    f = (notify_queue_fn *)get_plugin_function(open_plugin("notify",
							   PLUGIN_FATAL),
					       "disorder_notify_queue_move");
  (*f)(track, mover);
}

void notify_pause(const char *track, const char *who) {
  static notify_queue_fn *f;
  
  if(!f)
    f = (notify_queue_fn *)get_plugin_function(open_plugin("notify",
							   PLUGIN_FATAL),
					       "disorder_notify_pause");
  (*f)(track, who);
}

void notify_resume(const char *track, const char *who) {
  static notify_queue_fn *f;
  
  if(!f)
    f = (notify_queue_fn *)get_plugin_function(open_plugin("notify",
							   PLUGIN_FATAL),
					       "disorder_notify_resume");
  (*f)(track, who);
}

/* player plugin interfaces ***************************************************/

/* get type */

unsigned long play_get_type(const struct plugin *pl) {
  return *(const unsigned long *)get_plugin_object(pl, "disorder_player_type");
}

/* prefork */

typedef void *prefork_fn(const char *track);

void *play_prefork(const struct plugin *pl,
		   const char *track) {
  return ((prefork_fn *)get_plugin_function(pl,
					    "disorder_play_prefork"))(track);
}

/* play */

typedef void play_track_fn(const char *const *parameters,
			   int nparameters,
			   const char *path,
			   const char *track);

void play_track(const struct plugin *pl,
		const char *const *parameters,
		int nparameters,
		const char *path,
		const char *track) {
  ((play_track_fn *)get_plugin_function(pl,
					"disorder_play_track"))(parameters,
								nparameters,
								path,
								track);
}

/* cleanup */

typedef void cleanup_fn(void *data);

void play_cleanup(const struct plugin *pl, void *data) {
  ((cleanup_fn *)get_plugin_function(pl, "disorder_play_cleanup"))(data);
}

/* pause */

typedef int pause_fn(long *playedp, void *data);

int play_pause(const struct plugin *pl, long *playedp, void *data) {
  return (((pause_fn *)get_plugin_function(pl, "disorder_pause_track"))
	  (playedp, data));
}

/* resume */

typedef void resume_fn(void *data);

void play_resume(const struct plugin *pl, void *data) {
  (((resume_fn *)get_plugin_function(pl, "disorder_resume_track"))
   (data));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
