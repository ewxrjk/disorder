/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file server/lookup.h
 * @brief Server lookup code for CGI
 */

#ifndef LOOKUP_H
#define LOOKUP_H

extern disorder_client *client;

#define DC_QUEUE 0x0001
#define DC_PLAYING 0x0002
#define DC_RECENT 0x0004
#define DC_VOLUME 0x0008
#define DC_DIRS 0x0010
#define DC_FILES 0x0020
#define DC_NEW 0x0040
#define DC_RIGHTS 0x0080
#define DC_ENABLED 0x0100
#define DC_RANDOM_ENABLED 0x0200

extern struct queue_entry *queue;
extern struct queue_entry *playing;
extern struct queue_entry *recent;

extern int volume_left;
extern int volume_right;

extern char **files;
extern int nfiles;

extern char **dirs;
extern int ndirs;

extern char **new;
extern int nnew;

extern rights_type rights;

extern int enabled;
extern int random_enabled;

void lookup(unsigned want);
void lookup_reset(void);

#endif /* LOOKUP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
