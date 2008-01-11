/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2007 Richard Kettlewell
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
/** @file lib/mime.h
 * @brief Support for MIME and allied protocols
 */

#ifndef MIME_H
#define MIME_H

struct kvp;

int mime_content_type(const char *s,
		      char **typep,
		      struct kvp **parametersp);

const char *mime_parse(const char *s,
		       int (*callback)(const char *name, const char *value,
				       void *u),
		       void *u);
/* Parse a MIME message.  Calls CALLBACK for each header field, then returns a
 * pointer to the decoded body (might or might not point back into the original
 * string). */

int mime_multipart(const char *s,
		   int (*callback)(const char *s, void *u),
		   const char *boundary,
		   void *u);
/* call CALLBACK with each part of multipart document [s,s+n) */

int mime_rfc2388_content_disposition(const char *s,
				     char **dispositionp,
				     char **parameternamep,
				     char **parametervaluep);
/* Parse an RFC2388-style content-disposition field */

char *mime_qp(const char *s);

/** @brief Parsed form of an HTTP Cookie: header field
 *
 * See <a href="http://tools.ietf.org/html/rfc2109">RFC 2109</a>.
 */
struct cookiedata {
  /** @brief @c $Version or NULL if not set */
  char *version;

  /** @brief List of cookies */
  struct cookie *cookies;

  /** @brief Number of cookies */
  int ncookies;
};

/** @brief A parsed cookie
 *
 * See <a href="http://tools.ietf.org/html/rfc2109">RFC 2109</a> and @ref
 * cookiedata.
 */
struct cookie {
  /** @brief Cookie name */
  char *name;

  /** @brief Cookie value */
  char *value;

  /** @brief Cookie path */
  char *path;

  /** @brief Cookie domain */
  char *domain;
  
};

int parse_cookie(const char *s,
		 struct cookiedata *cd);
const struct cookie *find_cookie(const struct cookiedata *cd,
				 const char *name);
char *quote822(const char *s, int force);
char *mime_to_qp(const char *text);
const char *mime_encode_text(const char *text,
			     const char **charsetp,
			     const char **encodingp);
const char *mime_parse_word(const char *s, char **valuep,
			    int (*special)(int));
int mime_http_separator(int c);
int mime_tspecial(int c);

#endif /* MIME_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
