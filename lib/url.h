/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/url.h
 * @brief URL support functions
 */

#ifndef URL_H
#define URL_H

/** @brief A parsed HTTP URL */
struct url {
  /** @brief URL scheme
   *
   * Typically "http" or "https".  Might be NULL for a relative URL.
   */
  char *scheme;

  /** @brief Username
   *
   * Might well be NULL.  NB not supported currently.
   */
  char *user;

  /** @brief Password
   *
   * Migth well be NULL.  NB not supported currently.
   */
  char *password;
  
  /** @brief Hostname
   *
   * Might be NULL for a relative URL.
   */
  char *host;

  /** @brief Port number or -1 if none specified */
  long port;

  /** @brief Path
   *
   * May be the empty string.  Never NULL.  Will be decoded from the
   * original URL.
   */
  char *path;

  /** @brief Query
   *
   * NULL if there was no query part.  Will NOT be decoded from the
   * original URL.
   */
  char *query;
};

char *infer_url(int include_path_info);
int parse_url(const char *url, struct url *parsed);

#endif /* URL_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
