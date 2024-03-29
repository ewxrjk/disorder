/*
 * This file is part of DisOrder
 * Copyright (C) 2007-2009 Richard Kettlewell
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
/** @file server/normalize.c
 * @brief Convert "raw" format output to the configured format
 *
 * If libsamplerate is available then resample_convert() is used to do all
 * conversions.  If not then we invoke sox (even for trivial conversions such
 * as byte-swapping).  The sox support might be removed in a future version.
 */

#include "disorder-server.h"
#include "resample.h"

static char buffer[1024 * 1024];

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

/** @brief Copy bytes from one file descriptor to another
 * @param infd File descriptor read from
 * @param outfd File descriptor to write to
 * @param n Number of bytes to copy
 */
static void copy(int infd, int outfd, size_t n) {
  ssize_t written;

  while(n > 0) {
    const ssize_t readden = read(infd, buffer,
                                 n > sizeof buffer ? sizeof buffer : n);
    if(readden < 0) {
      if(errno == EINTR)
	continue;
      else
	disorder_fatal(errno, "read error");
    }
    if(readden == 0)
      disorder_fatal(0, "unexpected EOF");
    n -= readden;
    written = 0;
    while(written < readden) {
      const ssize_t w = write(outfd, buffer + written, readden - written);
      if(w < 0)
	disorder_fatal(errno, "write error");
      written += w;
    }
  }
}

#if !HAVE_SAMPLERATE_H
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
    default: disorder_fatal(0, "cannot handle sample size %d", header->bits);
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
      disorder_fatal(0, "cannot handle sample size %d", header->bits);
    *qq += sprintf((char *)(*(*pp)++ = *qq), "-%d", header->bits / 8) + 1;
    break;
  default:
    disorder_fatal(0, "unknown sox_generation %ld", config->sox_generation);
  }
}
#else
static void converted(uint8_t *bytes,
                      size_t nbytes,
                      void attribute((unused)) *cd) {
  /*syslog(LOG_INFO, "out: %02x %02x %02x %02x",
         bytes[0],
         bytes[1],
         bytes[2],
         bytes[3]);*/
  while(nbytes > 0) {
    ssize_t n = write(1, bytes, nbytes);
    if(n < 0)
      disorder_fatal(errno, "writing to stdout");
    bytes += n;
    nbytes -= n;
  }
}
#endif

