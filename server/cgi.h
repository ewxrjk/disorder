/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef CGI_H
#define CGI_H

extern struct kvp *cgi_args;

typedef struct {
  int quote;
  struct sink *sink;
} cgi_sink;

void cgi_parse(void);
/* parse CGI args */

const char *cgi_get(const char *name);
/* get an argument */

void cgi_header(struct sink *output, const char *name, const char *value);
/* output a header.  @name@ and @value@ are ASCII. */

void cgi_body(struct sink *output);
/* indicate the start of the body */

void cgi_output(cgi_sink *output, const char *fmt, ...)
  attribute((format (printf, 2, 3)));
/* SGML-quote formatted UTF-8 data and write it.  Checks errors. */

char *cgi_sgmlquote(const char *s, int raw);
/* SGML-quote multibyte string @s@ */

void cgi_attr(struct sink *output, const char *name, const char *value);
/* write an attribute */

void cgi_opentag(struct sink *output, const char *name, ...);
/* write an open tag, including attribute name-value pairs terminate
 * by (char *)0 */

void cgi_closetag(struct sink *output, const char *name);
/* write a close tag */

struct cgi_expansion {
  const char *name;
  int minargs, maxargs;
  unsigned flags;
#define EXP_MAGIC 0x0001
  void (*handler)(int nargs, char **args, cgi_sink *output, void *u);
};

void cgi_expand(const char *name,
		const struct cgi_expansion *expansions,
		size_t nexpansions,
		cgi_sink *output,
		void *u);
/* find @name@ and substitute for expansions */

void cgi_expand_string(const char *name,
		       const char *template,
		       const struct cgi_expansion *expansions,
		       size_t nexpansions,
		       cgi_sink *output,
		       void *u);
/* same but @template@ is text of template */

char *cgi_makeurl(const char *url, ...);
/* make up a URL */

const char *cgi_label(const char *key);
/* look up the translated label @key@ */

char **cgi_columns(const char *name, int *nheadings);
/* return the list of columns for @name@ */

const char *cgi_transform(const char *type,
			  const char *track,
			  const char *context);
/* transform a track or directory name for display */

void cgi_set_option(const char *name, const char *value);
/* set an option */

#endif /* CGI_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:46351d0acf757f7867a139f369342949 */
