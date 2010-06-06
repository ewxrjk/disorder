/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008 Richard Kettlewell
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
/** @file lib/addr.h
 * @brief Socket address support */

#ifndef ADDR_H
#define ADDR_H

#include <netdb.h>

#include "configuration.h"

struct addrinfo *get_address(const struct stringlist *a,
			     const struct addrinfo *pref,
			     char **namep);

int addrinfocmp(const struct addrinfo *a,
		const struct addrinfo *b);

int multicast(const struct sockaddr *sa);
char *format_sockaddr(const struct sockaddr *sa);

#endif /* ADDR_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
