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

#ifndef ADDR_H
#define ADDR_H

struct addrinfo *get_address(const struct stringlist *a,
			     const struct addrinfo *pref,
			     char **namep);

int addrinfocmp(const struct addrinfo *a,
		const struct addrinfo *b);

#endif /* ADDR_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
