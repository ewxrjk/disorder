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
/** @file lib/hreader.h
 * @brief Hands-off reader - read files without keeping them open
 */
#ifndef HREADER_H
#define HREADER_H

#include <unistd.h>

/** @brief A hands-off reader
 *
 * Allows files to be read without holding them open.
 */
struct hreader {
  const char *path;		/* file to read */
  off_t offset;			/* how far we've read so far */
  char *buffer;			/* input buffer */
  size_t bufsize;		/* buffer size */
  size_t bytes;			/* size of last read */
  size_t consumed;		/* bytes consumed by caller from last read */
};

/** @brief Initialize a hands-off reader
 * @param path File to read
 * @param h Reader to initialize
 */
void hreader_init(const char *path, struct hreader *h);

/** @brief Read some bytes
 * @param h Reader to read from
 * @param buffer Where to store bytes
 * @param n Maximum bytes to read
 * @return Bytes read, or 0 at EOF, or -1 on error
 */
int hreader_read(struct hreader *h, void *buffer, size_t n);

/** @brief Read more bytes
 * @param h Reader to update
 * @return Bytes available to read
 *
 * If not all bytes were consumed so far then just returns the number
 * of bytes left to consume.
 */
int hreader_fill(struct hreader *h);

/** @brief Consume some bytes
 * @param h Reader to update
 * @param n Bytes to consume
 */
void hreader_consume(struct hreader *h, size_t n);

#endif /* HREADER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
