/*
 * This file is part of DisOrder
 * Copyright (C) 2010 Richard Kettlewell
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
/** @file lib/hreader.c
 * @brief Hands-off reader - read files without keeping them open
 */
#include "hreader.h"
#include "mem.h"
#include <string.h>
#include <fcntl.h>

static int hreader_fill(struct hreader *h, off_t offset);

void hreader_init(const char *path, struct hreader *h) {
  memset(h, 0, sizeof *h);
  h->path = xstrdup(path);
  h->bufsize = 65536;
  h->buffer = xmalloc_noptr(h->bufsize);
}

int hreader_read(struct hreader *h, void *buffer, size_t n) {
  int r = hreader_pread(h, buffer, n, h->read_offset);
  if(r > 0)
    h->read_offset += r;
  return r;
}

int hreader_pread(struct hreader *h, void *buffer, size_t n, off_t offset) {
  size_t bytes_read = 0;

  while(bytes_read < n) {
    // If the desired byte range is outside the file, fetch new contents
    if(offset < h->buf_offset || offset >= h->buf_offset + (off_t)h->bytes) {
      int r = hreader_fill(h, offset);
      if(r < 0)
        return -1;                      /* disaster! */
      else if(r == 0)
        break;                          /* end of file */
    }
    // Figure out how much we can read this time round
    size_t left = h->bytes - (offset - h->buf_offset);
    // Truncate the read if we don't want that much
    if(left > (n - bytes_read))
      left = n - bytes_read;
    memcpy((char *)buffer + bytes_read,
           h->buffer + (offset - h->buf_offset),
           left);
    offset += left;
    bytes_read += left;
  }
  return bytes_read;
}

static int hreader_fill(struct hreader *h, off_t offset) {
  int fd = open(h->path, O_RDONLY);
  if(fd < 0)
    return -1;
  int n = pread(fd, h->buffer, h->bufsize, offset);
  close(fd);
  if(n < 0)
    return -1;
  h->buf_offset = offset;
  h->bytes = n;
  return n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
