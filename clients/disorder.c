/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "types.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <pcre.h>

#include "configuration.h"
#include "syscalls.h"
#include "log.h"
#include "queue.h"
#include "client.h"
#include "wstat.h"
#include "table.h"
#include "charset.h"
#include "kvp.h"
#include "split.h"
#include "sink.h"
#include "plugin.h"
#include "mem.h"
#include "defs.h"
#include "authorize.h"
#include "vector.h"

static int auto_reconfigure;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "help-commands", no_argument, 0, 'H' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder [OPTIONS] COMMAND ...\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --help-commands, -H     List commands\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

static void cf_version(disorder_client *c,
		       char attribute((unused)) **argv) {
  char *v;

  if(disorder_version(c, &v)) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(v)));
}

static void print_queue_entry(const struct queue_entry *q) {
  if(q->track) xprintf("track %s\n", nullcheck(utf82mb(q->track)));
  if(q->id) xprintf("  id %s\n", nullcheck(utf82mb(q->id)));
  if(q->submitter) xprintf("  submitted by %s at %s",
			   nullcheck(utf82mb(q->submitter)), ctime(&q->when));
  if(q->played) xprintf("  played at %s", ctime(&q->played));
  if(q->state == playing_started
     || q->state == playing_paused) xprintf("  %lds so far",  q->sofar);
  else if(q->expected) xprintf("  might start at %s", ctime(&q->expected));
  if(q->scratched) xprintf("  scratched by %s\n",
			   nullcheck(utf82mb(q->scratched)));
  else xprintf("  %s\n", playing_states[q->state]);
  if(q->wstat) xprintf("  %s\n", wstat(q->wstat));
}

static void cf_playing(disorder_client *c,
		       char attribute((unused)) **argv) {
  struct queue_entry *q;

  if(disorder_playing(c, &q)) exit(EXIT_FAILURE);
  if(q)
    print_queue_entry(q);
  else
    xprintf("nothing\n");
}

static void cf_play(disorder_client *c, char **argv) {
  while(*argv)
    if(disorder_play(c, *argv++)) exit(EXIT_FAILURE);
}

static void cf_remove(disorder_client *c, char **argv) {
  if(disorder_remove(c, argv[0])) exit(EXIT_FAILURE);
}

static void cf_disable(disorder_client *c,
		       char attribute((unused)) **argv) {
  if(disorder_disable(c)) exit(EXIT_FAILURE);
}

static void cf_enable(disorder_client *c, char attribute((unused)) **argv) {
  if(disorder_enable(c)) exit(EXIT_FAILURE);
}

static void cf_scratch(disorder_client *c,
		       char **argv) {
  if(disorder_scratch(c, argv[0])) exit(EXIT_FAILURE);
}

static void cf_shutdown(disorder_client *c,
			char attribute((unused)) **argv) {
  if(disorder_shutdown(c)) exit(EXIT_FAILURE);
}

static void cf_reconfigure(disorder_client *c,
			   char attribute((unused)) **argv) {
  if(disorder_reconfigure(c)) exit(EXIT_FAILURE);
}

static void cf_rescan(disorder_client *c, char attribute((unused)) **argv) {
  if(disorder_rescan(c)) exit(EXIT_FAILURE);
}

static void cf_somequeue(disorder_client *c,
			 int (*fn)(disorder_client *c,
				   struct queue_entry **qp)) {
  struct queue_entry *q;

  if(fn(c, &q)) exit(EXIT_FAILURE);
  while(q) {
    print_queue_entry(q);
    q = q->next;
  }
}

static void cf_recent(disorder_client *c, char attribute((unused)) **argv) {
  cf_somequeue(c, disorder_recent);
}

static void cf_queue(disorder_client *c, char attribute((unused)) **argv) {
  cf_somequeue(c, disorder_queue);
}

static void cf_quack(disorder_client attribute((unused)) *c,
		     char attribute((unused)) **argv) {
  xprintf("\n"
	  " .------------------.\n"
	  " | Naath is a babe! |\n"
	  " `---------+--------'\n"
	  "            \\\n"
	  "              >0\n"
	  "               (<)'\n"
	  "~~~~~~~~~~~~~~~~~~~~~~\n"
	  "\n");
}

