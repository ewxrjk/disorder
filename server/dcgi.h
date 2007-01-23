/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef DCGI_H
#define DCGI_H

typedef struct dcgi_global {
  disorder_client *client;
  unsigned flags;
#define DC_QUEUE 0x0001
#define DC_PLAYING 0x0002
#define DC_RECENT 0x0004
#define DC_VOLUME 0x0008
#define DC_DIRS 0x0010
#define DC_FILES 0x0020
  struct queue_entry *queue, *playing, *recent;
  int volume_left, volume_right;
  char **files, **dirs;
  int nfiles, ndirs;
} dcgi_global;

typedef struct dcgi_state {
  dcgi_global *g;
  struct queue_entry *track;
  struct kvp *pref;
  int index;
  int first, last;
  struct entry *entry;
  /* for searching */
  int ntracks;
  char **tracks;
  /* for @navigate@ */
  const char *nav_path;
  int nav_len, nav_dirlen;
} dcgi_state;

void disorder_cgi(cgi_sink *output, dcgi_state *ds);
void disorder_cgi_error(cgi_sink *output, dcgi_state *ds,
			const char *msg);

#endif /* DCGI_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
