/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2010 Richard Kettlewell
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
/** @file driver/disorder.c
 * @brief libao driver used by DisOrder
 *
 * The output from this driver is expected to be fed to @c
 * disorder-normalize to convert to the confnigured target format.
 *
 * @attention This driver will not build with libao 1.0.0.  libao has
 * taken away half the plugin API and not provided any replacement.
 */

#include "common.h"

#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ao/ao.h>
#include <ao/plugin.h>

#include "speaker-protocol.h"

/* extra declarations to help out lazy <ao/plugin.h> */
int ao_plugin_test(void);
ao_info *ao_plugin_driver_info(void);
char *ao_plugin_file_extension(void);

/** @brief Private data structure for this driver */
struct internal {
  int fd;				/* output file descriptor */
  int exit_on_error;			/* exit on write error */

  /** @brief Record of sample format */
  struct stream_header header;

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
  i->header.rate = format->rate;
  i->header.channels = format->channels;
  i->header.bits = format->bits;
  i->header.endian = ENDIAN_NATIVE;
  return 1;
}

/* play some samples */
int ao_plugin_play(ao_device *device, const char *output_samples, 
		   uint_32 num_bytes) {
  struct internal *i = device->internal;

  /* Fill in and write the header */
  i->header.nbytes = num_bytes;
  if(do_write(i->fd, &i->header, sizeof i->header) < 0) {
    if(i->exit_on_error) _exit(-1);
    return 0;
  }

  /* Write the sample data */
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
