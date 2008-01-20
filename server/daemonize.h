/*
 * This file is part of DisOrder.
 * Copyright (C) 2004 Richard Kettlewell
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
/** @file server/daemonize.h
 * @brief Go into background
 */

#ifndef DAEMONIZE_H
#define DAEMONIZE_H

void daemonize(const char *tag, int fac, const char *pidfile);
/* Go into background.  Send stdout/stderr to syslog.
 * If @pri@ is non-null, it should be "facility.level"
 * If @tag@ is non-null, it is used as a tag to each message
 * If @pidfile@ is non-null, the PID is written to that file.
 */

#endif /* DAEMONIZE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
