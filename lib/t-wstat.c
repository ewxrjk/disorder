/*
 * This file is part of DisOrder.
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
#include "test.h"

void test_wstat(void) {
  pid_t pid;
  int w;
  
  fprintf(stderr, "test_wstat\n");
  if(!(pid = xfork())) {
    _exit(1);
  }
  while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
    ;
  check_string(wstat(w), "exited with status 1");
  if(!(pid = xfork())) {
    kill(getpid(), SIGTERM);
    _exit(-1);
  }
  while(waitpid(pid, &w, 0) < 0 && errno == EINTR)
    ;
  check_string_prefix(wstat(w), "terminated by signal 15");
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
