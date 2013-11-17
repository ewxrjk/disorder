/*
 * This file is part of DisOrder
 * Copyright (C) 2013 Richard Kettlewell
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
/** @file lib/socketio.c
 * @brief Buffered socket IO
 */
#include "common.h"
#include "socketio.h"
#include "mem.h"

#include <errno.h>
#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

void socketio_init(struct socketio *sio, SOCKET sd) {
  sio->sd = sd;
  sio->inputptr = sio->inputlimit = sio->input;
  sio->outputused = 0;
  sio->error = 0;
}

int socketio_write(struct socketio *sio, const void *buffer, size_t n) {
  size_t chunk;
  while(n > 0) {
    chunk = n > sizeof sio->output ? sizeof sio->output : n;
    if(chunk) {
      memcpy(sio->output + sio->outputused, buffer, chunk);
      sio->outputused += chunk;
      buffer = (char *)buffer + chunk;
      n -= chunk;
    }
    if(sio->outputused == sizeof sio->output)
      if(socketio_flush(sio))
        return -1;
  }
  return 0;
}

static int socketio_fill(struct socketio *sio) {
  int n = recv(sio->sd, sio->input, sizeof sio->input, 0);
  if(n <= 0) {
    sio->error = n < 0 ? socket_error() : -1;
    return -1;
  }
  sio->inputptr = sio->input;
  sio->inputlimit = sio->input + n;
  return 0;
}

int socketio_getc(struct socketio *sio) {
  if(sio->inputptr >= sio->inputlimit) {
    if(socketio_fill(sio))
      return EOF;
  }
  return *sio->inputptr++;
}

int socketio_flush(struct socketio *sio) {
  size_t written = 0;
  while(written < sio->outputused) {
    int n = send(sio->sd, sio->output + written, sio->outputused - written, 0);
    if(n < 0) {
      sio->error = socket_error();
      return -1;
    }
    written += n;
  }
  sio->outputused = 0;
  return 0;
}

void socketio_close(struct socketio *sio) {
  socketio_flush(sio);
  closesocket(sio->sd);
}
