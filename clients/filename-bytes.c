/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file clients/filename-bytes.c
 * @brief Print out raw bytes of filenames in a directory
 */

#include "common.h"

#include <dirent.h>
#include <ctype.h>

int main(int attribute((unused)) argc, char **argv) {
  DIR *dp;
  struct dirent *de;
  int n;
  
  if(!(dp = opendir(argv[1]))) return -1;
  while((de = readdir(dp))) {
    for(n = 0; de->d_name[n]; ++n) {
      printf("%02x", (unsigned char)de->d_name[n]);
      if(n) putchar(' ');
    }
    putchar('\n');
    for(n = 0; de->d_name[n]; ++n) {
      if(isprint((unsigned char)de->d_name[n]))
	printf(" %c", (unsigned char)de->d_name[n]);
      else
	printf("  ");
      if(n) putchar(' ');
    }
    putchar('\n');
  }
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
