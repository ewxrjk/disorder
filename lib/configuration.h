/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2006 Richard Kettlewell
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

struct real_pcre;

/* Configuration is kept in a @struct config@; the live configuration
 * is always pointed to by @config@.  Values in @config@ are UTF-8 encoded.
 */

struct stringlist {
  int n;
  char **s;
};

struct stringlistlist {
  int n;
  struct stringlist *s;
};

struct collection {
  char *module;
  char *encoding;
  char *root;
};

struct collectionlist {
  int n;
  struct collection *s;
};

struct namepart {
  char *part;				/* part */
  struct real_pcre *re;			/* regexp */
  char *replace;			/* replacement string */
  char *context;			/* context glob */
  unsigned reflags;			/* regexp flags */
};

struct namepartlist {
  int n;
  struct namepart *s;
};

struct transform {
  char *type;				/* track or dir */
  char *context;			/* sort or choose */
  char *replace;			/* substitution string */
  struct real_pcre *re;			/* compiled re */
  unsigned flags;			/* regexp flags */
};

struct transformlist {
  int n;
  struct transform *t;
};

struct config {
  /* server config */
  struct stringlistlist player;		/* players */
  struct stringlistlist allow;		/* allowed users */
  struct stringlist scratch;		/* scratch tracks */
  long gap;				/* gap between tracks */
  long history;				/* length of history */
  struct stringlist trust;		/* trusted users */
  const char *user;			/* user to run as */
  long nice_rescan;			/* rescan subprocess niceness */
  struct stringlist plugins;		/* plugin path */
  struct stringlist stopword;		/* stopwords for track search */
  struct collectionlist collection;	/* track collections */
  long checkpoint_kbyte;
  long checkpoint_min;
  char *mixer;				/* mixer device file */
  char *channel;			/* mixer channel */
  long prefsync;			/* preflog sync intreval */
  struct stringlist listen;		/* secondary listen address */
  const char *alias;			/* alias format */
  int lock;				/* server takes a lock */
  long nice_server;			/* nice value for server */
  long nice_speaker;			/* nice value for speaker */
  /* shared client/server config */
  const char *home;			/* home directory for state files */
  /* client config */
  const char *username, *password;	/* our own username and password */
  struct stringlist connect;		/* connect address */
  /* web config */
  struct stringlist templates;		/* template path */
  const char *url;			/* canonical URL */
  long refresh;				/* maximum refresh period */
  unsigned restrictions;		/* restrictions */
#define RESTRICT_SCRATCH 1
#define RESTRICT_REMOVE 2
#define RESTRICT_MOVE 4
  struct namepartlist namepart;		/* transformations */
  int signal;				/* termination signal */
  const char *device;			/* ALSA output device */
  struct transformlist transform;	/* path name transformations */

  /* derived values: */
  int nparts;				/* number of distinct name parts */
  char **parts;				/* name part list  */
};

extern struct config *config;
/* the current configuration */

int config_read(void);
/* re-read config, return 0 on success or non-0 on error.
 * Only updates @config@ if the new configuration is valid. */

char *config_get_file(const char *name);
/* get a filename within the home directory */

struct passwd;

char *config_userconf(const char *home, const struct passwd *pw);
/* get the user's own private conffile, assuming their home dir is
 * @home@ if not null and using @pw@ otherwise */

char *config_usersysconf(const struct passwd *pw );
/* get the user's conffile in /etc */

char *config_private(void);
/* get the private config file */

extern char *configfile;

#endif /* CONFIGURATION_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
