/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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

#include <pwd.h>
#include <gcrypt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "authorize.h"
#include "log.h"
#include "configuration.h"
#include "printf.h"
#include "hex.h"

int authorize(const char *user) {
  uint8_t pwbin[10];
  const struct passwd *pw, *jbpw;
  gid_t jbgid;
  char *c, *t, *pwhex;
  int fd;
  FILE *fp;

  if(!(jbpw = getpwnam(config->user)))
    fatal(0, "cannot find user %s", config->user);
  jbgid = jbpw->pw_gid;
  if(!(pw = getpwnam(user)))
    fatal(0, "no such user as %s", user);
  if((c = config_userconf(0, pw)) && access(c, F_OK) == 0) {
    error(0, "%s already exists", c);
    return -1;
  }
  if((c = config_usersysconf(pw)) && access(c, F_OK) == 0) {
    error(0, "%s already exists", c);
    return -1;
  }
  byte_xasprintf(&t, "%s.new", c);
  gcry_randomize(pwbin, sizeof pwbin, GCRY_STRONG_RANDOM);
  pwhex = hex(pwbin, sizeof pwbin);

  /* create config.USER, to end up with mode 440 user:jukebox */
  if((fd = open(t, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0)
    fatal(errno, "error creating %s", t);
  if(fchown(fd, pw->pw_uid, -1) < 0)
    fatal(errno, "error chowning %s", t);
  if(fchmod(fd, 0400) < 0)
    fatal(errno, "error chmoding %s", t);
  if(!(fp = fdopen(fd, "w")))
    fatal(errno, "error calling fdopen");
  if(fprintf(fp, "password %s\n", pwhex) < 0
     || fclose(fp) < 0)
    fatal(errno, "error writing to %s", t);
  if(rename(t, c) < 0)
    fatal(errno, "error renaming %s to %s", t, c);

  /* append to config.private.  We might create it along the way (though this
   * is unlikely) in which case it had better be 640 root:jukebox */
  if(!(c = config_private()))
    fatal(0, "cannot determine private config file");
  if((fd = open(c, O_WRONLY|O_APPEND|O_CREAT, 0600)) < 0)
    fatal(errno, "error opening %s", c);
  if(fchown(fd, 0, jbgid) < 0)
    fatal(errno, "error chowning %s", c);
  if(fchmod(fd, 0640) < 0)
    fatal(errno, "error chmoding %s", t);
  if(!(fp = fdopen(fd, "a")))
    fatal(errno, "error calling fdopen");
  if(fprintf(fp, "allow %s %s\n", user, pwhex) < 0
     || fclose(fp) < 0)
    fatal(errno, "error appending to %s", c);
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
