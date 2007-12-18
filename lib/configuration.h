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
/** @file lib/configuration.h
 * @brief Configuration file support
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "speaker-protocol.h"

struct real_pcre;

/* Configuration is kept in a @struct config@; the live configuration
 * is always pointed to by @config@.  Values in @config@ are UTF-8 encoded.
 */

/** @brief A list of strings */
struct stringlist {
  /** @brief Number of strings */
  int n;
  /** @brief Array of strings */
  char **s;
};

/** @brief A list of list of strings */
struct stringlistlist {
  /** @brief Number of string lists */
  int n;
  /** @brief Array of string lists */
  struct stringlist *s;
};

/** @brief A collection of tracks */
struct collection {
  /** @brief Module that supports this collection */
  char *module;
  /** @brief Filename encoding */
  char *encoding;
  /** @brief Root directory */
  char *root;
};

/** @brief A list of collections */
struct collectionlist {
  /** @brief Number of collections */
  int n;
  /** @brief Array of collections */
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

/** @brief System configuration */
struct config {
  /* server config */

  /** @brief Authorization algorithm */
  char *authorization_algorithm;
  
  /** @brief All players */
  struct stringlistlist player;

  /** @brief All tracklength plugins */
  struct stringlistlist tracklength;

  /** @brief Allowed users */
  struct stringlistlist allow;

  /** @brief Scratch tracks */
  struct stringlist scratch;

  /** @brief Gap between tracks in seconds */
  long gap;

  /** @brief Maximum number of recent tracks to record in history */
  long history;

  /** @brief Expiry limit for noticed.db */
  long noticed_history;
  
  /** @brief Trusted users */
  struct stringlist trust;

  /** @brief User for server to run as */
  const char *user;

  /** @brief Nice value for rescan subprocess */
  long nice_rescan;

  /** @brief Paths to search for plugins */
  struct stringlist plugins;

  /** @brief List of stopwords */
  struct stringlist stopword;

  /** @brief List of collections */
  struct collectionlist collection;

  /** @brief Database checkpoint byte limit */
  long checkpoint_kbyte;

  /** @brief Databsase checkpoint minimum */
  long checkpoint_min;

  /** @brief Path to mixer device */
  char *mixer;

  /** @brief Mixer channel to use */
  char *channel;

  long prefsync;			/* preflog sync interval */

  /** @brief Secondary listen address */
  struct stringlist listen;

  /** @brief Alias format string */
  const char *alias;

  /** @brief Enable server locking */
  int lock;

  /** @brief Nice value for server */
  long nice_server;

  /** @brief Nice value for speaker */
  long nice_speaker;

  /** @brief Command execute by speaker to play audio */
  const char *speaker_command;

  /** @brief Target sample format */
  struct stream_header sample_format;

  /** @brief Sox syntax generation */
  long sox_generation;

  /** @brief Speaker backend
   *
   * Choices are @ref BACKEND_ALSA, @ref BACKEND_COMMAND or @ref
   * BACKEND_NETWORK.
   */
  int speaker_backend;
#define BACKEND_ALSA 0			/**< Use ALSA (Linux only) */
#define BACKEND_COMMAND 1		/**< Execute a command */
#define BACKEND_NETWORK 2		/**< Transmit RTP  */
#define BACKEND_COREAUDIO 3		/**< Use Core Audio (Mac only) */
#define BACKEND_OSS 4		        /**< Use OSS */

  /** @brief Home directory for state files */
  const char *home;

  /** @brief Login username */
  const char *username;

  /** @brief Login password */
  const char *password;

  /** @brief Address to connect to */
  struct stringlist connect;

  /** @brief Directories to search for web templates */
  struct stringlist templates;

  /** @brief Canonical URL of web interface */
  const char *url;

  /** @brief Short display limit */
  long short_display;

  /** @brief Maximum refresh interval for web interface (seconds) */
  long refresh;

  /** @brief Facilities restricted to trusted users
   *
   * A bitmap of @ref RESTRICT_SCRATCH, @ref RESTRICT_REMOVE and @ref
   * RESTRICT_MOVE.
   */
  unsigned restrictions;		/* restrictions */
#define RESTRICT_SCRATCH 1		/**< Restrict scratching */
#define RESTRICT_REMOVE 2		/**< Restrict removal */
#define RESTRICT_MOVE 4			/**< Restrict rearrangement */

  /** @brief Target queue length */
  long queue_pad;

  struct namepartlist namepart;		/* transformations */

  /** @brief Termination signal for subprocesses */
  int signal;

  /** @brief ALSA output device */
  const char *device;
  struct transformlist transform;	/* path name transformations */

  /** @brief Address to send audio data to */
  struct stringlist broadcast;

  /** @brief Source address for network audio transmission */
  struct stringlist broadcast_from;

  /** @brief TTL for multicast packets */
  long multicast_ttl;

  /** @brief Whether to loop back multicast packets */
  int multicast_loop;

  /** @brief Login lifetime in seconds */
  long cookie_login_lifetime;

  /** @brief Signing key lifetime in seconds */
  long cookie_key_lifetime;
  
  /* derived values: */
  int nparts;				/* number of distinct name parts */
  char **parts;				/* name part list  */

  /* undocumented, for testing only */
  long dbversion;
};

extern struct config *config;
/* the current configuration */

int config_read(int server);
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
extern int config_per_user;

#endif /* CONFIGURATION_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
