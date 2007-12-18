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

int mime_content_type(const char *s,
		      char **typep,
		      char **parameternamep,
		      char **parametervaluep);
/* Parse a content-type value.  returns 0 on success, -1 on error.
 * paramaternamep and parametervaluep are only set if present.
 * type and parametername are forced to lower case.
 */

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
char *mime_base64(const char *s, size_t *nsp);
char *mime_to_base64(const uint8_t *s, size_t ns);
/* convert quoted-printable or base64 data */

/** @brief Parsed form of an HTTP Cookie: header field */
struct cookiedata {
  /** @brief @c $Version or NULL if not set */
  char *version;

  /** @brief List of cookies */
  struct cookie *cookies;

  /** @brief Number of cookies */
  int ncookies;
};

/** @brief A parsed cookie */
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


#endif /* MIME_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
