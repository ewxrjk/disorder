/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
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
char *mime_base64(const char *s);
/* convert quoted-printable or base64 data */

#endif /* MIME_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
/* arch-tag:JMq56pGj+/kWY7mZDnHpWg */
