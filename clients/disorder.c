/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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
#include <ctype.h>

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
#include "version.h"

static disorder_client *client;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "local", no_argument, 0, 'l' },
  { "no-per-user-config", no_argument, 0, 'N' },
  { "help-commands", no_argument, 0, 'H' },
  { "user", required_argument, 0, 'u' },
  { "password", required_argument, 0, 'p' },
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
	  "  --local, -l             Force connection to local server\n"
	  "  --debug, -d             Turn on debugging\n");
  xfclose(stdout);
  exit(0);
}

static disorder_client *getclient(void) {
  if(!client) {
    if(!(client = disorder_new(1))) exit(EXIT_FAILURE);
    if(disorder_connect(client)) exit(EXIT_FAILURE);
  }
  return client;
}

static void cf_version(char attribute((unused)) **argv) {
  char *v;

  if(disorder_version(getclient(), &v)) exit(EXIT_FAILURE);
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

static void cf_playing(char attribute((unused)) **argv) {
  struct queue_entry *q;

  if(disorder_playing(getclient(), &q)) exit(EXIT_FAILURE);
  if(q)
    print_queue_entry(q);
  else
    xprintf("nothing\n");
}

static void cf_play(char **argv) {
  while(*argv)
    if(disorder_play(getclient(), *argv++)) exit(EXIT_FAILURE);
}

static void cf_remove(char **argv) {
  if(disorder_remove(getclient(), argv[0])) exit(EXIT_FAILURE);
}

static void cf_disable(char attribute((unused)) **argv) {
  if(disorder_disable(getclient())) exit(EXIT_FAILURE);
}

static void cf_enable(char attribute((unused)) **argv) {
  if(disorder_enable(getclient())) exit(EXIT_FAILURE);
}

static void cf_scratch(char **argv) {
  if(disorder_scratch(getclient(), argv[0])) exit(EXIT_FAILURE);
}

static void cf_shutdown(char attribute((unused)) **argv) {
  if(disorder_shutdown(getclient())) exit(EXIT_FAILURE);
}

static void cf_reconfigure(char attribute((unused)) **argv) {
  /* Re-check configuration for server */
  if(config_read(1)) fatal(0, "cannot read configuration");
  if(disorder_reconfigure(getclient())) exit(EXIT_FAILURE);
}

static void cf_rescan(char attribute((unused)) **argv) {
  if(disorder_rescan(getclient())) exit(EXIT_FAILURE);
}

static void cf_somequeue(int (*fn)(disorder_client *c,
				   struct queue_entry **qp)) {
  struct queue_entry *q;

  if(fn(getclient(), &q)) exit(EXIT_FAILURE);
  while(q) {
    print_queue_entry(q);
    q = q->next;
  }
}

static void cf_recent(char attribute((unused)) **argv) {
  cf_somequeue(disorder_recent);
}

static void cf_queue(char attribute((unused)) **argv) {
  cf_somequeue(disorder_queue);
}

static void cf_quack(char attribute((unused)) **argv) {
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

static void cf_somelist(char **argv,
			int (*fn)(disorder_client *c,
				  const char *arg, const char *re,
				  char ***vecp, int *nvecp)) {
  char **vec;
  const char *re;

  if(argv[1])
    re = xstrdup(argv[1] + 1);
  else
    re = 0;
  if(fn(getclient(), argv[0], re, &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
    xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static int isarg_regexp(const char *s) {
  return s[0] == '~';
}

static void cf_dirs(char **argv) {
  cf_somelist(argv, disorder_directories);
}

static void cf_files(char **argv) {
  cf_somelist(argv, disorder_files);
}

static void cf_allfiles(char **argv) {
  cf_somelist(argv, disorder_allfiles);
}

static void cf_get(char **argv) {
  char *value;

  if(disorder_get(getclient(), argv[0], argv[1], &value)) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(value)));
}

static void cf_length(char **argv) {
  long length;

  if(disorder_length(getclient(), argv[0], &length)) exit(EXIT_FAILURE);
  xprintf("%ld\n", length);
}

static void cf_set(char **argv) {
  if(disorder_set(getclient(), argv[0], argv[1], argv[2])) exit(EXIT_FAILURE);
}

static void cf_unset(char **argv) {
  if(disorder_unset(getclient(), argv[0], argv[1])) exit(EXIT_FAILURE);
}

static void cf_prefs(char **argv) {
  struct kvp *k;

  if(disorder_prefs(getclient(), argv[0], &k)) exit(EXIT_FAILURE);
  for(; k; k = k->next)
    xprintf("%s = %s\n",
	    nullcheck(utf82mb(k->name)), nullcheck(utf82mb(k->value)));
}

static void cf_search(char **argv) {
  char **results;
  int nresults, n;

  if(disorder_search(getclient(), *argv, &results, &nresults)) exit(EXIT_FAILURE);
  for(n = 0; n < nresults; ++n)
    xprintf("%s\n", nullcheck(utf82mb(results[n])));
}

static void cf_random_disable(char attribute((unused)) **argv) {
  if(disorder_random_disable(getclient())) exit(EXIT_FAILURE);
}

static void cf_random_enable(char attribute((unused)) **argv) {
  if(disorder_random_enable(getclient())) exit(EXIT_FAILURE);
}

static void cf_stats(char attribute((unused)) **argv) {
  char **vec;

  if(disorder_stats(getclient(), &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
      xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_get_volume(char attribute((unused)) **argv) {
  int l, r;

  if(disorder_get_volume(getclient(), &l, &r)) exit(EXIT_FAILURE);
  xprintf("%d %d\n", l, r);
}

static void cf_set_volume(char **argv) {
  if(disorder_set_volume(getclient(), atoi(argv[0]), atoi(argv[1]))) exit(EXIT_FAILURE);
}

static void cf_log(char attribute((unused)) **argv) {
  if(disorder_log(getclient(), sink_stdio("stdout", stdout))) exit(EXIT_FAILURE);
}

static void cf_move(char **argv) {
  long n;
  int e;
  
  if((e = xstrtol(&n, argv[1], 0, 10)))
    fatal(e, "cannot convert '%s'", argv[1]);
  if(n > INT_MAX || n < INT_MIN)
    fatal(e, "%ld out of range", n);
  if(disorder_move(getclient(), argv[0], (int)n)) exit(EXIT_FAILURE);
}

static void cf_part(char **argv) {
  char *s;

  if(disorder_part(getclient(), &s, argv[0], argv[1], argv[2])) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(s)));
}

static int isarg_filename(const char *s) {
  return s[0] == '/';
}

static void cf_authorize(char **argv) {
  authorize(getclient(), argv[0], argv[1]);
}

static void cf_resolve(char **argv) {
  char *track;

  if(disorder_resolve(getclient(), &track, argv[0])) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(track)));
}

static void cf_pause(char attribute((unused)) **argv) {
  if(disorder_pause(getclient())) exit(EXIT_FAILURE);
}

static void cf_resume(char attribute((unused)) **argv) {
  if(disorder_resume(getclient())) exit(EXIT_FAILURE);
}

static void cf_tags(char attribute((unused)) **argv) {
  char **vec;

  if(disorder_tags(getclient(), &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
      xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_users(char attribute((unused)) **argv) {
  char **vec;

  if(disorder_users(getclient(), &vec, 0)) exit(EXIT_FAILURE);
  while(*vec)
    xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_get_global(char **argv) {
  char *value;

  if(disorder_get_global(getclient(), argv[0], &value)) exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(value)));
}

static void cf_set_global(char **argv) {
  if(disorder_set_global(getclient(), argv[0], argv[1])) exit(EXIT_FAILURE);
}

static void cf_unset_global(char **argv) {
  if(disorder_unset_global(getclient(), argv[0])) exit(EXIT_FAILURE);
}

static int isarg_integer(const char *s) {
  if(!*s) return 0;
  while(*s) {
    if(!isdigit((unsigned char)*s))
      return 0;
    ++s;
  }
  return 1;
}

static void cf_new(char **argv) {
  char **vec;

  if(disorder_new_tracks(getclient(), &vec, 0, argv[0] ? atoi(argv[0]) : 0))
    exit(EXIT_FAILURE);
  while(*vec)
    xprintf("%s\n", nullcheck(utf82mb(*vec++)));
}

static void cf_rtp_address(char attribute((unused)) **argv) {
  char *address, *port;

  if(disorder_rtp_address(getclient(), &address, &port)) exit(EXIT_FAILURE);
  xprintf("address: %s\nport: %s\n", address, port);
}

static int isarg_rights(const char *arg) {
  return strchr(arg, ',') || !parse_rights(arg, 0, 0);
}

static void cf_adduser(char **argv) {
  if(disorder_adduser(getclient(), argv[0], argv[1], argv[2]))
    exit(EXIT_FAILURE);
}

static void cf_deluser(char **argv) {
  if(disorder_deluser(getclient(), argv[0]))
    exit(EXIT_FAILURE);
}

static void cf_edituser(char **argv) {
  if(disorder_edituser(getclient(), argv[0], argv[1], argv[2]))
    exit(EXIT_FAILURE);
}

static void cf_userinfo(char **argv) {
  char *s;

  if(disorder_userinfo(getclient(), argv[0], argv[1], &s))
    exit(EXIT_FAILURE);
  xprintf("%s\n", nullcheck(utf82mb(s)));
}

static int isarg_option(const char *arg) {
  return arg[0] == '-';
}

static int argvlen(char **argv) {
  char **start = argv;
  
  while(*argv)
    ++argv;
  return argv - start;
}

static const struct option setup_guest_options[] = {
  { "help", no_argument, 0, 'h' },
  { "online-registration", no_argument, 0, 'r' },
  { "no-online-registration", no_argument, 0, 'R' },
  { 0, 0, 0, 0 }
};

static void help_setup_guest(void) {
  xprintf("Usage:\n"
	  "  disorder setup-guest [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h                Display usage message\n"
	  "  --online-registration     Enable online registration (default)\n"
	  "  --no-online-registration  Disable online registration\n");
  xfclose(stdout);
  exit(0);
}

static void cf_setup_guest(char **argv) {
  int n, online_registration = 1;
  
  while((n = getopt_long(argvlen(argv) + 1, argv - 1,
			 "hrR", setup_guest_options, 0)) >= 0) {
    switch(n) {
    case 'h': help_setup_guest();
    case 'r': online_registration = 1; break;
    case 'R': online_registration = 0; break;
    default: fatal(0, "invalid option");
    }
  }
  if(online_registration && !config->mail_sender)
    fatal(0, "you MUST set mail_sender if you want online registration");
  if(disorder_adduser(getclient(), "guest", "",
		      online_registration ? "read,register" : "read"))
    exit(EXIT_FAILURE);
}

static const struct command {
  const char *name;
  int min, max;
  void (*fn)(char **);
  int (*isarg)(const char *);
  const char *argstr, *desc;
} commands[] = {
  { "adduser",        2, 3, cf_adduser, isarg_rights, "USER PASSWORD [RIGHTS]",
                      "Create a new user" },
  { "allfiles",       1, 2, cf_allfiles, isarg_regexp, "DIR [~REGEXP]",
                      "List all files and directories in DIR" },
  { "authorize",      1, 2, cf_authorize, isarg_rights, "USER [RIGHTS]",
                      "Authorize USER to connect to the server" },
  { "deluser",        1, 1, cf_deluser, 0, "USER",
                      "Delete a user" },
  { "dirs",           1, 2, cf_dirs, isarg_regexp, "DIR [~REGEXP]",
                      "List directories in DIR" },
  { "disable",        0, 0, cf_disable, 0, "",
                      "Disable play" },
  { "disable-random", 0, 0, cf_random_disable, 0, "",
                      "Disable random play" },
  { "edituser",       3, 3, cf_edituser, 0, "USER PROPERTY VALUE",
                      "Set a property of a user" },
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
  { "new",            0, 1, cf_new, isarg_integer, "[MAX]",
                      "Get the most recently added MAX tracks" },
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
  { "rtp-address",    0, 0, cf_rtp_address, 0, "",
                      "Report server's broadcast address" },
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
  { "setup-guest",    0, INT_MAX, cf_setup_guest, isarg_option, "[OPTIONS]",
                      "Create the guest login" },
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
  { "userinfo",       2, 2, cf_userinfo, 0, "USER PROPERTY",
                      "Get a property of as user" },
  { "users",          0, 0, cf_users, 0, "",
                      "List all users" },
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
  int n, i, j, local = 0;
  int status = 0;
  struct vector args;
  const char *user = 0, *password = 0;

  mem_init();
  /* garbage-collect PCRE's memory */
  pcre_malloc = xmalloc;
  pcre_free = xfree;
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "+hVc:dHlNu:p:", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'H': help_commands();
    case 'V': version("disorder");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'l': local = 1; break;
    case 'N': config_per_user = 0; break;
    case 'u': user = optarg; break;
    case 'p': password = optarg; break;
    default: fatal(0, "invalid option");
    }
  }
  if(config_read(0)) fatal(0, "cannot read configuration");
  if(user) {
    config->username = user;
    config->password = 0;
  }
  if(password)
    config->password = password;
  if(local)
    config->connect.n = 0;
  n = optind;
  optind = 1;				/* for subsequent getopt calls */
  /* accumulate command args */
  while(n < argc) {
    if((i = TABLE_FIND(commands, struct command, name, argv[n])) < 0)
      fatal(0, "unknown command '%s'", argv[n]);
    if(n + commands[i].min >= argc)
      fatal(0, "missing arguments to '%s'", argv[n]);
    vector_init(&args);
    /* Include the command name in the args, but at element -1, for
     * the benefit of subcommand getopt calls */
    vector_append(&args, argv[n]);
    n++;
    for(j = 0; j < commands[i].min; ++j)
      vector_append(&args, nullcheck(mb2utf8(argv[n + j])));
    for(; j < commands[i].max
	  && n + j < argc
	  && commands[i].isarg(argv[n + j]); ++j)
      vector_append(&args, nullcheck(mb2utf8(argv[n + j])));
    vector_terminate(&args);
    commands[i].fn(args.vec + 1);
    n += j;
  }
  if(client && disorder_close(client)) exit(EXIT_FAILURE);
  if(fclose(stdout) < 0) fatal(errno, "error closing stdout");
  return status;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
