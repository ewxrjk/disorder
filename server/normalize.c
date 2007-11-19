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
/** @file server/normalize.c
 * @brief Convert "raw" format output to the configured format
 *
 * Currently we invoke sox even for trivial conversions such as byte-swapping.
 * Ideally we would do all conversion including resampling in this one process
 * and eliminate the dependency on sox.
 */

#include <config.h>
#include "types.h"

#include <getopt.h>
#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "syscalls.h"
#include "log.h"
#include "configuration.h"
#include "speaker-protocol.h"
#include "defs.h"

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
static void help(void) {
  xprintf("Usage:\n"
	  "  disorder-normalize [OPTIONS]\n"
	  "Options:\n"
	  "  --help, -h              Display usage message\n"
	  "  --version, -V           Display version number\n"
	  "  --config PATH, -c PATH  Set configuration file\n"
	  "  --debug, -d             Turn on debugging\n"
          "  --[no-]syslog           Force logging\n"
          "\n"
          "Audio format normalizer for DisOrder.  Not intended to be run\n"
          "directly.\n");
  xfclose(stdout);
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("disorder-normalize version %s\n", disorder_version_string);
  xfclose(stdout);
  exit(0);
}

/** @brief Copy bytes from one file descriptor to another
 * @param infd File descriptor read from
 * @param outfd File descriptor to write to
 * @param n Number of bytes to copy
 */
static void copy(int infd, int outfd, size_t n) {
  char buffer[4096];
  ssize_t written;

  while(n > 0) {
    const ssize_t readden = read(infd, buffer,
                                 n > sizeof buffer ? sizeof buffer : n);
    if(readden < 0) {
      if(errno == EINTR)
	continue;
      else
	fatal(errno, "read error");
    }
    if(readden == 0)
      fatal(0, "unexpected EOF");
    n -= readden;
    written = 0;
    while(written < readden) {
      const ssize_t w = write(outfd, buffer + written, readden - written);
      if(w < 0)
	fatal(errno, "write error");
      written += w;
    }
  }
}

static void soxargs(const char ***pp, char **qq,
                    const struct stream_header *header) {
  *(*pp)++ = "-t.raw";
  *(*pp)++ = "-s";
  *qq += sprintf((char *)(*(*pp)++ = *qq), "-r%d", header->rate) + 1;
  *qq += sprintf((char *)(*(*pp)++ = *qq), "-c%d", header->channels) + 1;
  /* sox 12.17.9 insists on -b etc; CVS sox insists on -<n> etc; both are
   * deployed! */
  switch(config->sox_generation) {
  case 0:
    if(header->bits != 8
       && header->endian != ENDIAN_NATIVE)
      *(*pp)++ = "-x";
    switch(header->bits) {
    case 8: *(*pp)++ = "-b"; break;
    case 16: *(*pp)++ = "-w"; break;
    case 32: *(*pp)++ = "-l"; break;
    case 64: *(*pp)++ = "-d"; break;
    default: fatal(0, "cannot handle sample size %d", header->bits);
    }
    break;
  case 1:
    if(header->bits != 8
       && header->endian != ENDIAN_NATIVE)
      switch(header->endian) {
      case ENDIAN_BIG: *(*pp)++ = "-B"; break;
      case ENDIAN_LITTLE: *(*pp)++ = "-L"; break;
      }
    if(header->bits % 8)
      fatal(0, "cannot handle sample size %d", header->bits);
    *qq += sprintf((char *)(*(*pp)++ = *qq), "-%d", header->bits / 8) + 1;
    break;
  default:
    fatal(0, "unknown sox_generation %ld", config->sox_generation);
  }
}

int main(int argc, char attribute((unused)) **argv) {
  struct stream_header header, latest_format;
  int n, p[2], outfd = -1, logsyslog = !isatty(2);
  pid_t pid = -1;

  set_progname(argv);
  if(!setlocale(LC_CTYPE, ""))
    fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: fatal(0, "invalid option");
    }
  }
  if(config_read(1))
    fatal(0, "cannot read configuration");
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  memset(&latest_format, 0, sizeof latest_format);
  for(;;) {
    n = 0;
    while((size_t)n < sizeof header) {
      int r = read(0, (char *)&header + n, sizeof header - n);

      if(r < 0) {
        if(errno != EINTR)
          fatal(errno, "error reading header");
      } else if(r == 0) {
        if(n)
          fatal(0, "EOF reading header");
        break;
      } else
        n += r;
    }
    if(!n)
      break;
    /* Sanity check the header */
    if(header.rate < 100 || header.rate > 1000000)
      fatal(0, "implausible rate %"PRId32"Hz (%#"PRIx32")",
            header.rate, header.rate);
    if(header.channels < 1 || header.channels > 2)
      fatal(0, "unsupported channel count %d", header.channels);
    if(header.bits % 8 || !header.bits || header.bits > 64)
      fatal(0, "unsupported sample size %d bits", header.bits);
    if(header.endian != ENDIAN_BIG && header.endian != ENDIAN_LITTLE)
      fatal(0, "unsupported byte order %x", header.bits);
    /* Skip empty chunks regardless of their alleged format */
    if(header.nbytes == 0)
      continue;
    /* If the format has changed we stop/start the converter */
    if(!formats_equal(&header, &latest_format)) {
      if(pid != -1) {
        /* There's a running converter, stop it */
        xclose(outfd);
        if(waitpid(pid, &n, 0) < 0)
          fatal(errno, "error calling waitpid");
        if(n)
          fatal(0, "sox failed: %#x", n);
        pid = -1;
        outfd = -1;
      }
      if(!formats_equal(&header, &config->sample_format)) {
        const char *av[32], **pp = av;
        char argbuf[1024], *q = argbuf;
        
        /* Input format doesn't match target, need to start a converter */
        *pp++ = "sox";
        soxargs(&pp, &q, &header);
        *pp++ = "-";                  /* stdin */
        soxargs(&pp, &q, &config->sample_format);
        *pp++ = "-";                  /* stdout */
        *pp = 0;
        /* This pipe will be sox's stdin */
        xpipe(p);
        if(!(pid = xfork())) {
          exitfn = _exit;
          xdup2(p[0], 0);
          xclose(p[0]);
          xclose(p[1]);
          execvp(av[0], (char **)av);
          fatal(errno, "sox");
        }
        xclose(p[0]);
        outfd = p[1];
      } else
        /* Input format matches output, can just copy bytes */
        outfd = 1;
      /* Remember current format for next iteration */
      latest_format = header;
    }
    /* Convert or copy this chunk */
    copy(0, outfd, header.nbytes);
  }
  if(outfd != -1)
    xclose(outfd);
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
