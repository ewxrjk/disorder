/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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
/** @file plugins/tracklength-mp3.c
 * @brief Compute track lengths for MP3 files
 */
#include "tracklength.h"
#include <mad.h>
#include "madshim.h"

static void *mmap_file(const char *path, size_t *lengthp) {
  int fd;
  void *base;
  struct stat sb;
  
  if((fd = open(path, O_RDONLY)) < 0) {
    disorder_error(errno, "error opening %s", path);
    return 0;
  }
  if(fstat(fd, &sb) < 0) {
    disorder_error(errno, "error calling stat on %s", path);
    goto error;
  }
  if(sb.st_size == 0)			/* can't map 0-length files */
    goto error;
  if((base = mmap(0, sb.st_size, PROT_READ,
		  MAP_SHARED, fd, 0)) == (void *)-1) {
    disorder_error(errno, "error calling mmap on %s", path);
    goto error;
  }
  *lengthp = sb.st_size;
  close(fd);
  return base;
error:
  close(fd);
  return 0;
}

long tl_mp3(const char *path) {
  size_t length;
  void *base;
  buffer b;

  if(!(base = mmap_file(path, &length))) return -1;
  b.duration = mad_timer_zero;
  scan_mp3(base, length, &b);
  munmap(base, length);
  return b.duration.seconds + !!b.duration.fraction;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
