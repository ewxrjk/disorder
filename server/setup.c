/*
 * This file is part of DisOrder.
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
/** @file server/setup.c
 * @brief Automated setup functions
 */

#include <config.h>
#include "types.h"

#include <unistd.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

#include "log.h"
#include "mem.h"
#include "printf.h"
#include "configuration.h"
#include "setup.h"
#include "hex.h"
#include "defs.h"

/** @brief Create config.private with a login for root */
void make_root_login(void) {
  struct stat sb;
  char *privconfig,  *privconfignew;
  int fd;
  FILE *fp;
  struct passwd *pw;
  uint8_t pwbin[10];
  char *pwhex;
  
  if(config->user) {
    if(!(pw = getpwnam(config->user)))
      fatal(0, "cannot find user %s", config->user);
  } else
    pw = 0;
  /* Compute filenames */
  byte_xasprintf(&privconfig, "%s/config.private", pkgconfdir);
  byte_xasprintf(&privconfignew, "%s/config.private.new", pkgconfdir);
  /* If config.private already exists don't overwrite it */
  if(stat(privconfig, &sb) == 0)
    return;
  /* Choose a new root password */
  gcry_randomize(pwbin, sizeof pwbin, GCRY_STRONG_RANDOM);
  pwhex = hex(pwbin, sizeof pwbin);
  /* Create the file */
  if((fd = open(privconfignew, O_WRONLY|O_CREAT, 0600)) < 0)
    fatal(errno, "error creating %s", privconfignew);
  /* Fix permissions */
  if(pw) {
    if(fchown(fd, 0, pw->pw_gid) < 0)
      fatal(errno, "error setting owner/group for %s", privconfignew);
    if(fchmod(fd, 0640) < 0)
      fatal(errno, "error setting permissions for %s", privconfignew);
  }
  /* Write the required 'allow' line */
  if(!(fp = fdopen(fd, "w")))
    fatal(errno, "fdopen");
  if(fprintf(fp, "allow root %s\n", pwhex) < 0
     || fclose(fp) < 0)
    fatal(errno, "error writing %s", privconfignew);
  /* Rename into place */
  if(rename(privconfignew, privconfig) < 0)
    fatal(errno, "error renaming %s", privconfignew);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
