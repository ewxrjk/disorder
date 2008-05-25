/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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

#include <pwd.h>
#include <gcrypt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "client.h"
#include "authorize.h"
#include "log.h"
#include "configuration.h"
#include "printf.h"
#include "base64.h"

/** @brief Create a DisOrder login for the calling user, called @p user
 * @param client DisOrder client
 * @param user Username to create (UTF-8)
 * @param rights Initial rights or NULL for default
 * @return 0 on success, non-0 on error
 */
int authorize(disorder_client *client, const char *user, const char *rights) {
  /* base64 is 3-into-4 so we make the password a multiple of 3 bytes long */
  uint8_t pwbin[12];
  const struct passwd *pw;
  char *pwhex;
  int fd;
  FILE *fp;
  char *configdir, *configpath, *configpathtmp;
  struct stat sb;
  uid_t old_uid = getuid();
  gid_t old_gid = getgid();

  if(!(pw = getpwnam(user)))
    /* If it's a NIS world then /etc/passwd may be a lie, but it emphasizes
     * that it's talking about the login user, not the DisOrder user */
    fatal(0, "no such user as %s in /etc/passwd", user);

  /* Choose a random password */
  gcry_randomize(pwbin, sizeof pwbin, GCRY_STRONG_RANDOM);
  pwhex = mime_to_base64(pwbin, sizeof pwbin);

  /* Create the user on the server */
  if(disorder_adduser(client, user, pwhex, rights))
    return -1;

  /* Become the target user */
  if(setegid(pw->pw_gid) < 0)
    fatal(errno, "setegid %lu", (unsigned long)pw->pw_gid);
  if(seteuid(pw->pw_uid) < 0)
    fatal(errno, "seteuid %lu", (unsigned long)pw->pw_uid);
  
  /* Make sure the configuration directory exists*/
  byte_xasprintf(&configdir, "%s/.disorder", pw->pw_dir);
  if(mkdir(configdir, 02700) < 0) {
    if(errno != EEXIST)
      fatal(errno, "creating %s", configdir);
  }

  /* Make sure the configuration file does not exist */
  byte_xasprintf(&configpath, "%s/passwd", configdir);
  if(lstat(configpath, &sb) == 0)
    fatal(0, "%s already exists", configpath);
  if(errno != ENOENT)
    fatal(errno, " checking %s", configpath);
  
  byte_xasprintf(&configpathtmp, "%s.new", configpath);

  /* Create config file with mode 600 */
  if((fd = open(configpathtmp, O_WRONLY|O_CREAT, 0600)) < 0)
    fatal(errno, "error creating %s", configpathtmp);

  /* Write password */
  if(!(fp = fdopen(fd, "w")))
    fatal(errno, "error calling fdopen");
  if(fprintf(fp, "password %s\n", pwhex) < 0
     || fclose(fp) < 0)
    fatal(errno, "error writing to %s", configpathtmp);

  /* Rename config file into place */
  if(rename(configpathtmp, configpath) < 0)
    fatal(errno, "error renaming %s to %s", configpathtmp, configpath);

  /* Put our identity back */
  if(seteuid(old_uid) < 0)
    fatal(errno, "seteuid %lu", (unsigned long)old_uid);
  if(setegid(old_gid) < 0)
    fatal(errno, "setegid %lu", (unsigned long)old_gid);
  
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
