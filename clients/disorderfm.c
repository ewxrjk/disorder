/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007, 2008 Richard Kettlewell
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

#include "common.h"

#include <getopt.h>
#include <unistd.h>
#include <locale.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <langinfo.h>
#include <fnmatch.h>

#include "syscalls.h"
#include "log.h"
#include "printf.h"
#include "charset.h"
#include "defs.h"
#include "mem.h"
#include "version.h"

/* Arguments etc ----------------------------------------------------------- */

typedef int copyfn(const char *from, const char *to);
typedef int mkdirfn(const char *dir, mode_t mode);

/* Input and output directories */
static const char *source, *destination;

/* Function used to copy or link a file */
static copyfn *copier = link;

/* Function used to make a directory */
static mkdirfn *dirmaker = mkdir;

/* Various encodings */
static const char *fromencoding, *toencoding, *tagencoding;

/* Directory for untagged files */
static const char *untagged;

/* Extract tag information? */
static int extracttags;

/* Windows-friendly filenames? */
static int windowsfriendly;

/* Native character encoding (i.e. from LC_CTYPE) */
static const char *nativeencoding;

/* Count of errors */
static long errors;

/* Included/excluded filename patterns */
static struct pattern {
  struct pattern *next;
  const char *pattern;
  int type;
} *patterns, **patterns_end = &patterns;

static int default_inclusion = 1;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "debug", no_argument, 0, 'd' },
  { "from", required_argument, 0, 'f' },
  { "to", required_argument, 0, 't' },
  { "include", required_argument, 0, 'i' },
  { "exclude", required_argument, 0, 'e' },
  { "extract-tags", no_argument, 0, 'E' },
  { "tag-encoding", required_argument, 0, 'T' },
  { "untagged", required_argument, 0, 'u' },
  { "windows-friendly", no_argument, 0, 'w' },
  { "link", no_argument, 0, 'l' },
  { "symlink", no_argument, 0, 's' },
  { "copy", no_argument, 0, 'c' },
  { "no-action", no_argument, 0, 'n' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
"  disorderfm [OPTIONS] SOURCE DESTINATION\n"
"Options:\n"
"  --from, -f ENCODING     Source encoding\n"
"  --to, -t ENCODING       Destination encoding\n"
"If neither --from nor --to are specified then no encoding translation is\n"
"performed.  If only one is specified then the other defaults to the current\n"
"locale's encoding.\n"
"  --windows-friendly, -w  Replace illegal characters with '_'\n"
"  --include, -i PATTERN   Include files matching a glob pattern\n"
"  --exclude, -e PATTERN   Include files matching a glob pattern\n"
"--include and --exclude may be used multiple times.  They are checked in\n"
"order and the first match wins.  If --include is ever used then nonmatching\n"
"files are excluded, otherwise they are included.\n"
"  --link, -l              Link files from source to destination (default)\n"
"  --symlink, -s           Symlink files from source to destination\n"
"  --copy, -c              Copy files from source to destination\n"
"  --no-action, -n         Just report what would be done\n"
"  --debug, -d             Debug mode\n"
"  --help, -h              Display usage message\n"
"  --version, -V           Display version number\n");
  /* TODO: tag extraction stuff when implemented */
  xfclose(stdout);
  exit(0);
}

/* Utilities --------------------------------------------------------------- */