static void cf_somelist(disorder_client *c, char **argv,
			int (*fn)(disorder_client *c,
				  const char *arg, const char *re,
				  char ***vecp, int *nvecp)) {
  char **vec;
  const char *re;

  if(argv[1])
    re = xstrdup(argv[1] + 1);
  else
    re = 0;
  if(fn(c, argv[0], re, &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
    xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static int isarg_regexp(const char *s) {
  return s[0] == '~';
}

static void cf_dirs(disorder_client *c,
		    char **argv) {
  cf_somelist(c, argv, disorder_directories);
}

static void cf_files(disorder_client *c, char **argv) {
  cf_somelist(c, argv, disorder_files);
}

static void cf_allfiles(disorder_client *c, char **argv) {
  cf_somelist(c, argv, disorder_allfiles);
}

static void cf_get(disorder_client *c, char **argv) {
  char *value;

  if(disorder_get(c, argv[0], argv[1], &value)) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(value)));
}

static void cf_length(disorder_client *c, char **argv) {
  long length;

  if(disorder_length(c, argv[0], &length)) exit(EXIT_FAILURE);
  xprintf("%ld\n", length);
}

static void cf_set(disorder_client *c, char **argv) {
  if(disorder_set(c, argv[0], argv[1], argv[2])) exit(EXIT_FAILURE);
}

static void cf_unset(disorder_client *c, char **argv) {
  if(disorder_unset(c, argv[0], argv[1])) exit(EXIT_FAILURE);
}

static void cf_prefs(disorder_client *c, char **argv) {
  struct kvp *k;

  if(disorder_prefs(c, argv[0], &k)) exit(EXIT_FAILURE);
  for(; k; k = k->next)
    xprintf("%s = %s\n",
	    nullcheck(utf82mb(k->name)), nullcheck(utf82mb(k->value)));
}

static void cf_search(disorder_client *c, char **argv) {
  char **results;
  int nresults, n;

  if(disorder_search(c, *argv, &results, &nresults)) exit(EXIT_FAILURE);
  for(n = 0; n < nresults; ++n)
    xprintf("%s\n", nullcheck(utf82mb(results[n])));
}

static void cf_random_disable(disorder_client *c,
			      char attribute((unused)) **argv) {
  if(disorder_random_disable(c)) exit(EXIT_FAILURE);
}

static void cf_random_enable(disorder_client *c,
			     char attribute((unused)) **argv) {
  if(disorder_random_enable(c)) exit(EXIT_FAILURE);
}

static void cf_stats(disorder_client *c,
		     char attribute((unused)) **argv) {
  char **vec;

  if(disorder_stats(c, &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
      xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_get_volume(disorder_client *c,
			  char attribute((unused)) **argv) {
  int l, r;

  if(disorder_get_volume(c, &l, &r)) exit(EXIT_FAILURE);
  xprintf("%d %d\n", l, r);
}

static void cf_set_volume(disorder_client *c,
			  char **argv) {
  if(disorder_set_volume(c, atoi(argv[0]), atoi(argv[1]))) exit(EXIT_FAILURE);
}

static void cf_become(disorder_client *c,
		      char **argv) {
  if(disorder_become(c, argv[0])) exit(EXIT_FAILURE);
}

static void cf_log(disorder_client *c,
		   char attribute((unused)) **argv) {
  if(disorder_log(c, sink_stdio("stdout", stdout))) exit(EXIT_FAILURE);
}

static void cf_move(disorder_client *c,
		   char **argv) {
  long n;
  int e;
  
  if((e = xstrtol(&n, argv[1], 0, 10)))
    fatal(e, "cannot convert '%s'", argv[1]);
  if(n > INT_MAX || n < INT_MIN)
    fatal(e, "%ld out of range", n);
  if(disorder_move(c, argv[0], (int)n)) exit(EXIT_FAILURE);
}

static void cf_part(disorder_client *c,
		    char **argv) {
  char *s;

  if(disorder_part(c, &s, argv[0], argv[1], argv[2])) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(s)));
}

static int isarg_filename(const char *s) {
  return s[0] == '/';
}

static void cf_authorize(disorder_client attribute((unused)) *c,
			 char **argv) {
  if(!authorize(argv[0]))
    auto_reconfigure = 1;
}

static void cf_resolve(disorder_client *c,
		       char **argv) {
  char *track;

  if(disorder_resolve(c, &track, argv[0])) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(track)));
}

