
/*
 * This file is part of DisOrder.
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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ao/ao.h>
#include <ao/plugin.h>

/* extra declarations to help out lazy <ao/plugin.h> */
int ao_plugin_test(void);
ao_info *ao_plugin_driver_info(void);
char *ao_plugin_file_extension(void);

/* private data structure for this driver */
struct internal {
  int fd;				/* output file descriptor */
  int exit_on_error;			/* exit on write error */
};

/* like write() but never returns EINTR/EAGAIN or short */
static int do_write(int fd, const void *ptr, size_t n) {
  size_t written = 0;
  int ret;
  struct pollfd ufd;

  memset(&ufd, 0, sizeof ufd);
  ufd.fd = fd;
  ufd.events = POLLOUT;
  while(written < n) {
    ret = write(fd, (const char *)ptr + written, n - written);
    if(ret < 0) {
      switch(errno) {
      case EINTR: break;
      case EAGAIN:
	/* Someone sneakily gave us a nonblocking file descriptor, wait until
	 * we can write again */
	ret = poll(&ufd, 1, -1);
	if(ret < 0 && errno != EINTR) return -1;
	break;
      default:
	return -1;
      }
    } else
      written += ret;
  }
  return written;
}

/* return 1 if this driver can be opened */
int ao_plugin_test(void) {
  return 1;
}

/* return info about this driver */
ao_info *ao_plugin_driver_info(void) {
  static const char *options[] = { "fd" };
  static const ao_info info = {
    AO_TYPE_LIVE,			/* type */
    (char *)"DisOrder format driver",	/* name */
    (char *)"disorder",			/* short_name */
    (char *)"http://www.greenend.org.uk/rjk/disorder/", /* comment */
    (char *)"Richard Kettlewell",	/* author */
    AO_FMT_NATIVE,			/* preferred_byte_format */
    0,					/* priority */
    (char **)options,			/* options */
    1,					/* option_count */
  };
  return (ao_info *)&info;
}

/* initialize the private data structure */
int ao_plugin_device_init(ao_device *device) {
  struct internal *i = malloc(sizeof (struct internal));
  const char *e;

  if(!i) return 0;
  memset(i, 0, sizeof *i);
  if((e = getenv("DISORDER_RAW_FD")))
    i->fd = atoi(e);
  else
    i->fd = 1;
  device->internal = i;
  return 1;
}

/* set an option */
int ao_plugin_set_option(ao_device *device,
			 const char *key,
			 const char *value) {
  struct internal *i = device->internal;

  if(!strcmp(key, "fd"))
    i->fd = atoi(value);
  else if(!strcmp(key, "fragile"))
    i->exit_on_error = atoi(value);
  /* unknown options are required to be ignored */
  return 1;
}

/* open the device */
int ao_plugin_open(ao_device *device, ao_sample_format *format) {
  struct internal *i = device->internal;
  
  /* we would like native-order samples */
  device->driver_byte_format = AO_FMT_NATIVE;
  if(do_write(i->fd, format, sizeof *format) < 0) {
    if(i->exit_on_error) exit(-1);
    return 0;
  }
  return 1;
}

/* play some samples */
int ao_plugin_play(ao_device *device, const char *output_samples, 
		   uint_32 num_bytes) {
  struct internal *i = device->internal;

  if(do_write(i->fd, output_samples, num_bytes) < 0) {
    if(i->exit_on_error) _exit(-1);
    return 0;
  }
  return 1;
}

/* close the device */
int ao_plugin_close(ao_device attribute((unused)) *device) {
  return 1;
}

/* delete private data structures */
void ao_plugin_device_clear(ao_device *device) {
  free(device->internal);
  device->internal = 0;
}

/* report preferred filename extension */
char *ao_plugin_file_extension(void) {
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:ru/Jqo0hseWMoSt/ba4Xlw */
