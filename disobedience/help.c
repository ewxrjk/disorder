/*
 * This file is part of DisOrder
 * Copyright (C) 2007, 2008 Richard Kettlewell
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
/** @file disobedience/help.c
 * @brief Help support
 */

#include "disobedience.h"
#include <sys/wait.h>
#include <unistd.h>

/** @brief Display the manual page */
void popup_help(const char *what) {
  char *path;
  pid_t pid;
  int w;

  if(!what)
    what = "index.html";
  byte_xasprintf(&path, "%s/%s", dochtmldir, what);
  if(!(pid = xfork())) {
    exitfn = _exit;
    if(!xfork()) {
      execlp(browser, browser, path, (char *)0);
      disorder_fatal(errno, "error executing %s", browser);
    }
    _exit(0);
  }
  while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
    ;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
