/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2007, 2008, 2013 Richard Kettlewell
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

#if HAVE_NETDB_H
# include <netdb.h>
#endif

struct stringlist;

/** @brief A network address */
struct netaddress {
  /** @brief Address family
   *
   * Typically @c AF_UNIX, @c AF_INET, @c AF_INET6 or @c AF_UNSPEC.
   * Set to -1 to mean 'no address'.
   */
  int af;

  /** @brief Address or NULL for 'any' */
  char *address;

  /** @brief Port number */
  int port;
};

/** @brief A socket address and length */
struct resolved {

  /** @brief Pointer to the address */
  struct sockaddr *sa;

  /** @brief Length of the address */
  socklen_t len;
};

struct addrinfo *get_address(const struct stringlist *a,
			     const struct addrinfo *pref,
			     char **namep)
  attribute((nonnull (1)));

int addrinfocmp(const struct addrinfo *a,
		const struct addrinfo *b) attribute((nonnull (1, 2)));
int sockaddrcmp(const struct sockaddr *a,
		const struct sockaddr *b) attribute((nonnull (1, 2)));

int multicast(const struct sockaddr *sa) attribute((nonnull (1)));
char *format_sockaddr(const struct sockaddr *sa) attribute((nonnull (1)));

int netaddress_parse(struct netaddress *na,
		     int nvec,
		     char **vec) attribute((nonnull (1)));
void netaddress_format(const struct netaddress *na,
		       int *nvecp,
		       char ***vecp) attribute((nonnull (1)));
int netaddress_resolve(const struct netaddress *na, int passive, int type,
		       struct resolved **raddr_out, size_t *nraddr_out)
  attribute((nonnull (1, 4, 5)));
void netaddress_free_resolved(struct resolved *raddr, size_t nraddr);

#endif /* ADDR_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
