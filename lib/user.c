/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/user.c
 * @brief Jukebox user management
 */

#include "common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include "user.h"
#include "log.h"
#include "configuration.h"
#include "mem.h"

/** @brief Become the jukebox user
 *
 * If a jukebox user is configured then becomes that user.
 */
void become_mortal(void) {
  struct passwd *pw;
  
  if(config->user) {
    if(!(pw = getpwnam(config->user)))
      fatal(0, "cannot find user %s", config->user);
    if(pw->pw_uid != getuid()) {
      if(initgroups(config->user, pw->pw_gid))
	fatal(errno, "error calling initgroups");
      if(setgid(pw->pw_gid) < 0) fatal(errno, "error calling setgid");
      if(setuid(pw->pw_uid) < 0) fatal(errno, "error calling setgid");
      info("changed to user %s (uid %lu)", config->user, (unsigned long)getuid());
    }
    /* sanity checks */
    if(getuid() != pw->pw_uid) fatal(0, "wrong real uid");
    if(geteuid() != pw->pw_uid) fatal(0, "wrong effective uid");
    if(getgid() != pw->pw_gid) fatal(0, "wrong real gid");
    if(getegid() != pw->pw_gid) fatal(0, "wrong effective gid");
    if(setuid(0) != -1) fatal(0, "setuid(0) unexpectedly succeeded");
    if(seteuid(0) != -1) fatal(0, "seteuid(0) unexpectedly succeeded");
  }
}

/** @brief Create the jukebox state directory
 *
 * If the home directory does not exist then creates it and assigns
 * it suitable permissions.
 */
void make_home(void) {
  struct stat sb;
  struct passwd *pw;
  char *home, *p;
  
  if(stat(config->home, &sb) < 0) {
    /* create parent directories */
    home = xstrdup(config->home);
    p = home;
    while(*p) {
      if(*p == '/' && p > home) {
        *p = 0;
        mkdir(home, 0755);
        *p = '/';
      }
      ++p;
    }
    /* create the directory itself */
    if(mkdir(config->home, 02755) < 0)
      fatal(errno, "error creating %s", config->home);
    /* make sure it has the right ownership */
    if(config->user) {
      if(!(pw = getpwnam(config->user)))
        fatal(0, "cannot find user %s", config->user);
      if(chown(config->home, pw->pw_uid, pw->pw_gid) < 0)
        fatal(errno, "error chowning %s", config->home);
    }
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
