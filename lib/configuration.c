/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006, 2007 Richard Kettlewell
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
/** @file lib/configuration.c
 * @brief Configuration file support
 */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stddef.h>
#include <pwd.h>
#include <langinfo.h>
#include <pcre.h>
#include <signal.h>

#include "configuration.h"
#include "mem.h"
#include "log.h"
#include "split.h"
#include "syscalls.h"
#include "table.h"
#include "inputline.h"
#include "charset.h"
#include "defs.h"
#include "mixer.h"
#include "printf.h"
#include "regsub.h"
#include "signame.h"

/** @brief Path to config file 
 *
 * set_configfile() sets the deafult if it is null.
 */
char *configfile;

/** @brief Config file parser state */
struct config_state {
  /** @brief Filename */
  const char *path;
  /** @brief Line number */
  int line;
  /** @brief Configuration object under construction */
  struct config *config;
};

/** @brief Current configuration */
struct config *config;

/** @brief One configuration item */
struct conf {
  /** @brief Name as it appears in the config file */
  const char *name;
  /** @brief Offset in @ref config structure */
  size_t offset;
  /** @brief Pointer to item type */
  const struct conftype *type;
  /** @brief Pointer to item-specific validation routine */
  int (*validate)(const struct config_state *cs,
		  int nvec, char **vec);
};

/** @brief Type of a configuration item */
struct conftype {
  /** @brief Pointer to function to set item */
  int (*set)(const struct config_state *cs,
	     const struct conf *whoami,
	     int nvec, char **vec);
  /** @brief Pointer to function to free item */
  void (*free)(struct config *c, const struct conf *whoami);
};

/** @brief Compute the address of an item */
#define ADDRESS(C, TYPE) ((TYPE *)((char *)(C) + whoami->offset))
/** @brief Return the value of an item */
#define VALUE(C, TYPE) (*ADDRESS(C, TYPE))

