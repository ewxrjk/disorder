/*
 * This file is part of DisOrder.
 * Copyright (C) 2006 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <sys/select.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "queue.h"
#include "mem.h"
#include "log.h"
#include "eclient.h"
#include "configuration.h"
#include "syscalls.h"
#include "wstat.h"
#include "charset.h"

/* TODO: a more comprehensive test */

static fd_set rfd, wfd;
static int maxfd;
static disorder_eclient *clients[1024];
static char **tracks;
static disorder_eclient *c;
static char u_value;
static int quit;

static const char *modes[] = { "none", "read", "write", "read write" };

static void cb_comms_error(void *u, const char *msg) {
  assert(u == &u_value);
  fprintf(stderr, "! comms error: %s\n", msg);
}

static void cb_protocol_error(void *u,
			      void attribute((unused)) *v,
			      int attribute((unused)) code,
			      const char *msg) {
  assert(u == &u_value);
  fprintf(stderr, "! protocol error: %s\n", msg);
}

static void cb_poll(void *u, disorder_eclient *c_, int fd, unsigned mode) {
  assert(u == &u_value);
  assert(fd >= 0);
  assert(fd < 1024);			/* bodge */
  fprintf(stderr, "  poll callback %d %s\n", fd, modes[mode]);
  if(mode & DISORDER_POLL_READ)
    FD_SET(fd, &rfd);
  else
    FD_CLR(fd, &rfd);
  if(mode & DISORDER_POLL_WRITE)
    FD_SET(fd, &wfd);
  else
    FD_CLR(fd, &wfd);
  clients[fd] = mode ? c_ : 0;
  if(fd > maxfd) maxfd = fd;
}

static void cb_report(void attribute((unused)) *u,
		      const char attribute((unused)) *msg) {
}

static const disorder_eclient_callbacks callbacks = {
  cb_comms_error,
  cb_protocol_error,
  cb_poll,
  cb_report
};

/* cheap plastic event loop */
static void loop(void) {
  int n;
  
  while(!quit) {
    fd_set r = rfd, w = wfd;
    n = select(maxfd + 1, &r, &w, 0, 0);
    if(n < 0) {
      if(errno == EINTR) continue;
      fatal(errno, "select");
    }
    for(n = 0; n <= maxfd; ++n)
      if(clients[n] && (FD_ISSET(n, &r) || FD_ISSET(n, &w)))
	disorder_eclient_polled(clients[n],
				((FD_ISSET(n, &r) ? DISORDER_POLL_READ : 0)
				 |(FD_ISSET(n, &w) ? DISORDER_POLL_WRITE : 0)));
  }
  printf(". quit\n");
}

static void done(void) {
  printf(". done\n");
  disorder_eclient_close(c);
  quit = 1;
}

static void play_completed(void *v) {
  assert(v == tracks);
  printf("* played: %s\n", *tracks);
  ++tracks;
  if(*tracks) {
    if(disorder_eclient_play(c, *tracks, play_completed, tracks))
      exit(1);
  } else
    done();
}

static void version_completed(void *v, const char *value) {
  printf("* version: %s\n", value);
  if(v) {
    if(*tracks) {
      if(disorder_eclient_play(c, *tracks, play_completed, tracks))
	exit(1);
    } else
      done();
  }
}

/* TODO: de-dupe with disorder.c */
static void print_queue_entry(const struct queue_entry *q) {
  if(q->track) xprintf("track %s\n", nullcheck(utf82mb(q->track)));
  if(q->id) xprintf("  id %s\n", nullcheck(utf82mb(q->id)));
  if(q->submitter) xprintf("  submitted by %s at %s",
			   nullcheck(utf82mb(q->submitter)), ctime(&q->when));
  if(q->played) xprintf("  played at %s", ctime(&q->played));
  if(q->state == playing_started
     || q->state == playing_paused) xprintf("  %lds so far",  q->sofar);
  else if(q->expected) xprintf("  might start at %s", ctime(&q->expected));
  if(q->scratched) xprintf("  scratched by %s\n",
			   nullcheck(utf82mb(q->scratched)));
  else xprintf("  %s\n", playing_states[q->state]);
  if(q->wstat) xprintf("  %s\n", wstat(q->wstat));
}

static void recent_completed(void *v, struct queue_entry *q) {
  assert(v == 0);
  for(; q; q = q->next)
    print_queue_entry(q);
  if(disorder_eclient_version(c, version_completed, (void *)"")) exit(1);
}

int main(int argc, char **argv) {
  assert(argc > 0);
  mem_init(1);
  debugging = 0;			/* turn on for even more verbosity */
  if(config_read()) fatal(0, "config_read failed");
  tracks = &argv[1];
  c = disorder_eclient_new(&callbacks, &u_value);
  assert(c != 0);
  /* stack up several version commands to test pipelining */
  if(disorder_eclient_version(c, version_completed, 0)) exit(1);
  if(disorder_eclient_version(c, version_completed, 0)) exit(1);
  if(disorder_eclient_version(c, version_completed, 0)) exit(1);
  if(disorder_eclient_version(c, version_completed, 0)) exit(1);
  if(disorder_eclient_version(c, version_completed, 0)) exit(1);
  if(disorder_eclient_recent(c, recent_completed, 0)) exit(1);
  loop();
  exit(0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:JRDbPXSlG3t9Urek88LwCg */
