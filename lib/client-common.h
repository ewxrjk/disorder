/*
 * This file is part of DisOrder
 * Copyright (C) 2004-2007 Richard Kettlewell
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
/** @file lib/client-common.h
 * @brief Common code to client APIs
 */

#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#include <sys/types.h>
#include <sys/socket.h>

socklen_t find_server(struct config *c, struct sockaddr **sap, char **namep);

/** @brief Marker for a command body */
extern const char disorder__body[1];

/** @brief Marker for a list of args */
extern const char disorder__list[1];

#endif /* CLIENT_COMMON_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
