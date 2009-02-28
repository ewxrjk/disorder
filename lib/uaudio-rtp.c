/*
 * This file is part of DisOrder.
 * Copyright (C) 2009 Richard Kettlewell
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
/** @file lib/uaudio-oss.c
 * @brief Support for RTP network play backend */
#include "common.h"

#include <pthread.h>

#include "uaudio.h"
#include "mem.h"
#include "log.h"
#include "syscalls.h"

static const char *const rtp_options[] = {
  NULL
};

static void rtp_start(uaudio_callback *callback,
                      void *userdata) {
  (void)callback;
  (void)userdata;
  /* TODO */
}

static void rtp_stop(void) {
  /* TODO */
}

static void rtp_activate(void) {
  /* TODO */
}

static void rtp_deactivate(void) {
  /* TODO */
}

const struct uaudio uaudio_rtp = {
  .name = "rtp",
  .options = rtp_options,
  .start = rtp_start,
  .stop = rtp_stop,
  .activate = rtp_activate,
  .deactivate = rtp_deactivate
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
