/*
 * This file is part of DisOrder
 * Copyright (C) 2020 Mark Wooding
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
/** @file lib/home.c
 * @brief Find things in the user's home directory
 */

#include "common.h"

#include <errno.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_PWD_H
# include <pwd.h>
#endif
#if HAVE_SHLOBJ_H
# include <Shlobj.h>
#endif

#include "mem.h"
#include "home.h"
#include "log.h"
#include "printf.h"

#if _WIN32
#  define DIRSEP "\\"
#else
#  define DIRSEP "/"
#endif

static char *profiledir;


/** @brief Return the user's profile directory
 * @return profile directory
 * On Unix, this defaults to `$HOME/.disorder/'; on Windows, it's
 * `%APPDATA%\DisOrder\'.  The trailing delimiter is included.
 */
const char *profile_directory(void) {
  char *t;

  if(profiledir) return profiledir;
  if((t = getenv("DISORDER_HOME")))
    profiledir = t;
  else {
#if _WIN32
    wchar_t *wpath = 0;
    char *appdata;
    if(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) != S_OK) {
      disorder_error(0, "error calling SHGetKnownFolderPath");
      return 0;
    }
    t = win_wtomb(wpath);
    CoTaskMemFree(wpath);
    byte_xasprintf(&profiledir, "%s\\DisOrder", appdata);
#else
    struct passwd *pw;
    if(!(t = getenv("HOME"))) {
      if(!(pw = getpwuid(getuid())))
	disorder_error(0, "user not found in password database");
      t = pw->pw_dir;
    }
    byte_xasprintf(&profiledir, "%s/.disorder", t);
#endif
  }
  return profiledir;
}

/** @brief Return the name of a file within the user's profile directory
 * @param file Basename of the file desired
 * @return Full path name of selected file.
 * This currently doesn't do anything very useful with directory separators
 * within @a file.
 */
char *profile_filename(const char *file) {
  const char *d;
  char *t;
  if(!(d = profile_directory())) return 0;
  byte_xasprintf(&t, "%s" DIRSEP "%s", d, file);
  return t;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