static int set_signal(const struct config_state *cs,
		      const struct conf *whoami,
		      int nvec, char **vec) {
  int n;
  
  if(nvec != 1) {
    error(0, "%s:%d: '%s' requires one argument",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  if((n = find_signal(vec[0])) == -1) {
    error(0, "%s:%d: unknown signal '%s'",
	  cs->path, cs->line, vec[0]);
    return -1;
  }
  VALUE(cs->config, int) = n;
  return 0;
}

static int set_collections(const struct config_state *cs,
			   const struct conf *whoami,
			   int nvec, char **vec) {
  struct collectionlist *cl;
  
  if(nvec != 3) {
    error(0, "%s:%d: '%s' requires three arguments",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  if(vec[2][0] != '/') {
    error(0, "%s:%d: collection root must start with '/'",
	  cs->path, cs->line);
    return -1;
  }
  if(vec[2][1] && vec[2][strlen(vec[2])-1] == '/') {
    error(0, "%s:%d: collection root must not end with '/'",
	  cs->path, cs->line);
    return -1;
  }
  cl = ADDRESS(cs->config, struct collectionlist);
  ++cl->n;
  cl->s = xrealloc(cl->s, cl->n * sizeof (struct collection));
  cl->s[cl->n - 1].module = xstrdup(vec[0]);
  cl->s[cl->n - 1].encoding = xstrdup(vec[1]);
  cl->s[cl->n - 1].root = xstrdup(vec[2]);
  return 0;
}

static int set_boolean(const struct config_state *cs,
		       const struct conf *whoami,
		       int nvec, char **vec) {
  int state;
  
  if(nvec != 1) {
    error(0, "%s:%d: '%s' takes only one argument",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  if(!strcmp(vec[0], "yes")) state = 1;
  else if(!strcmp(vec[0], "no")) state = 0;
  else {
    error(0, "%s:%d: argument to '%s' must be 'yes' or 'no'",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  VALUE(cs->config, int) = state;
  return 0;
}

static int set_string(const struct config_state *cs,
		      const struct conf *whoami,
		      int nvec, char **vec) {
  if(nvec != 1) {
    error(0, "%s:%d: '%s' takes only one argument",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  VALUE(cs->config, char *) = xstrdup(vec[0]);
  return 0;
}

static int set_stringlist(const struct config_state *cs,
			  const struct conf *whoami,
			  int nvec, char **vec) {
  int n;
  struct stringlist *sl;

  sl = ADDRESS(cs->config, struct stringlist);
  sl->n = 0;
  for(n = 0; n < nvec; ++n) {
    sl->n++;
    sl->s = xrealloc(sl->s, (sl->n * sizeof (char *)));
    sl->s[sl->n - 1] = xstrdup(vec[n]);
  }
  return 0;
}

static int set_integer(const struct config_state *cs,
		       const struct conf *whoami,
		       int nvec, char **vec) {
  char *e;

  if(nvec != 1) {
    error(0, "%s:%d: '%s' takes only one argument",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  if(xstrtol(ADDRESS(cs->config, long), vec[0], &e, 0)) {
    error(errno, "%s:%d: converting integer", cs->path, cs->line);
    return -1;
  }
  if(*e) {
    error(0, "%s:%d: invalid integer syntax", cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int set_stringlist_accum(const struct config_state *cs,
				const struct conf *whoami,
				int nvec, char **vec) {
  int n;
  struct stringlist *s;
  struct stringlistlist *sll;

  sll = ADDRESS(cs->config, struct stringlistlist);
  sll->n++;
  sll->s = xrealloc(sll->s, (sll->n * sizeof (struct stringlist)));
  s = &sll->s[sll->n - 1];
  s->n = nvec;
  s->s = xmalloc((nvec + 1) * sizeof (char *));
  for(n = 0; n < nvec; ++n)
    s->s[n] = xstrdup(vec[n]);
  return 0;
}

static int set_string_accum(const struct config_state *cs,
			    const struct conf *whoami,
			    int nvec, char **vec) {
  int n;
  struct stringlist *sl;

  sl = ADDRESS(cs->config, struct stringlist);
  for(n = 0; n < nvec; ++n) {
    sl->n++;
    sl->s = xrealloc(sl->s, (sl->n * sizeof (char *)));
    sl->s[sl->n - 1] = xstrdup(vec[n]);
  }
  return 0;
}

static int set_restrict(const struct config_state *cs,
			const struct conf *whoami,
			int nvec, char **vec) {
  unsigned r = 0;
  int n, i;
  
  static const struct restriction {
    const char *name;
    unsigned bit;
  } restrictions[] = {
    { "remove", RESTRICT_REMOVE },
    { "scratch", RESTRICT_SCRATCH },
    { "move", RESTRICT_MOVE },
  };

  for(n = 0; n < nvec; ++n) {
    if((i = TABLE_FIND(restrictions, struct restriction, name, vec[n])) < 0) {
      error(0, "%s:%d: invalid restriction '%s'",
	    cs->path, cs->line, vec[n]);
      return -1;
    }
    r |= restrictions[i].bit;
  }
  VALUE(cs->config, unsigned) = r;
  return 0;
}

static int parse_sample_format(const struct config_state *cs,
			       ao_sample_format *ao,
			       int nvec, char **vec) {
  char *p = vec[0];
  long t;

  if (nvec != 1) {
    error(0, "%s:%d: wrong number of arguments", cs->path, cs->line);
    return -1;
  }
  if (xstrtol(&t, p, &p, 0)) {
    error(errno, "%s:%d: converting bits-per-sample", cs->path, cs->line);
    return -1;
  }
  if (t != 8 && t != 16) {
    error(0, "%s:%d: bad bite-per-sample (%ld)", cs->path, cs->line, t);
    return -1;
  }
  if (ao) ao->bits = t;
  switch (*p) {
    case 'l': case 'L': t = AO_FMT_LITTLE; p++; break;
    case 'b': case 'B': t = AO_FMT_BIG; p++; break;
    default: t = AO_FMT_NATIVE; break;
  }
  if (ao) ao->byte_format = t;
  if (*p != '/') {
    error(errno, "%s:%d: expected `/' after bits-per-sample",
	  cs->path, cs->line);
    return -1;
  }
  p++;
  if (xstrtol(&t, p, &p, 0)) {
    error(errno, "%s:%d: converting sample-rate", cs->path, cs->line);
    return -1;
  }
  if (t < 1 || t > INT_MAX) {
    error(0, "%s:%d: silly sample-rate (%ld)", cs->path, cs->line, t);
    return -1;
  }
  if (ao) ao->rate = t;
  if (*p != '/') {
    error(0, "%s:%d: expected `/' after sample-rate",
	  cs->path, cs->line);
    return -1;
  }
  p++;
  if (xstrtol(&t, p, &p, 0)) {
    error(errno, "%s:%d: converting channels", cs->path, cs->line);
    return -1;
  }
  if (t < 1 || t > 8) {
    error(0, "%s:%d: silly number (%ld) of channels", cs->path, cs->line, t);
    return -1;
  }
  if (ao) ao->channels = t;
  if (*p) {
    error(0, "%s:%d: junk after channels", cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int set_sample_format(const struct config_state *cs,
			     const struct conf *whoami,
			     int nvec, char **vec) {
  return parse_sample_format(cs, ADDRESS(cs->config, ao_sample_format),
			     nvec, vec);
}

static int set_namepart(const struct config_state *cs,
			const struct conf *whoami,
			int nvec, char **vec) {
  struct namepartlist *npl = ADDRESS(cs->config, struct namepartlist);
  unsigned reflags;
  const char *errstr;
  int erroffset, n;
  pcre *re;

  if(nvec < 3) {
    error(0, "%s:%d: namepart needs at least 3 arguments", cs->path, cs->line);
    return -1;
  }
  if(nvec > 5) {
    error(0, "%s:%d: namepart needs at most 5 arguments", cs->path, cs->line);
    return -1;
  }
  reflags = nvec >= 5 ? regsub_flags(vec[4]) : 0;
  if(!(re = pcre_compile(vec[1],
			 PCRE_UTF8
			 |regsub_compile_options(reflags),
			 &errstr, &erroffset, 0))) {
    error(0, "%s:%d: error compiling regexp /%s/: %s (offset %d)",
	  cs->path, cs->line, vec[1], errstr, erroffset);
    return -1;
  }
  npl->s = xrealloc(npl->s, (npl->n + 1) * sizeof (struct namepart));
  npl->s[npl->n].part = xstrdup(vec[0]);
  npl->s[npl->n].re = re;
  npl->s[npl->n].replace = xstrdup(vec[2]);
  npl->s[npl->n].context = xstrdup(vec[3]);
  npl->s[npl->n].reflags = reflags;
  ++npl->n;
  /* XXX a bit of a bodge; relies on there being very few parts. */
  for(n = 0; (n < cs->config->nparts
	      && strcmp(cs->config->parts[n], vec[0])); ++n)
    ;
  if(n >= cs->config->nparts) {
    cs->config->parts = xrealloc(cs->config->parts,
				 (cs->config->nparts + 1) * sizeof (char *));
    cs->config->parts[cs->config->nparts++] = xstrdup(vec[0]);
  }
  return 0;
}

static int set_transform(const struct config_state *cs,
			 const struct conf *whoami,
			 int nvec, char **vec) {
  struct transformlist *tl = ADDRESS(cs->config, struct transformlist);
  pcre *re;
  unsigned reflags;
  const char *errstr;
  int erroffset;

  if(nvec < 3) {
    error(0, "%s:%d: transform needs at least 3 arguments", cs->path, cs->line);
    return -1;
  }
  if(nvec > 5) {
    error(0, "%s:%d: transform needs at most 5 arguments", cs->path, cs->line);
    return -1;
  }
  reflags = (nvec >= 5 ? regsub_flags(vec[4]) : 0);
  if(!(re = pcre_compile(vec[1],
			 PCRE_UTF8
			 |regsub_compile_options(reflags),
			 &errstr, &erroffset, 0))) {
    error(0, "%s:%d: error compiling regexp /%s/: %s (offset %d)",
	  cs->path, cs->line, vec[1], errstr, erroffset);
    return -1;
  }
  tl->t = xrealloc(tl->t, (tl->n + 1) * sizeof (struct namepart));
  tl->t[tl->n].type = xstrdup(vec[0]);
  tl->t[tl->n].context = xstrdup(vec[3] ? vec[3] : "*");
  tl->t[tl->n].re = re;
  tl->t[tl->n].replace = xstrdup(vec[2]);
  tl->t[tl->n].flags = reflags;
  ++tl->n;
  return 0;
}

static int set_backend(const struct config_state *cs,
		       const struct conf *whoami,
		       int nvec, char **vec) {
  int *const valuep = ADDRESS(cs->config, int);
  
  if(nvec != 1) {
    error(0, "%s:%d: '%s' requires one argument",
	  cs->path, cs->line, whoami->name);
    return -1;
  }
  if(!strcmp(vec[0], "alsa")) {
#if API_ALSA
    *valuep = BACKEND_ALSA;
#else
    error(0, "%s:%d: ALSA is not available on this platform",
	  cs->path, cs->line);
    return -1;
#endif
  } else if(!strcmp(vec[0], "command"))
    *valuep = BACKEND_COMMAND;
  else if(!strcmp(vec[0], "network"))
    *valuep = BACKEND_NETWORK;
  else {
    error(0, "%s:%d: invalid '%s' value '%s'",
	  cs->path, cs->line, whoami->name, vec[0]);
    return -1;
  }
  return 0;
}

/* free functions */

static void free_none(struct config attribute((unused)) *c,
		      const struct conf attribute((unused)) *whoami) {
}

static void free_string(struct config *c,
			const struct conf *whoami) {
  xfree(VALUE(c, char *));
}

static void free_stringlist(struct config *c,
			    const struct conf *whoami) {
  int n;
  struct stringlist *sl = ADDRESS(c, struct stringlist);

  for(n = 0; n < sl->n; ++n)
    xfree(sl->s[n]);
  xfree(sl->s);
}

static void free_stringlistlist(struct config *c,
				const struct conf *whoami) {
  int n, m;
  struct stringlistlist *sll = ADDRESS(c, struct stringlistlist);
  struct stringlist *sl;

  for(n = 0; n < sll->n; ++n) {
    sl = &sll->s[n];
    for(m = 0; m < sl->n; ++m)
      xfree(sl->s[m]);
    xfree(sl->s);
  }
  xfree(sll->s);
}

static void free_collectionlist(struct config *c,
				const struct conf *whoami) {
  struct collectionlist *cll = ADDRESS(c, struct collectionlist);
  struct collection *cl;
  int n;

  for(n = 0; n < cll->n; ++n) {
    cl = &cll->s[n];
    xfree(cl->module);
    xfree(cl->encoding);
    xfree(cl->root);
  }
  xfree(cll->s);
}

static void free_namepartlist(struct config *c,
			      const struct conf *whoami) {
  struct namepartlist *npl = ADDRESS(c, struct namepartlist);
  struct namepart *np;
  int n;

  for(n = 0; n < npl->n; ++n) {
    np = &npl->s[n];
    xfree(np->part);
    pcre_free(np->re);			/* ...whatever pcre_free is set to. */
    xfree(np->replace);
    xfree(np->context);
  }
  xfree(npl->s);
}

static void free_transformlist(struct config *c,
			       const struct conf *whoami) {
  struct transformlist *tl = ADDRESS(c, struct transformlist);
  struct transform *t;
  int n;

  for(n = 0; n < tl->n; ++n) {
    t = &tl->t[n];
    xfree(t->type);
    pcre_free(t->re);			/* ...whatever pcre_free is set to. */
    xfree(t->replace);
    xfree(t->context);
  }
  xfree(tl->t);
}

/* configuration types */

static const struct conftype
  type_signal = { set_signal, free_none },
  type_collections = { set_collections, free_collectionlist },
  type_boolean = { set_boolean, free_none },
  type_string = { set_string, free_string },
  type_stringlist = { set_stringlist, free_stringlist },
  type_integer = { set_integer, free_none },
  type_stringlist_accum = { set_stringlist_accum, free_stringlistlist },
  type_string_accum = { set_string_accum, free_stringlist },
  type_sample_format = { set_sample_format, free_none },
  type_restrict = { set_restrict, free_none },
  type_namepart = { set_namepart, free_namepartlist },
  type_transform = { set_transform, free_transformlist },
  type_backend = { set_backend, free_none };

/* specific validation routine */

#define VALIDATE_FILE(test, what) do {				\
  struct stat sb;						\
  int n;							\
								\
  for(n = 0; n < nvec; ++n) {					\
    if(stat(vec[n], &sb) < 0) {					\
      error(errno, "%s:%d: %s", cs->path, cs->line, vec[n]);	\
      return -1;						\
    }								\
    if(!test(sb.st_mode)) {					\
      error(0, "%s:%d: %s is not a %s",				\
	    cs->path, cs->line, vec[n], what);			\
      return -1;						\
    }								\
  }								\
} while(0)

static int validate_isdir(const struct config_state *cs,
			  int nvec, char **vec) {
  VALIDATE_FILE(S_ISDIR, "directory");
  return 0;
}

static int validate_isreg(const struct config_state *cs,
			  int nvec, char **vec) {
  VALIDATE_FILE(S_ISREG, "regular file");
  return 0;
}

static int validate_ischr(const struct config_state *cs,
			  int nvec, char **vec) {
  VALIDATE_FILE(S_ISCHR, "character device");
  return 0;
}

static int validate_player(const struct config_state *cs,
			   int nvec,
			   char attribute((unused)) **vec) {
  if(nvec < 2) {
    error(0, "%s:%d: should be at least 'player PATTERN MODULE'",
	  cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int validate_allow(const struct config_state *cs,
			  int nvec,
			  char attribute((unused)) **vec) {
  if(nvec != 2) {
    error(0, "%s:%d: must be 'allow NAME PASS'", cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int validate_non_negative(const struct config_state *cs,
				 int nvec, char **vec) {
  long n;

  if(nvec < 1) {
    error(0, "%s:%d: missing argument", cs->path, cs->line);
    return -1;
  }
  if(nvec > 1) {
    error(0, "%s:%d: too many arguments", cs->path, cs->line);
    return -1;
  }
  if(xstrtol(&n, vec[0], 0, 0)) {
    error(0, "%s:%d: %s", cs->path, cs->line, strerror(errno));
    return -1;
  }
  if(n < 0) {
    error(0, "%s:%d: must not be negative", cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int validate_positive(const struct config_state *cs,
			  int nvec, char **vec) {
  long n;

  if(nvec < 1) {
    error(0, "%s:%d: missing argument", cs->path, cs->line);
    return -1;
  }
  if(nvec > 1) {
    error(0, "%s:%d: too many arguments", cs->path, cs->line);
    return -1;
  }
  if(xstrtol(&n, vec[0], 0, 0)) {
    error(0, "%s:%d: %s", cs->path, cs->line, strerror(errno));
    return -1;
  }
  if(n <= 0) {
    error(0, "%s:%d: must be positive", cs->path, cs->line);
    return -1;
  }
  return 0;
}

static int validate_isauser(const struct config_state *cs,
			    int attribute((unused)) nvec,
			    char **vec) {
  struct passwd *pw;

  if(!(pw = getpwnam(vec[0]))) {
    error(0, "%s:%d: no such user as '%s'", cs->path, cs->line, vec[0]);
    return -1;
  }
  return 0;
}

static int validate_sample_format(const struct config_state *cs,
				  int attribute((unused)) nvec,
				  char **vec) {
  return parse_sample_format(cs, 0, nvec, vec);
}

static int validate_channel(const struct config_state *cs,
			    int attribute((unused)) nvec,
			    char **vec) {
  if(mixer_channel(vec[0]) == -1) {
    error(0, "%s:%d: invalid channel '%s'", cs->path, cs->line, vec[0]);
    return -1;
  }
  return 0;
}

static int validate_any(const struct config_state attribute((unused)) *cs,
			int attribute((unused)) nvec,
			char attribute((unused)) **vec) {
  return 0;
}

static int validate_url(const struct config_state attribute((unused)) *cs,
			int attribute((unused)) nvec,
			char **vec) {
  const char *s;
  int n;
  /* absoluteURI   = scheme ":" ( hier_part | opaque_part )
     scheme        = alpha *( alpha | digit | "+" | "-" | "." ) */
  s = vec[0];
  n = strspn(s, ("abcdefghijklmnopqrstuvwxyz"
		 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		 "0123456789"));
  if(s[n] != ':') {
    error(0, "%s:%d: invalid url '%s'", cs->path, cs->line, vec[0]);
    return -1;
  }
  if(!strncmp(s, "http:", 5)
     || !strncmp(s, "https:", 6)) {
    s += n + 1;
    /* we only do a rather cursory check */
    if(strncmp(s, "//", 2)) {
      error(0, "%s:%d: invalid url '%s'", cs->path, cs->line, vec[0]);
      return -1;
    }
  }
  return 0;
}

static int validate_alias(const struct config_state *cs,
			  int nvec,
			  char **vec) {
  const char *s;
  int in_brackets = 0, c;

  if(nvec < 1) {
    error(0, "%s:%d: missing argument", cs->path, cs->line);
    return -1;
  }
  if(nvec > 1) {
    error(0, "%s:%d: too many arguments", cs->path, cs->line);
    return -1;
  }
  s = vec[0];
  while((c = (unsigned char)*s++)) {
    if(in_brackets) {
      if(c == '}')
	in_brackets = 0;
      else if(!isalnum(c)) {
	error(0, "%s:%d: invalid part name in alias expansion in '%s'",
	      cs->path, cs->line, vec[0]);
	  return -1;
      }
    } else {
      if(c == '{') {
	in_brackets = 1;
	if(*s == '/')
	  ++s;
      } else if(c == '\\') {
	if(!(c = (unsigned char)*s++)) {
	  error(0, "%s:%d: unterminated escape in alias expansion in '%s'",
		cs->path, cs->line, vec[0]);
	  return -1;
	} else if(c != '\\' && c != '{') {
	  error(0, "%s:%d: invalid escape in alias expansion in '%s'",
		cs->path, cs->line, vec[0]);
	  return -1;
	}
      }
    }
    ++s;
  }
  if(in_brackets) {
    error(0, "%s:%d: unterminated part name in alias expansion in '%s'",
	  cs->path, cs->line, vec[0]);
    return -1;
  }
  return 0;
}

static int validate_addrport(const struct config_state attribute((unused)) *cs,
			     int nvec,
			     char attribute((unused)) **vec) {
  switch(nvec) {
  case 0:
    error(0, "%s:%d: missing address",
	  cs->path, cs->line);
    return -1;
  case 1:
    error(0, "%s:%d: missing port name/number",
	  cs->path, cs->line);
    return -1;
  case 2:
    return 0;
  default:
    error(0, "%s:%d: expected ADDRESS PORT",
	  cs->path, cs->line);
    return -1;
  }
}

static int validate_address(const struct config_state attribute((unused)) *cs,
			 int nvec,
			 char attribute((unused)) **vec) {
  switch(nvec) {
  case 0:
    error(0, "%s:%d: missing address",
	  cs->path, cs->line);
    return -1;
  case 1:
  case 2:
    return 0;
  default:
    error(0, "%s:%d: expected ADDRESS PORT",
	  cs->path, cs->line);
    return -1;
  }
}

/** @brief Item name and and offset */
#define C(x) #x, offsetof(struct config, x)
/** @brief Item name and and offset */
#define C2(x,y) #x, offsetof(struct config, y)

/** @brief All configuration items */
static const struct conf conf[] = {
  { C(alias),            &type_string,           validate_alias },
  { C(allow),            &type_stringlist_accum, validate_allow },
  { C(broadcast),        &type_stringlist,       validate_addrport },
  { C(broadcast_from),   &type_stringlist,       validate_address },
  { C(channel),          &type_string,           validate_channel },
  { C(checkpoint_kbyte), &type_integer,          validate_non_negative },
  { C(checkpoint_min),   &type_integer,          validate_non_negative },
  { C(collection),       &type_collections,      validate_any },
  { C(connect),          &type_stringlist,       validate_addrport },
  { C(device),           &type_string,           validate_any },
  { C(gap),              &type_integer,          validate_non_negative },
  { C(history),          &type_integer,          validate_positive },
  { C(home),             &type_string,           validate_isdir },
  { C(listen),           &type_stringlist,       validate_addrport },
  { C(lock),             &type_boolean,          validate_any },
  { C(mixer),            &type_string,           validate_ischr },
  { C(namepart),         &type_namepart,         validate_any },
  { C2(nice, nice_rescan), &type_integer,        validate_non_negative },
  { C(nice_rescan),      &type_integer,          validate_non_negative },
  { C(nice_server),      &type_integer,          validate_any },
  { C(nice_speaker),     &type_integer,          validate_any },
  { C(password),         &type_string,           validate_any },
  { C(player),           &type_stringlist_accum, validate_player },
  { C(plugins),          &type_string_accum,     validate_isdir },
  { C(prefsync),         &type_integer,          validate_positive },
  { C(queue_pad),        &type_integer,          validate_positive },
  { C(refresh),          &type_integer,          validate_positive },
  { C2(restrict, restrictions),         &type_restrict,         validate_any },
  { C(sample_format),    &type_sample_format,    validate_sample_format },
  { C(scratch),          &type_string_accum,     validate_isreg },
  { C(signal),           &type_signal,           validate_any },
  { C(sox_generation),   &type_integer,          validate_non_negative },
  { C(speaker_backend),  &type_backend,          validate_any },
  { C(speaker_command),  &type_string,           validate_any },
  { C(stopword),         &type_string_accum,     validate_any },
  { C(templates),        &type_string_accum,     validate_isdir },
  { C(transform),        &type_transform,        validate_any },
  { C(trust),            &type_string_accum,     validate_any },
  { C(url),              &type_string,           validate_url },
  { C(user),             &type_string,           validate_isauser },
  { C(username),         &type_string,           validate_any },
};

/** @brief Find a configuration item's definition by key */
static const struct conf *find(const char *key) {
  int n;

  if((n = TABLE_FIND(conf, struct conf, name, key)) < 0)
    return 0;
  return &conf[n];
}

/** @brief Set a new configuration value */
static int config_set(const struct config_state *cs,
		      int nvec, char **vec) {
  const struct conf *which;

  D(("config_set %s", vec[0]));
  if(!(which = find(vec[0]))) {
    error(0, "%s:%d: unknown configuration key '%s'",
	  cs->path, cs->line, vec[0]);
    return -1;
  }
  return (which->validate(cs, nvec - 1, vec + 1)
	  || which->type->set(cs, which, nvec - 1, vec + 1));
}

/** @brief Error callback used by config_include() */
static void config_error(const char *msg, void *u) {
  const struct config_state *cs = u;

  error(0, "%s:%d: %s", cs->path, cs->line, msg);
}

/** @brief Include a file by name */
static int config_include(struct config *c, const char *path) {
  FILE *fp;
  char *buffer, *inputbuffer, **vec;
  int n, ret = 0;
  struct config_state cs;

  cs.path = path;
  cs.line = 0;
  cs.config = c;
  D(("%s: reading configuration", path));
  if(!(fp = fopen(path, "r"))) {
    error(errno, "error opening %s", path);
    return -1;
  }
  while(!inputline(path, fp, &inputbuffer, '\n')) {
    ++cs.line;
    if(!(buffer = mb2utf8(inputbuffer))) {
      error(errno, "%s:%d: cannot convert to UTF-8", cs.path, cs.line);
      ret = -1;
      xfree(inputbuffer);
      continue;
    }
    xfree(inputbuffer);
    if(!(vec = split(buffer, &n, SPLIT_COMMENTS|SPLIT_QUOTES,
		     config_error, &cs))) {
      ret = -1;
      xfree(buffer);
      continue;
    }
    if(n) {
      if(!strcmp(vec[0], "include")) {
	if(n != 2) {
	  error(0, "%s:%d: must be 'include PATH'", cs.path, cs.line);
	  ret = -1;
	} else
	  config_include(c, vec[1]);
      } else
	ret |= config_set(&cs, n, vec);
    }
    for(n = 0; vec[n]; ++n) xfree(vec[n]);
    xfree(vec);
    xfree(buffer);
  }
  if(ferror(fp)) {
    error(errno, "error reading %s", path);
    ret = -1;
  }
  fclose(fp);
  return ret;
}

/** @brief Make a new default configuration */
static struct config *config_default(void) {
  struct config *c = xmalloc(sizeof *c);
  const char *logname;
  struct passwd *pw;

  /* Strings had better be xstrdup'd as they will get freed at some point. */
  c->gap = 2;
  c->history = 60;
  c->home = xstrdup(pkgstatedir);
  if(!(pw = getpwuid(getuid())))
    fatal(0, "cannot determine our username");
  logname = pw->pw_name;
  c->username = xstrdup(logname);
  c->refresh = 15;
  c->prefsync = 3600;
  c->signal = SIGKILL;
  c->alias = xstrdup("{/artist}{/album}{/title}{ext}");
  c->lock = 1;
  c->device = xstrdup("default");
  c->nice_rescan = 10;
  c->speaker_command = 0;
  c->sample_format.bits = 16;
  c->sample_format.rate = 44100;
  c->sample_format.channels = 2;
  c->sample_format.byte_format = AO_FMT_NATIVE;
  c->queue_pad = 10;
  c->speaker_backend = -1;
  return c;
}

static char *get_file(struct config *c, const char *name) {
  char *s;

  byte_xasprintf(&s, "%s/%s", c->home, name);
  return s;
}

/** @brief Set the default configuration file */
static void set_configfile(void) {
  if(!configfile)
    byte_xasprintf(&configfile, "%s/config", pkgconfdir);
}

/** @brief Free a configuration object */
static void config_free(struct config *c) {
  int n;

  if(c) {
    for(n = 0; n < (int)(sizeof conf / sizeof *conf); ++n)
      conf[n].type->free(c, &conf[n]);
    for(n = 0; n < c->nparts; ++n)
      xfree(c->parts[n]);
    xfree(c->parts);
    xfree(c);
  }
}

/** @brief Set post-parse defaults */
static void config_postdefaults(struct config *c) {
  struct config_state cs;
  const struct conf *whoami;
  int n;

  static const char *namepart[][4] = {
    { "title",  "/([0-9]+:)?([^/]+)\\.[a-zA-Z0-9]+$", "$2", "display" },
    { "title",  "/([^/]+)\\.[a-zA-Z0-9]+$",           "$1", "sort" },
    { "album",  "/([^/]+)/[^/]+$",                    "$1", "*" },
    { "artist", "/([^/]+)/[^/]+/[^/]+$",              "$1", "*" },
    { "ext",    "(\\.[a-zA-Z0-9]+)$",                 "$1", "*" },
  };
#define NNAMEPART (int)(sizeof namepart / sizeof *namepart)

  static const char *transform[][5] = {
    { "track", "^.*/([0-9]+:)?([^/]+)\\.[a-zA-Z0-9]+$", "$2", "display", "" },
    { "track", "^.*/([^/]+)\\.[a-zA-Z0-9]+$",           "$1", "sort", "" },
    { "dir",   "^.*/([^/]+)$",                          "$1", "*", "" },
    { "dir",   "^(the) ([^/]*)",                        "$2, $1", "sort", "i", },
    { "dir",   "[[:punct:]]",                           "", "sort", "g", }
  };
#define NTRANSFORM (int)(sizeof transform / sizeof *transform)

  cs.path = "<internal>";
  cs.line = 0;
  cs.config = c;
  if(!c->namepart.n) {
    whoami = find("namepart");
    for(n = 0; n < NNAMEPART; ++n)
      set_namepart(&cs, whoami, 4, (char **)namepart[n]);
  }
  if(!c->transform.n) {
    whoami = find("transform");
    for(n = 0; n < NTRANSFORM; ++n)
      set_transform(&cs, whoami, 5, (char **)transform[n]);
  }
  if(c->speaker_backend == -1) {
    if(c->speaker_command)
      c->speaker_backend = BACKEND_COMMAND;
    else if(c->broadcast.n)
      c->speaker_backend = BACKEND_NETWORK;
    else {
#if API_ALSA
      c->speaker_backend = BACKEND_ALSA;
#else
      c->speaker_backend = BACKEND_COMMAND;
#endif
    }
  }
  if(c->speaker_backend == BACKEND_COMMAND && !c->speaker_command)
    fatal(0, "speaker_backend is command but speaker_command is not set");
  if(c->speaker_backend == BACKEND_NETWORK && !c->broadcast.n)
    fatal(0, "speaker_backend is network but broadcast is not set");
}

/** @brief (Re-)read the config file */
int config_read() {
  struct config *c;
  char *privconf;
  struct passwd *pw;

  set_configfile();
  c = config_default();
  /* standalone Disobedience installs might not have a global config file */
  if(access(configfile, F_OK) == 0)
    if(config_include(c, configfile))
      return -1;
  /* if we can read the private config file, do */
  if((privconf = config_private())
     && access(privconf, R_OK) == 0
     && config_include(c, privconf))
    return -1;
  xfree(privconf);
  /* if there's a per-user system config file for this user, read it */
  if(!(pw = getpwuid(getuid())))
    fatal(0, "cannot determine our username");
  if((privconf = config_usersysconf(pw))
     && access(privconf, F_OK) == 0
     && config_include(c, privconf))
      return -1;
  xfree(privconf);
  /* if we have a password file, read it */
  if((privconf = config_userconf(getenv("HOME"), pw))
     && access(privconf, F_OK) == 0
     && config_include(c, privconf))
    return -1;
  xfree(privconf);
  /* install default namepart and transform settings */
  config_postdefaults(c);
  /* everything is good so we shall use the new config */
  config_free(config);
  config = c;
  return 0;
}

/** @brief Return the path to the private configuration file */
char *config_private(void) {
  char *s;

  set_configfile();
  byte_xasprintf(&s, "%s.private", configfile);
  return s;
}

/** @brief Return the path to user's personal configuration file */
char *config_userconf(const char *home, const struct passwd *pw) {
  char *s;

  byte_xasprintf(&s, "%s/.disorder/passwd", home ? home : pw->pw_dir);
  return s;
}

/** @brief Return the path to user-specific system configuration */
char *config_usersysconf(const struct passwd *pw) {
  char *s;

  set_configfile();
  if(!strchr(pw->pw_name, '/')) {
    byte_xasprintf(&s, "%s.%s", configfile, pw->pw_name);
    return s;
  } else
    return 0;
}

char *config_get_file(const char *name) {
  return get_file(config, name);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
