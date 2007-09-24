/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/speaker-protocol.c
 * @brief Speaker/server protocol support
 */

#include <config.h>
#include "types.h"

#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#include "speaker-protocol.h"
#include "log.h"

/** @brief Send a speaker message
 * @param fd File descriptor to send to
 * @param sm Pointer to message
 * @param datafd File descriptoxr to pass with message or -1
 *
 * @p datafd will be the output from some decoder.
 */
void speaker_send(int fd, const struct speaker_message *sm, int datafd) {
  struct msghdr m;
  struct iovec iov;
  union {
    struct cmsghdr cmsg;
    char size[CMSG_SPACE(sizeof (int))];
  } u;
  int ret;

  memset(&m, 0, sizeof m);
  m.msg_iov = &iov;
  m.msg_iovlen = 1;
  iov.iov_base = (void *)sm;
  iov.iov_len = sizeof *sm;
  if(datafd != -1) {
    m.msg_control = (void *)&u.cmsg;
    m.msg_controllen = sizeof u;
    memset(&u, 0, sizeof u);
    u.cmsg.cmsg_len = CMSG_LEN(sizeof (int));
    u.cmsg.cmsg_level = SOL_SOCKET;
    u.cmsg.cmsg_type = SCM_RIGHTS;
    *(int *)CMSG_DATA(&u.cmsg) = datafd;
  }
  do {
    ret = sendmsg(fd, &m, 0);
  } while(ret < 0 && errno == EINTR);
  if(ret < 0)
    fatal(errno, "sendmsg");
}

/** @brief Receive a speaker message
 * @param fd File descriptor to read from
 * @param sm Where to store received message
 * @param datafd Where to store received file descriptor or NULL
 * @return -ve on @c EAGAIN, 0 at EOF, +ve on success
 *
 * If @p datafd is NULL but a file descriptor is nonetheless received,
 * the process is terminated with an error.
 */
int speaker_recv(int fd, struct speaker_message *sm, int *datafd) {
  struct msghdr m;
  struct iovec iov;
  union {
    struct cmsghdr cmsg;
    char size[CMSG_SPACE(sizeof (int))];
  } u;
  int ret;

  memset(&m, 0, sizeof m);
  m.msg_iov = &iov;
  m.msg_iovlen = 1;
  iov.iov_base = (void *)sm;
  iov.iov_len = sizeof *sm;
  if(datafd) {
    m.msg_control = (void *)&u.cmsg;
    m.msg_controllen = sizeof u;
    memset(&u, 0, sizeof u);
    u.cmsg.cmsg_len = CMSG_LEN(sizeof (int));
    u.cmsg.cmsg_level = SOL_SOCKET;
    u.cmsg.cmsg_type = SCM_RIGHTS;
    *datafd = -1;
  }
  do {
    ret = recvmsg(fd, &m, MSG_DONTWAIT);
  } while(ret < 0 && errno == EINTR);
  if(ret < 0) {
    if(errno != EAGAIN) fatal(errno, "recvmsg");
    return -1;
  }
  if((size_t)m.msg_controllen >= CMSG_LEN(sizeof (int))) {
    if(!datafd)
      fatal(0, "got an unexpected file descriptor from recvmsg");
    else
      *datafd = *(int *)CMSG_DATA(&u.cmsg);
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