int main(int argc, char attribute((unused)) **argv) {
  struct stream_header header, latest_format;
  int n, outfd = -1, logsyslog = !isatty(2), rs_in_use = 0;
  pid_t pid = -1;
  struct resampler rs[1];

  set_progname(argv);
  if(!setlocale(LC_CTYPE, ""))
    disorder_fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVc:dDSs", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorder-normalize");
    case 'c': configfile = optarg; break;
    case 'd': debugging = 1; break;
    case 'D': debugging = 0; break;
    case 'S': logsyslog = 0; break;
    case 's': logsyslog = 1; break;
    default: disorder_fatal(0, "invalid option");
    }
  }
  config_per_user = 0;
  if(config_read(1, NULL))
    disorder_fatal(0, "cannot read configuration");
  if(logsyslog) {
    openlog(progname, LOG_PID, LOG_DAEMON);
    log_default = &log_syslog;
  }
  memset(&latest_format, 0, sizeof latest_format);
  for(;;) {
    /* Read one header */
    n = 0;
    while((size_t)n < sizeof header) {
      int r = read(0, (char *)&header + n, sizeof header - n);

      if(r < 0) {
        if(errno != EINTR)
          disorder_fatal(errno, "error reading header");
      } else if(r == 0) {
        if(n)
          disorder_fatal(0, "EOF reading header");
        break;
      } else
        n += r;
    }
    if(!n)
      break;
    D(("NEW HEADER: %"PRIu32" bytes %"PRIu32"Hz %"PRIu8" channels %"PRIu8" bits %"PRIu8" endian",
       header.nbytes, header.rate, header.channels, header.bits, header.endian));
    /* Sanity check the header */
    if(header.rate < 100 || header.rate > 1000000)
      disorder_fatal(0, "implausible rate %"PRId32"Hz (%#"PRIx32")",
                     header.rate, header.rate);
    if(header.channels < 1 || header.channels > 2)
      disorder_fatal(0, "unsupported channel count %d", header.channels);
    if(header.bits % 8 || !header.bits || header.bits > 64)
      disorder_fatal(0, "unsupported sample size %d bits", header.bits);
    if(header.endian != ENDIAN_BIG && header.endian != ENDIAN_LITTLE)
      disorder_fatal(0, "unsupported byte order %d", header.endian);
    /* Skip empty chunks regardless of their alleged format */
    if(header.nbytes == 0)
      continue;
    /* If the format has changed we stop/start the converter */
#if HAVE_SAMPLERATE_H
    /* We have libsamplerate */
    if(formats_equal(&header, &config->sample_format))
      /* If the format is already correct then we just write out the data */
      copy(0, 1, header.nbytes);
    else {
      /* If we have a resampler active already check it is suitable and destroy
       * it if not */
      if(rs_in_use) {
        D(("call resample_close"));
        resample_close(rs);
        rs_in_use = 0;
      }
      /*syslog(LOG_INFO, "%d/%d/%d/%d/%d -> %d/%d/%d/%d/%d",
             header.bits,
             header.channels, 
             header.rate,
             1,
             header.endian,
             config->sample_format.bits,
             config->sample_format.channels, 
             config->sample_format.rate,
             1,
             config->sample_format.endian);*/
      if(!rs_in_use) {
        /* Create a suitable resampler. */
        D(("call resample_init"));
        resample_init(rs,
                      header.bits,
                      header.channels, 
                      header.rate,
                      1,                /* signed */
                      header.endian,
                      config->sample_format.bits,
                      config->sample_format.channels, 
                      config->sample_format.rate,
                      1,                /* signed */
                      config->sample_format.endian);
        latest_format = header;
        rs_in_use = 1;
        /* TODO speaker protocol does not record signedness of samples.  It's
         * assumed that they are always signed.  This should be fixed in the
         * future (and the sample format syntax extended in a compatible
         * way). */
      }
      /* Feed data through the resampler */
      size_t used = 0, left = header.nbytes;
      while(used || left) {
        if(left) {
          size_t limit = (sizeof buffer) - used;
          if(limit > left)
            limit = left;
          ssize_t r = read(0, buffer + used, limit);
          if(r < 0)
            disorder_fatal(errno, "reading from stdin");
          if(r == 0)
            disorder_fatal(0, "unexpected EOF");
          left -= r;
          used += r;
          //syslog(LOG_INFO, "read %zd bytes", r);
          D(("read %zd bytes", r));
        }
        /*syslog(LOG_INFO, " in: %02x %02x %02x %02x",
               (uint8_t)buffer[0],
               (uint8_t)buffer[1], 
               (uint8_t)buffer[2],
               (uint8_t)buffer[3]);*/
        D(("calling resample_convert used=%zu !left=%d", used, !left));
        const size_t consumed = resample_convert(rs,
                                                 (uint8_t *)buffer, used,
                                                 !left,
                                                 converted, 0);
        //syslog(LOG_INFO, "used=%zu consumed=%zu", used, consumed);
        D(("consumed=%zu", consumed));
        memmove(buffer, buffer + consumed, used - consumed);
        used -= consumed;
      }
    }
#else
    /* We do not have libsamplerate.  We will use sox instead. */
    if(!formats_equal(&header, &latest_format)) {
      if(pid != -1) {
        /* There's a running converter, stop it */
        xclose(outfd);
        if(waitpid(pid, &n, 0) < 0)
          disorder_fatal(errno, "error calling waitpid");
        if(n)
          disorder_fatal(0, "sox failed: %#x", n);
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
        int p[2];
        xpipe(p);
        if(!(pid = xfork())) {
          exitfn = _exit;
          xdup2(p[0], 0);
          xclose(p[0]);
          xclose(p[1]);
          execvp(av[0], (char **)av);
          disorder_fatal(errno, "sox");
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
#endif
  }
  if(outfd != -1)
    xclose(outfd);
  if(pid != -1) {
    /* There's still a converter running */
    if(waitpid(pid, &n, 0) < 0)
      disorder_fatal(errno, "error calling waitpid");
    if(n)
      disorder_fatal(0, "sox failed: %#x", n);
  }
  if(rs_in_use)
    resample_close(rs);
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
