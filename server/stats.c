/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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

#include <locale.h>
#include <stdio.h>
#include <errno.h>
#include <pcre.h>
#include <getopt.h>

#include "defs.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"
#include "configuration.h"
#include "trackdb.h"

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-stats [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h               Display usage message\n"
	  "  --version, -V            Display version number\n"
	  "  --config PATH, -c PATH   Set configuration file\n"
	  "  --[no-]debug, -d         Turn on (off) debugging\n"
	  "\n"
	  "Generate DisOrder database statistics.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder-stats version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  char **stats;

  set_progname(argv);
  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dD", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    default: fatal(0, "invalid option");
    }
  }
  if(config_read(0))
    fatal(0, "cannot read configuration");
  trackdb_init(0);
  trackdb_open();
  stats = trackdb_stats(0);
  while(*stats)
    xprintf("%s\n", *stats++);
  xfclose(stdout);
  return 0;
}


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
