/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2009 Richard Kettlewell
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
/** @file server/trackname.c
 * @brief Utility to run the track name calculator in isolation
 */
#include "disorder-server.h"

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void attribute((noreturn)) help(void) {
  xprintf("Usage:\n"
	  "  trackname [OPTIONS] TRACK CONTEXT PART\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n");
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n;
  const char *s;

  if(!setlocale(LC_CTYPE, ""))
    disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:d", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("trackname");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(argc - optind < 3) disorder_fatal(0, "not enough arguments");
  if(argc - optind > 3) disorder_fatal(0, "too many arguments");
  if(config_read(0, NULL)) disorder_fatal(0, "cannot read configuration");
  s = trackname_part(argv[optind], argv[optind+1], argv[optind+2]);
  if(!s) disorder_fatal(0, "trackname_part returned NULL");
  xprintf("%s\n", nullcheck(utf82mb(s)));
  if(fclose(stdout) < 0) disorder_fatal(errno, "error closing stdout");
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
