/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/speaker-protocol.c
 * @brief Speaker/server protocol support
 */

#include "common.h"

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>

#include "speaker-protocol.h"
#include "log.h"

/** @brief Send a speaker message
 * @param fd File descriptor to send to
 * @param sm Pointer to message
 */
void speaker_send(int fd, const struct speaker_message *sm) {
  int ret;

  do {
    ret = write(fd, sm, sizeof *sm);
  } while(ret < 0 && errno == EINTR);
  if(ret < 0)
    fatal(errno, "write");
}

/** @brief Receive a speaker message
 * @param fd File descriptor to read from
 * @param sm Where to store received message
 * @return -ve on @c EAGAIN, 0 at EOF, +ve on success
 */
int speaker_recv(int fd, struct speaker_message *sm) {
  int ret;

  do {
    ret = read(fd, sm, sizeof *sm);
  } while(ret < 0 && errno == EINTR);
  if(ret < 0) {
    if(errno != EAGAIN) fatal(errno, "recvmsg");
    return -1;
  }
  return ret;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
