/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file lib/random.c
 * @brief Random number generator
 *
 */

#include "common.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "random.h"
#include "log.h"
#include "salsa208.h"
#include "basen.h"
#include "mem.h"

static int random_count;
static int random_fd = -1;
static salsa208_context random_ctx[1];

/** @brief Rekey the RNG
 *
 * Resets the RNG's key to a random one read from /dev/urandom
 */
static void random__rekey(void) {
  char key[32];
  int n;

  if(random_fd < 0) {
    if((random_fd = open("/dev/urandom", O_RDONLY)) < 0)
      disorder_fatal(errno, "opening /dev/urandom");
  }
  if((n = read(random_fd, key, sizeof key)) < 0)
    disorder_fatal(errno, "reading from /dev/urandom");
  if((size_t)n < sizeof key)
    disorder_fatal(0, "reading from /dev/urandom: short read");
  salsa208_setkey(random_ctx, key, sizeof key);
  random_count = 256 * 1024 * 1024;
}

/** @brief Get random bytes
 * @param ptr Where to put random bytes
 * @param bytes How many random bytes to generate
 */
void random_get(void *ptr, size_t bytes) {
  if(random_count == 0)
    random__rekey();
  salsa208_stream(random_ctx, 0, ptr, bytes);
  if(bytes > (size_t)random_count)
    random_count = 0;
  else
    random_count -= bytes;
}

/** @brief Return a random ID string */
char *random_id(void) {
  uint32_t words[2];
  char id[128];

  random_get(words, sizeof words);
  basen(words, sizeof words / sizeof *words, id, sizeof id, 62);
  return xstrdup(id);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
