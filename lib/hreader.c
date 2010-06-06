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

void hreader_init(const char *path, struct hreader *h) {
  memset(h, 0, sizeof *h);
  h->path = xstrdup(path);
  h->bufsize = 65536;
  h->buffer = xmalloc_noptr(h->bufsize);
}

int hreader_read(struct hreader *h, void *buffer, size_t n) {
  if(h->consumed == h->bytes) {
    int r;

    if((r = hreader_fill(h)) <= 0)
      return r;
  }
  if(n > h->bytes - h->consumed)
    n = h->bytes - h->consumed;
  memcpy(buffer, h->buffer + h->consumed, n);
  h->consumed += n;
  return n;
}

int hreader_fill(struct hreader *h) {
  if(h->consumed < h->bytes)
    return h->bytes - h->consumed;
  h->bytes = h->consumed = 0;
  int fd = open(h->path, O_RDONLY);
  if(fd < 0)
    return -1;
  if(lseek(fd, h->offset, SEEK_SET) < 0) {
    close(fd);
    return -1;
  }
  int n = read(fd, h->buffer, h->bufsize);
  close(fd);
  if(n < 0)
    return -1;
  h->bytes = n;
  return n;
}

void hreader_consume(struct hreader *h, size_t n) {
  h->consumed += n;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
