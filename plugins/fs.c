/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#include <disorder.h>

void disorder_scan(const char *path) {
  struct stat sb;
  DIR *dp;
  struct dirent *de;
  char *np;

  if(stat(path, &sb) < 0) {
    disorder_error(errno, "cannot stat %s", path);
    return;
  }
  if(S_ISDIR(sb.st_mode)) {
    if(!(dp = opendir(path))) {
      disorder_error(errno, "cannot open directory %s", path);
      return;
    }
    while((errno = 0),
	  (de = readdir(dp))) {
      if(de->d_name[0] != '.') {
	disorder_asprintf(&np, "%s/%s", path, de->d_name);
	disorder_scan(np);
      }
    }
    if(errno)
      disorder_error(errno, "error reading directory %s", path);
    closedir(dp);
  } else if(S_ISREG(sb.st_mode)) {
    if(access(path, R_OK) < 0) {
      disorder_error(errno, "cannot access file %s", path);
      return;
    }
    if(printf("%s%c", path, 0) < 0)
      disorder_fatal(errno, "error writing to scanner output pipe");
  }
}

int disorder_check(const char attribute((unused)) *root, const char *path) {
  if(access(path, R_OK) == 0)
    return 1;
  else if(errno == ENOENT)
    return 0;
  else {
    disorder_error(errno, "cannot access %s", path);
    return -1;
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
