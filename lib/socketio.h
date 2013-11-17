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
/** @file lib/socketio.h
 * @brief Buffered socket I/O
 */
#ifndef SOCKETIO_H
# define SOCKETIO_H

#define SOCKETIO_BUFFER 4096

struct socketio {
  SOCKET sd;
  char *inputptr, *inputlimit;
  size_t outputused;
  int error;
  char input[SOCKETIO_BUFFER];
  char output[SOCKETIO_BUFFER];
};

void socketio_init(struct socketio *sio, SOCKET sd);
int socketio_write(struct socketio *sio, const void *buffer, size_t n);
int socketio_getc(struct socketio *sio);
int socketio_flush(struct socketio *sio);
void socketio_close(struct socketio *sio);

static inline int socketio_error(struct socketio *sio) {
  return sio->error > 0 ? sio->error : 0;
}

static inline int socketio_eof(struct socketio *sio) {
  return sio->error == -1;
}

#endif
