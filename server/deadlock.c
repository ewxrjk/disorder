/*
 * This file is part of DisOrder 
 * Copyright (C) 2005, 2007-2009 Richard Kettlewell
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
/** @file server/deadlock.c
 * @brief Deadlock monitor
 *
 * Spawned by the server.
 */
#include "disorder-server.h"

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "no-debug", no_argument, 0, 'D' },
  { "syslog", no_argument, 0, 's' },
  { "no-syslog", no_argument, 0, 'S' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void attribute((noreturn)) help(void) {
  xprintf("Usage:\n"
	  "  disorder-deadlock [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Force logging\n"
          "\n"
          "Deadlock manager for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

int main(int argc, char **argv) {
  int n, err, aborted, logsyslog = !isatty(2);

  set_progname(argv);
  if(!setlocale(LC_CTYPE, "")) disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-deadlock");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  config_per_user = 0;
  if(config_read(0, NULL)) disorder_fatal(0, "cannot read configuration");
  disorder_info("started");
  trackdb_init(TRACKDB_NO_RECOVER);
  while(getppid() != 1) {
    if((err = trackdb_env->lock_detect(trackdb_env,
				       0,
				       DB_LOCK_DEFAULT,
				       &aborted)))
      disorder_fatal(0, "trackdb_env->lock_detect: %s", db_strerror(err));
    if(aborted)
      D(("aborted %d lock requests", aborted));
    sleep(1);
  }
  /* if our parent goes away, it's time to stop */
  disorder_info("stopped (parent terminated)");
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
