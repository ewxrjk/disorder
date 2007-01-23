/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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

#ifndef SPEAKER_H
#define SPEAKER_H

struct speaker_message {
  int type;                             /* message type */
  long data;                            /* whatever */
  char id[24];                          /* ID including terminator */
};

/* messages from the main DisOrder server */
#define SM_PREPARE 0                    /* prepare ID */
#define SM_PLAY 1                       /* play ID */
#define SM_PAUSE 2                      /* pause current track */
#define SM_RESUME 3                     /* resume current track */
#define SM_CANCEL 4                     /* cancel ID */
#define SM_RELOAD 5                     /* reload configuration */

/* messages from the speaker */
#define SM_PAUSED 128                   /* paused ID, DATA seconds in */
#define SM_FINISHED 129                 /* finished ID */
#define SM_PLAYING 131                  /* playing ID, DATA seconds in */

void speaker_send(int fd, const struct speaker_message *sm, int datafd);
/* Send a message.  DATAFD is passed too if not -1.  Does not close DATAFD. */

int speaker_recv(int fd, struct speaker_message *sm, int *datafd);
/* Receive a message.  If DATAFD is not null then can receive an FD.  Return 0
 * on EOF, +ve if a message is read, -1 on EAGAIN, terminates on any other
 * error. */

#endif /* SPEAKER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
/* arch-tag:QpuCTaKRDXEwz+Df/Jt+Wg */