/* Copy FROM to TO.  Has the same signature as link/symlink. */
static int copy(const char *from, const char *to) {
  int fdin, fdout;
  char buffer[4096];
  int n;

  if((fdin = open(from, O_RDONLY)) < 0)
    fatal(errno, "error opening %s", from);
  if((fdout = open(to, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
    fatal(errno, "error opening %s", to);
  while((n = read(fdin, buffer, sizeof buffer)) > 0) {
    if(write(fdout, buffer, n) < 0)
      fatal(errno, "error writing to %s", to);
  }
  if(n < 0) fatal(errno, "error reading %s", from);
  if(close(fdout) < 0) fatal(errno, "error closing %s", to);
  xclose(fdin);
  return 0;
}

static int nocopy(const char *from, const char *to) {
  xprintf("%s -> %s\n",
          any2mb(fromencoding, from),
          any2mb(toencoding, to));
  return 0;
}

static int nomkdir(const char *dir, mode_t attribute((unused)) mode) {
  xprintf("mkdir %s\n", any2mb(toencoding, dir));
  return 0;
}

/* Name translation -------------------------------------------------------- */

static int bad_windows_char(int c) {
  switch(c) {
  default:
    return 0;
    /* Documented as bad by MS */
  case '<':
  case '>':
  case ':':
  case '"':
  case '\\':
  case '|':
    /* Not documented as bad by MS but Samba mangles anyway? */
  case '*':
    return 1;
  }
}

/* Return the translated form of PATH */
static char *nametrans(const char *path) {
  char *t = any2any(fromencoding, toencoding, path);

  if(windowsfriendly) {
    /* See:
     * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/fileio/fs/naming_a_file.asp?frame=true&hidetoc=true */
    /* List of forbidden names */
    static const char *const devicenames[] = {
      "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
      "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
      "LPT6", "LPT7", "LPT8", "LPT9", "CLOCK$"
    };
#define NDEVICENAMES (sizeof devicenames / sizeof *devicenames)
    char *s;
    size_t n, l;

    /* Certain characters are just not allowed.  We replace them with
     * underscores. */
    for(s = t; *s; ++s)
      if(bad_windows_char((unsigned char)*s))
        *s = '_';
    /* Trailing spaces and dots are not allowed.  We just strip them. */
    while(s > t && (s[-1] == ' ' || s[-1] == '.'))
      --s;
    *s = 0;
    /* Reject device names */
    if((s = strchr(t, '.'))) l = s - t;
    else l = 0;
    for(n = 0; n < NDEVICENAMES; ++n)
      if(l == strlen(devicenames[n]) && !strncasecmp(devicenames[n], t, l))
        break;
    if(n < NDEVICENAMES)
      byte_xasprintf(&t, "_%s", t);
  }
  return t;
}

/* The file walker --------------------------------------------------------- */

/* Visit file or directory PATH relative to SOURCE.  SOURCE is a null pointer
 * at the top level.
 *
 * PATH is something we extracted from the filesystem so by assumption is in
 * the FROM encoding, which might _not_ be the same as the current locale's
 * encoding.
 *
 * For most errors we carry on as best we can.
 */
static void visit(const char *path, const char *destpath) {
  const struct pattern *p;
  struct stat sb;
  /* fullsourcepath is the full source pathname for PATH */
  char *fullsourcepath;
  /* fulldestpath will be the full destination pathname */
  char *fulldestpath;
  /* String to use in error messags.  We convert to the current locale; this
   * may be somewhat misleading but is necessary to avoid getting EILSEQ in
   * error messages. */
  char *errsourcepath, *errdestpath;

  D(("visit %s", path ? path : "NULL"));
  
  /* Set up all the various path names */
  if(path) {
    byte_xasprintf(&fullsourcepath, "%s/%s",
                   source, path);
    byte_xasprintf(&fulldestpath, "%s/%s",
                   destination, destpath);
    byte_xasprintf(&errsourcepath, "%s/%s",
                   source, any2mb(fromencoding, path));
    byte_xasprintf(&errdestpath, "%s/%s",
                   destination, any2mb(toencoding, destpath));
    for(p = patterns; p; p = p->next)
      if(fnmatch(p->pattern, path, FNM_PATHNAME) == 0)
        break;
    if(p) {
      /* We found a matching pattern */
      if(p->type == 'e') {
        D(("%s matches %s therefore excluding",
           path, p->pattern));
        return;
      }
    } else {
      /* We did not find a matching pattern */
      if(!default_inclusion) {
        D(("%s matches nothing and not including by default", path));
        return;
      }
    }
  } else {
    fullsourcepath = errsourcepath = (char *)source;
    fulldestpath = errdestpath = (char *)destination;
  }

  /* The destination directory might be a subdirectory of the source
   * directory. In that case we'd better not descend into it when we encounter
   * it in the source. */
  if(!strcmp(fullsourcepath, destination)) {
    info("%s matches destination directory, not recursing", errsourcepath);
    return;
  }
  
  /* Find out what kind of file we're dealing with */
  if(stat(fullsourcepath, &sb) < 0) {
    error(errno, "cannot stat %s", errsourcepath );
    ++errors;
    return;
  }
  if(S_ISREG(sb.st_mode)) {
    if(copier != nocopy)
      if(unlink(fulldestpath) < 0 && errno != ENOENT) {
        error(errno, "cannot remove %s", errdestpath);
        ++errors;
        return;
      }
    if(copier(fullsourcepath, fulldestpath) < 0) {
      error(errno, "cannot link %s to %s", errsourcepath, errdestpath);
      ++errors;
      return;
    }
  } else if(S_ISDIR(sb.st_mode)) {
    DIR *dp;
    struct dirent *de;
    char *childpath, *childdestpath;
  
    /* We create the directory on the destination side.  If it already exists,
     * that's fine. */
    if(dirmaker(fulldestpath, 0777) < 0 && errno != EEXIST) {
      error(errno, "cannot mkdir %s", errdestpath);
      ++errors;
      return;
    }
    /* We read the directory and visit all the files in it in any old order. */
    if(!(dp = opendir(fullsourcepath))) {
      error(errno, "cannot open directory %s", errsourcepath);
      ++errors;
      return;
    }
    while(((errno = 0), (de = readdir(dp)))) {
      if(!strcmp(de->d_name, ".")
         || !strcmp(de->d_name, "..")) continue;
      if(path) {
        byte_xasprintf(&childpath, "%s/%s", path, de->d_name);
        byte_xasprintf(&childdestpath, "%s/%s",
                       destpath, nametrans(de->d_name));
      } else {
        childpath = de->d_name;
        childdestpath = nametrans(de->d_name);
      }
      visit(childpath, childdestpath);
    }
    if(errno) fatal(errno, "error reading directory %s", errsourcepath);
    closedir(dp);
  } else {
    /* We don't handle special files, but we'd better warn the user. */
    info("ignoring %s", errsourcepath);
  }
}

int main(int argc, char **argv) {
  int n;
  struct pattern *p;

  mem_init();
  if(!setlocale(LC_CTYPE, "")) fatal(errno, "error calling setlocale");
  while((n = getopt_long(argc, argv, "hVdf:t:i:e:ET:u:wlscn", options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version("disorderfm");
    case 'd': debugging = 1; break;
    case 'f': fromencoding = optarg; break;
    case 't': toencoding = optarg; break;
    case 'i': 
    case 'e':
      p = xmalloc(sizeof *p);
      p->type = n;
      p->pattern = optarg;
      p->next = 0;
      *patterns_end = p;
      patterns_end = &p->next;
      if(n == 'i') default_inclusion = 0;
      break;
    case 'E': extracttags = 1; break;
    case 'T': tagencoding = optarg; break;
    case 'u': untagged = optarg; break;
    case 'w': windowsfriendly = 1; break;
    case 'l': copier = link; break;
    case 's': copier = symlink; break;
    case 'c': copier = copy; break;
    case 'n': copier = nocopy; dirmaker = nomkdir; break;
    default: fatal(0, "invalid option");
    }
  }
  if(optind == argc) fatal(0, "missing SOURCE and DESTINATION arguments");
  else if(optind + 1 == argc) fatal(0, "missing DESTINATION argument");
  else if(optind + 2 != argc) fatal(0, "redundant extra arguments");
  if(extracttags) fatal(0, "--extract-tags is not implemented yet"); /* TODO */
  if(tagencoding && !extracttags)
    fatal(0, "--tag-encoding without --extra-tags does not make sense");
  if(untagged && !extracttags)
    fatal(0, "--untagged without --extra-tags does not make sense");
  source = argv[optind];
  destination = argv[optind + 1];
  nativeencoding = nl_langinfo(CODESET);
  if(fromencoding || toencoding) {
    if(!fromencoding) fromencoding = nativeencoding;
    if(!toencoding) toencoding = nativeencoding;
  }
  if(!tagencoding) tagencoding = nativeencoding;
  visit(0, 0);
  xfclose(stdout);
  if(errors) fprintf(stderr, "%ld errors\n", errors);
  return !!errors;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