static void cf_pause(disorder_client *c,
			char attribute((unused)) **argv) {
  if(disorder_pause(c)) exit(EXIT_FAILURE);
}

static void cf_resume(disorder_client *c,
			char attribute((unused)) **argv) {
  if(disorder_resume(c)) exit(EXIT_FAILURE);
}

static void cf_tags(disorder_client *c,
		     char attribute((unused)) **argv) {
  char **vec;

  if(disorder_tags(c, &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
      xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_get_global(disorder_client *c, char **argv) {
  char *value;

  if(disorder_get_global(c, argv[0], &value)) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(value)));
}

static void cf_set_global(disorder_client *c, char **argv) {
  if(disorder_set_global(c, argv[0], argv[1])) exit(EXIT_FAILURE);
}

static void cf_unset_global(disorder_client *c, char **argv) {
  if(disorder_unset_global(c, argv[0])) exit(EXIT_FAILURE);
}

static const struct command {
  const char *name;
  int min, max;
  void (*fn)(disorder_client *c, char **);
  int (*isarg)(const char *);
  const char *argstr, *desc;
} commands[] = {
  { "allfiles",       1, 2, cf_allfiles, isarg_regexp, "DIR [~REGEXP]",
                      "List all files and directories in DIR" },
  { "authorize",      1, 1, cf_authorize, 0, "USER",
                      "Authorize USER to connect to the server" },
  { "become",         1, 1, cf_become, 0, "USER",
                      "Become user USER" },
  { "dirs",           1, 2, cf_dirs, isarg_regexp, "DIR [~REGEXP]",
                      "List directories in DIR" },
  { "disable",        0, 0, cf_disable, 0, "",
                      "Disable play" },
  { "disable-random", 0, 0, cf_random_disable, 0, "",
                      "Disable random play" },
  { "enable",         0, 0, cf_enable, 0, "",
                      "Enable play" },
  { "enable-random",  0, 0, cf_random_enable, 0, "",
                      "Enable random play" },
  { "files",          1, 2, cf_files, isarg_regexp, "DIR [~REGEXP]",
                      "List files in DIR" },
  { "get",            2, 2, cf_get, 0, "TRACK NAME",
                      "Get a preference value" },
  { "get-global",     1, 1, cf_get_global, 0, "NAME",
                      "Get a global preference value" },
  { "get-volume",     0, 0, cf_get_volume, 0, "",
                      "Get the current volume" },
  { "length",         1, 1, cf_length, 0, "TRACK",
                      "Get the length of TRACK in seconds" },
  { "log",            0, 0, cf_log, 0, "",
                      "Copy event log to stdout" },
  { "move",           2, 2, cf_move, 0, "TRACK DELTA",
                      "Move a track in the queue" },
  { "part",           3, 3, cf_part, 0, "TRACK CONTEXT PART",
                      "Find a track name part" },
  { "pause",          0, 0, cf_pause, 0, "",
                      "Pause the currently playing track" },
  { "play",           1, INT_MAX, cf_play, isarg_filename, "TRACKS...",
                      "Add TRACKS to the end of the queue" },
  { "playing",        0, 0, cf_playing, 0, "",
                      "Report the playing track" },
  { "prefs",          1, 1, cf_prefs, 0, "TRACK",
                      "Display all the preferences for TRACK" },
  { "quack",          0, 0, cf_quack, 0, 0, 0 },
  { "queue",          0, 0, cf_queue, 0, "",
                      "Display the current queue" },
  { "random-disable", 0, 0, cf_random_disable, 0, "",
                      "Disable random play" },
  { "random-enable",  0, 0, cf_random_enable, 0, "",
                      "Enable random play" },
  { "recent",         0, 0, cf_recent, 0, "",
                      "Display recently played track" },
  { "reconfigure",    0, 0, cf_reconfigure, 0, "",
                      "Reconfigure the daemon" },
  { "remove",         1, 1, cf_remove, 0, "TRACK",
                      "Remove a track from the queue" },
  { "rescan",         0, 0, cf_rescan, 0, "",
                      "Rescan for new tracks" },
  { "resolve",        1, 1, cf_resolve, 0, "TRACK",
                      "Resolve alias for TRACK" },
  { "resume",         0, 0, cf_resume, 0, "",
                      "Resume after a pause" },
  { "scratch",        0, 0, cf_scratch, 0, "",
                      "Scratch the currently playing track" },
  { "scratch-id",     1, 1, cf_scratch, 0, "ID",
                      "Scratch the currently playing track" },
  { "search",         1, 1, cf_search, 0, "WORDS",
                      "Display tracks matching all the words" },
  { "set",            3, 3, cf_set, 0, "TRACK NAME VALUE",
                      "Set a preference value" },
  { "set-global",     2, 2, cf_set_global, 0, "NAME VALUE",
                      "Set a global preference value" },
  { "set-volume",     2, 2, cf_set_volume, 0, "LEFT RIGHT",
                      "Set the volume" },
  { "shutdown",       0, 0, cf_shutdown, 0, "",
                      "Shut down the daemon" },
  { "stats",          0, 0, cf_stats, 0, "",
                      "Display server statistics" },
  { "tags",           0, 0, cf_tags, 0, "",
                      "List known tags" },
  { "unset",          2, 2, cf_unset, 0, "TRACK NAME",
                      "Unset a preference" },
  { "unset-global",   1, 1, cf_unset_global, 0, "NAME",
                      "Unset a global preference" },
  { "version",        0, 0, cf_version, 0, "",
                      "Display the server version" },
};

static void help_commands(void) {
  unsigned n, max = 0, l;

  xprintf("Command summary:\n");
  for(n = 0; n < sizeof commands / sizeof *commands; ++n) {
    if(!commands[n].desc) continue;
    l = strlen(commands[n].name);
    if(*commands[n].argstr)
      l += strlen(commands[n].argstr) + 1;
    if(l > max)
      max = l;
  }
  for(n = 0; n < sizeof commands / sizeof *commands; ++n) {
    if(!commands[n].desc) continue;
    l = strlen(commands[n].name);
    if(*commands[n].argstr)
      l += strlen(commands[n].argstr) + 1;
    xprintf("  %s%s%s%*s  %s\n", commands[n].name,
	    *commands[n].argstr ? " " : "",
	    commands[n].argstr,
	    max - l, "",
	    commands[n].desc);
  }
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n, i, j;
  disorder_client *c = 0;
  const char *s;
  int status = 0;
  struct vector args;

  mem_init();
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dHL", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'H': help_commands();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  if(config_read()) fatal(0, "cannot read configuration");
  if(!(c = disorder_new(1))) exit(EXIT_FAILURE);
  s = config_get_file("socket");
  if(disorder_connect(c)) exit(EXIT_FAILURE);
  n = optind;
  /* accumulate command args */
  while(n < argc) {
    if((i = TABLE_FIND(commands, struct command, name, argv[n])) < 0)
      fatal(0, "unknown command '%s'", argv[n]);
    if(n + commands[i].min >= argc)
      fatal(0, "missing arguments to '%s'", argv[n]);
    n++;
    vector_init(&args);
    for(j = 0; j < commands[i].min; ++j)
      vector_append(&args, nullcheck(mb2utf8(argv[n + j])));
    for(; j < commands[i].max
	  && n + j < argc
	  && commands[i].isarg(argv[n + j]); ++j)
      vector_append(&args, nullcheck(mb2utf8(argv[n + j])));
    vector_terminate(&args);
    commands[i].fn(c, args.vec);
    n += j;
  }
  if(auto_reconfigure) {
    assert(c != 0);
    if(disorder_reconfigure(c)) exit(EXIT_FAILURE);
  }
  if(c && disorder_close(c)) exit(EXIT_FAILURE);
  if(fclose(stdout) < 0) fatal(errno, "error closing stdout");
  return status;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
