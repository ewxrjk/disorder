/*
 * This file is part of DisOrder.
 * Copyright (C) 2004-2008 Richard Kettlewell
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
/** @file lib/client.c
 * @brief Simple C client
 *
 * See @ref lib/eclient.c for an asynchronous-capable client
 * implementation.
 */

#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <pcre.h>

#include "log.h"
#include "mem.h"
#include "queue.h"
#include "client.h"
#include "charset.h"
#include "hex.h"
#include "split.h"
#include "vector.h"
#include "inputline.h"
#include "kvp.h"
#include "syscalls.h"
#include "printf.h"
#include "sink.h"
#include "addr.h"
#include "authhash.h"
#include "client-common.h"
#include "rights.h"
#include "trackdb.h"
#include "kvp.h"

/** @brief Client handle contents */
struct disorder_client {
  /** @brief Stream to read from */
  FILE *fpin;
  /** @brief Stream to write to */
  FILE *fpout;
  /** @brief Peer description */
  char *ident;
  /** @brief Username */
  char *user;
  /** @brief Report errors to @c stderr */
  int verbose;
  /** @brief Last error string */
  const char *last;
};

/** @brief Create a new client
 * @param verbose If nonzero, write extra junk to stderr
 * @return Pointer to new client
 *
 * You must call disorder_connect(), disorder_connect_user() or
 * disorder_connect_cookie() to connect it.  Use disorder_close() to
 * dispose of the client when finished with it.
 */
disorder_client *disorder_new(int verbose) {
  disorder_client *c = xmalloc(sizeof (struct disorder_client));

  c->verbose = verbose;
  return c;
}

/** @brief Read a response line
 * @param c Client
 * @param rp Where to store response, or NULL (UTF-8)
 * @return Response code 0-999 or -1 on error
 */
static int response(disorder_client *c, char **rp) {
  char *r;

  if(inputline(c->ident, c->fpin, &r, '\n')) {
    byte_xasprintf((char **)&c->last, "input error: %s", strerror(errno));
    return -1;
  }
  D(("response: %s", r));
  if(rp)
    *rp = r;
  if(r[0] >= '0' && r[0] <= '9'
     && r[1] >= '0' && r[1] <= '9'
     && r[2] >= '0' && r[2] <= '9'
     && r[3] == ' ') {
    c->last = r + 4;
    return (r[0] * 10 + r[1]) * 10 + r[2] - 111 * '0';
  } else {
    c->last = "invalid reply format";
    error(0, "invalid reply format from %s", c->ident);
    return -1;
  }
}

/** @brief Return last response string
 * @param c Client
 * @return Last response string (UTF-8, English) or NULL
 */
const char *disorder_last(disorder_client *c) {
  return c->last;
}

/** @brief Read and partially parse a response
 * @param c Client
 * @param rp Where to store response text (or NULL) (UTF-8)
 * @return 0 on success, non-0 on error
 *
 * 5xx responses count as errors.
 *
 * @p rp will NOT be filled in for xx9 responses (where it is just
 * commentary for a command where it would normally be meaningful).
 *
 * NB that the response will NOT be converted to the local encoding.
 */
static int check_response(disorder_client *c, char **rp) {
  int rc;
  char *r;

  if((rc = response(c, &r)) == -1)
    return -1;
  else if(rc / 100 == 2) {
    if(rp)
      *rp = (rc % 10 == 9) ? 0 : xstrdup(r + 4);
    return 0;
  } else {
    if(c->verbose)
      error(0, "from %s: %s", c->ident, utf82mb(r));
    return rc;
  }
}

/** @brief Issue a command and parse a simple response
 * @param c Client
 * @param rp Where to store result, or NULL
 * @param cmd Command
 * @param body Body or NULL
 * @param nbody Length of body or -1
 * @param ap Arguments (UTF-8), terminated by (char *)0
 * @return 0 on success, non-0 on error
 *
 * 5xx responses count as errors.
 *
 * @p rp will NOT be filled in for xx9 responses (where it is just
 * commentary for a command where it would normally be meaningful).
 *
 * NB that the response will NOT be converted to the local encoding
 * nor will quotes be stripped.  See dequote().
 *
 * If @p body is not NULL then the body is sent immediately after the
 * command.  @p nbody should be the number of lines or @c -1 to count
 * them if @p body is NULL-terminated.
 *
 * Usually you would call this via one of the following interfaces:
 * - disorder_simple()
 * - disorder_simple_body()
 * - disorder_simple_list()
 */
static int disorder_simple_v(disorder_client *c,
			     char **rp,
			     const char *cmd,
                             char **body, int nbody,
                             va_list ap) {
  const char *arg;
  struct dynstr d;

  if(!c->fpout) {
    c->last = "not connected";
    error(0, "not connected to server");
    return -1;
  }
  if(cmd) {
    dynstr_init(&d);
    dynstr_append_string(&d, cmd);
    while((arg = va_arg(ap, const char *))) {
      dynstr_append(&d, ' ');
      dynstr_append_string(&d, quoteutf8(arg));
    }
    dynstr_append(&d, '\n');
    dynstr_terminate(&d);
    D(("command: %s", d.vec));
    if(fputs(d.vec, c->fpout) < 0)
      goto write_error;
    if(body) {
      if(nbody < 0)
        for(nbody = 0; body[nbody]; ++nbody)
          ;
      for(int n = 0; n < nbody; ++n) {
        if(body[n][0] == '.')
          if(fputc('.', c->fpout) < 0)
            goto write_error;
        if(fputs(body[n], c->fpout) < 0)
          goto write_error;
        if(fputc('\n', c->fpout) < 0)
          goto write_error;
      }
      if(fputs(".\n", c->fpout) < 0)
        goto write_error;
    }
    if(fflush(c->fpout))
      goto write_error;
  }
  return check_response(c, rp);
write_error:
  byte_xasprintf((char **)&c->last, "write error: %s", strerror(errno));
  error(errno, "error writing to %s", c->ident);
  return -1;
}

/** @brief Issue a command and parse a simple response
 * @param c Client
 * @param rp Where to store result, or NULL (UTF-8)
 * @param cmd Command
 * @return 0 on success, non-0 on error
 *
 * The remaining arguments are command arguments, terminated by (char
 * *)0.  They should be in UTF-8.
 *
 * 5xx responses count as errors.
 *
 * @p rp will NOT be filled in for xx9 responses (where it is just
 * commentary for a command where it would normally be meaningful).
 *
 * NB that the response will NOT be converted to the local encoding
 * nor will quotes be stripped.  See dequote().
 */
static int disorder_simple(disorder_client *c,
			   char **rp,
			   const char *cmd, ...) {
  va_list ap;
  int ret;

  va_start(ap, cmd);
  ret = disorder_simple_v(c, rp, cmd, 0, 0, ap);
  va_end(ap);
  return ret;
}

/** @brief Issue a command with a body and parse a simple response
 * @param c Client
 * @param rp Where to store result, or NULL (UTF-8)
 * @param body Pointer to body
 * @param nbody Size of body
 * @param cmd Command
 * @return 0 on success, non-0 on error
 *
 * See disorder_simple().
 */
static int disorder_simple_body(disorder_client *c,
                                char **rp,
                                char **body, int nbody,
                                const char *cmd, ...) {
  va_list ap;
  int ret;

  va_start(ap, cmd);
  ret = disorder_simple_v(c, rp, cmd, body, nbody, ap);
  va_end(ap);
  return ret;
}

/** @brief Dequote a result string
 * @param rc 0 on success, non-0 on error
 * @param rp Where result string is stored (UTF-8)
 * @return @p rc
 *
 * This is used as a wrapper around disorder_simple() to dequote
 * results in place.
 */
static int dequote(int rc, char **rp) {
  char **rr;

  if(!rc) {
    if((rr = split(*rp, 0, SPLIT_QUOTES, 0, 0)) && *rr) {
      *rp = *rr;
      return 0;
    }
    error(0, "invalid reply: %s", *rp);
  }
  return rc;
}

/** @brief Generic connection routine
 * @param conf Configuration to follow
 * @param c Client
 * @param username Username to log in with or NULL
 * @param password Password to log in with or NULL
 * @param cookie Cookie to log in with or NULL
 * @return 0 on success, non-0 on error
 *
 * @p cookie is tried first if not NULL.  If it is NULL then @p
 * username must not be.  If @p username is not NULL then nor may @p
 * password be.
 */
int disorder_connect_generic(struct config *conf,
                             disorder_client *c,
                             const char *username,
                             const char *password,
                             const char *cookie) {
  int fd = -1, fd2 = -1, nrvec, rc;
  unsigned char *nonce;
  size_t nl;
  const char *res;
  char *r, **rvec;
  const char *protocol, *algorithm, *challenge;
  struct sockaddr *sa;
  socklen_t salen;

  if((salen = find_server(conf, &sa, &c->ident)) == (socklen_t)-1)
    return -1;
  c->fpin = c->fpout = 0;
  if((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0) {
    byte_xasprintf((char **)&c->last, "socket: %s", strerror(errno));
    error(errno, "error calling socket");
    return -1;
  }
  if(connect(fd, sa, salen) < 0) {
    byte_xasprintf((char **)&c->last, "connect: %s", strerror(errno));
    error(errno, "error calling connect");
    goto error;
  }
  if((fd2 = dup(fd)) < 0) {
    byte_xasprintf((char **)&c->last, "dup: %s", strerror(errno));
    error(errno, "error calling dup");
    goto error;
  }
  if(!(c->fpin = fdopen(fd, "rb"))) {
    byte_xasprintf((char **)&c->last, "fdopen: %s", strerror(errno));
    error(errno, "error calling fdopen");
    goto error;
  }
  fd = -1;
  if(!(c->fpout = fdopen(fd2, "wb"))) {
    byte_xasprintf((char **)&c->last, "fdopen: %s", strerror(errno));
    error(errno, "error calling fdopen");
    goto error;
  }
  fd2 = -1;
  if((rc = disorder_simple(c, &r, 0, (const char *)0)))
    goto error_rc;
  if(!(rvec = split(r, &nrvec, SPLIT_QUOTES, 0, 0)))
    goto error;
  if(nrvec != 3) {
    c->last = "cannot parse server greeting";
    error(0, "cannot parse server greeting %s", r);
    goto error;
  }
  protocol = *rvec++;
  if(strcmp(protocol, "2")) {
    c->last = "unknown protocol version";
    error(0, "unknown protocol version: %s", protocol);
    goto error;
  }
  algorithm = *rvec++;
  challenge = *rvec++;
  if(!(nonce = unhex(challenge, &nl)))
    goto error;
  if(cookie) {
    if(!dequote(disorder_simple(c, &c->user, "cookie", cookie, (char *)0),
		&c->user))
      return 0;				/* success */
    if(!username) {
      c->last = "cookie failed and no username";
      error(0, "cookie did not work and no username available");
      goto error;
    }
  }
  if(!(res = authhash(nonce, nl, password, algorithm))) {
    c->last = "error computing authorization hash";
    goto error;
  }
  if((rc = disorder_simple(c, 0, "user", username, res, (char *)0)))
    goto error_rc;
  c->user = xstrdup(username);
  return 0;
error:
  rc = -1;
error_rc:
  if(c->fpin) {
    fclose(c->fpin);
    c->fpin = 0;
  }
  if(c->fpout) {
    fclose(c->fpout);
    c->fpout = 0;
  }
  if(fd2 != -1) close(fd2);
  if(fd != -1) close(fd);
  return rc;
}

/** @brief Connect a client with a specified username and password
 * @param c Client
 * @param username Username to log in with
 * @param password Password to log in with
 * @return 0 on success, non-0 on error
 */
int disorder_connect_user(disorder_client *c,
			  const char *username,
			  const char *password) {
  return disorder_connect_generic(config,
                                  c,
				  username,
				  password,
				  0);
}

/** @brief Connect a client
 * @param c Client
 * @return 0 on success, non-0 on error
 *
 * The connection will use the username and password found in @ref
 * config, or directly from the database if no password is found and
 * the database is readable (usually only for root).
 */
int disorder_connect(disorder_client *c) {
  const char *username, *password;

  if(!(username = config->username)) {
    c->last = "no username";
    error(0, "no username configured");
    return -1;
  }
  password = config->password;
  /* Maybe we can read the database */
  if(!password && trackdb_readable()) {
    trackdb_init(TRACKDB_NO_RECOVER|TRACKDB_NO_UPGRADE);
    trackdb_open(TRACKDB_READ_ONLY);
    password = trackdb_get_password(username);
    trackdb_close();
  }
  if(!password) {
    /* Oh well */
    c->last = "no password";
    error(0, "no password configured");
    return -1;
  }
  return disorder_connect_generic(config,
                                  c,
				  username,
				  password,
				  0);
}

/** @brief Connect a client
 * @param c Client
 * @param cookie Cookie to log in with, or NULL
 * @return 0 on success, non-0 on error
 *
 * If @p cookie is NULL or does not work then we attempt to log in as
 * guest instead (so when the cookie expires only an extra round trip
 * is needed rathre than a complete new login).
 */
int disorder_connect_cookie(disorder_client *c,
			    const char *cookie) {
  return disorder_connect_generic(config,
                                  c,
				  "guest",
				  "",
				  cookie);
}

/** @brief Close a client
 * @param c Client
 * @return 0 on succcess, non-0 on errior
 *
 * The client is still closed even on error.  It might well be
 * appropriate to ignore the return value.
 */
int disorder_close(disorder_client *c) {
  int ret = 0;

  if(c->fpin) {
    if(fclose(c->fpin) < 0) {
      byte_xasprintf((char **)&c->last, "fclose: %s", strerror(errno));
      error(errno, "error calling fclose");
      ret = -1;
    }
    c->fpin = 0;
  }
  if(c->fpout) {
    if(fclose(c->fpout) < 0) {
      byte_xasprintf((char **)&c->last, "fclose: %s", strerror(errno));
      error(errno, "error calling fclose");
      ret = -1;
    }
    c->fpout = 0;
  }
  c->ident = 0;
  c->user = 0;
  return 0;
}

/** @brief Play a track
 * @param c Client
 * @param track Track to play (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_play(disorder_client *c, const char *track) {
  return disorder_simple(c, 0, "play", track, (char *)0);
}

/** @brief Remove a track
 * @param c Client
 * @param track Track to remove (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_remove(disorder_client *c, const char *track) {
  return disorder_simple(c, 0, "remove", track, (char *)0);
}

/** @brief Move a track
 * @param c Client
 * @param track Track to move (UTF-8)
 * @param delta Distance to move by
 * @return 0 on success, non-0 on error
 */
int disorder_move(disorder_client *c, const char *track, int delta) {
  char d[16];

  byte_snprintf(d, sizeof d, "%d", delta);
  return disorder_simple(c, 0, "move", track, d, (char *)0);
}

/** @brief Enable play
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_enable(disorder_client *c) {
  return disorder_simple(c, 0, "enable", (char *)0);
}

/** @brief Disable play
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_disable(disorder_client *c) {
  return disorder_simple(c, 0, "disable", (char *)0);
}

/** @brief Scratch the currently playing track
 * @param id Playing track ID or NULL (UTF-8)
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_scratch(disorder_client *c, const char *id) {
  return disorder_simple(c, 0, "scratch", id, (char *)0);
}

/** @brief Shut down the server
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_shutdown(disorder_client *c) {
  return disorder_simple(c, 0, "shutdown", (char *)0);
}

/** @brief Make the server re-read its configuration
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_reconfigure(disorder_client *c) {
  return disorder_simple(c, 0, "reconfigure", (char *)0);
}

/** @brief Rescan tracks
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_rescan(disorder_client *c) {
  return disorder_simple(c, 0, "rescan", (char *)0);
}

/** @brief Get server version number
 * @param c Client
 * @param rp Where to store version string (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_version(disorder_client *c, char **rp) {
  return dequote(disorder_simple(c, rp, "version", (char *)0), rp);
}

static void client_error(const char *msg,
			 void attribute((unused)) *u) {
  error(0, "error parsing reply: %s", msg);
}

/** @brief Get currently playing track
 * @param c Client
 * @param qp Where to store track information
 * @return 0 on success, non-0 on error
 *
 * @p qp gets NULL if no track is playing.
 */
int disorder_playing(disorder_client *c, struct queue_entry **qp) {
  char *r;
  struct queue_entry *q;
  int rc;

  if((rc = disorder_simple(c, &r, "playing", (char *)0)))
    return rc;
  if(r) {
    q = xmalloc(sizeof *q);
    if(queue_unmarshall(q, r, client_error, 0))
      return -1;
    *qp = q;
  } else
    *qp = 0;
  return 0;
}

/** @brief Fetch the queue, recent list, etc */
static int disorder_somequeue(disorder_client *c,
			      const char *cmd, struct queue_entry **qp) {
  struct queue_entry *qh, **qt = &qh, *q;
  char *l;
  int rc;

  if((rc = disorder_simple(c, 0, cmd, (char *)0)))
    return rc;
  while(inputline(c->ident, c->fpin, &l, '\n') >= 0) {
    if(!strcmp(l, ".")) {
      *qt = 0;
      *qp = qh;
      return 0;
    }
    q = xmalloc(sizeof *q);
    if(!queue_unmarshall(q, l, client_error, 0)) {
      *qt = q;
      qt = &q->next;
    }
  }
  if(ferror(c->fpin)) {
    byte_xasprintf((char **)&c->last, "input error: %s", strerror(errno));
    error(errno, "error reading %s", c->ident);
  } else {
    c->last = "input error: unexpxected EOF";
    error(0, "error reading %s: unexpected EOF", c->ident);
  }
  return -1;
}

/** @brief Get recently played tracks
 * @param c Client
 * @param qp Where to store track information
 * @return 0 on success, non-0 on error
 *
 * The last entry in the list is the most recently played track.
 */
int disorder_recent(disorder_client *c, struct queue_entry **qp) {
  return disorder_somequeue(c, "recent", qp);
}

/** @brief Get queue
 * @param c Client
 * @param qp Where to store track information
 * @return 0 on success, non-0 on error
 *
 * The first entry in the list will be played next.
 */
int disorder_queue(disorder_client *c, struct queue_entry **qp) {
  return disorder_somequeue(c, "queue", qp);
}

/** @brief Read a dot-stuffed list
 * @param c Client
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 *
 * The list will have a final NULL not counted in @p nvecp.
 */
static int readlist(disorder_client *c, char ***vecp, int *nvecp) {
  char *l;
  struct vector v;

  vector_init(&v);
  while(inputline(c->ident, c->fpin, &l, '\n') >= 0) {
    if(!strcmp(l, ".")) {
      vector_terminate(&v);
      if(nvecp)
	*nvecp = v.nvec;
      *vecp = v.vec;
      return 0;
    }
    vector_append(&v, l + (*l == '.'));
  }
  if(ferror(c->fpin)) {
    byte_xasprintf((char **)&c->last, "input error: %s", strerror(errno));
    error(errno, "error reading %s", c->ident);
  } else {
    c->last = "input error: unexpxected EOF";
    error(0, "error reading %s: unexpected EOF", c->ident);
  }
  return -1;
}

/** @brief Issue a comamnd and get a list response
 * @param c Client
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @param cmd Command
 * @return 0 on success, non-0 on error
 *
 * The remaining arguments are command arguments, terminated by (char
 * *)0.  They should be in UTF-8.
 *
 * 5xx responses count as errors.
 *
 * See disorder_simple().
 */
static int disorder_simple_list(disorder_client *c,
				char ***vecp, int *nvecp,
				const char *cmd, ...) {
  va_list ap;
  int ret;

  va_start(ap, cmd);
  ret = disorder_simple_v(c, 0, cmd, 0, 0, ap);
  va_end(ap);
  if(ret) return ret;
  return readlist(c, vecp, nvecp);
}

/** @brief List directories below @p dir
 * @param c Client
 * @param dir Directory to list, or NULL for root (UTF-8)
 * @param re Regexp that results must match, or NULL (UTF-8)
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_directories(disorder_client *c, const char *dir, const char *re,
			 char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "dirs", dir, re, (char *)0);
}

/** @brief List files below @p dir
 * @param c Client
 * @param dir Directory to list, or NULL for root (UTF-8)
 * @param re Regexp that results must match, or NULL (UTF-8)
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_files(disorder_client *c, const char *dir, const char *re,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "files", dir, re, (char *)0);
}

/** @brief List files and directories below @p dir
 * @param c Client
 * @param dir Directory to list, or NULL for root (UTF-8)
 * @param re Regexp that results must match, or NULL (UTF-8)
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_allfiles(disorder_client *c, const char *dir, const char *re,
		      char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "allfiles", dir, re, (char *)0);
}

/** @brief Return the user we logged in with
 * @param c Client
 * @return User name (owned by @p c, don't modify)
 */
char *disorder_user(disorder_client *c) {
  return c->user;
}

/** @brief Set a track preference
 * @param c Client
 * @param track Track name (UTF-8)
 * @param key Preference name (UTF-8)
 * @param value Preference value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_set(disorder_client *c, const char *track,
		 const char *key, const char *value) {
  return disorder_simple(c, 0, "set", track, key, value, (char *)0);
}

/** @brief Unset a track preference
 * @param c Client
 * @param track Track name (UTF-8)
 * @param key Preference name (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_unset(disorder_client *c, const char *track,
		   const char *key) {
  return disorder_simple(c, 0, "unset", track, key, (char *)0);
}

/** @brief Get a track preference
 * @param c Client
 * @param track Track name (UTF-8)
 * @param key Preference name (UTF-8)
 * @param valuep Where to store preference value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_get(disorder_client *c,
		 const char *track, const char *key, char **valuep) {
  return dequote(disorder_simple(c, valuep, "get", track, key, (char *)0),
		 valuep);
}

static void pref_error_handler(const char *msg,
			       void attribute((unused)) *u) {
  error(0, "error handling 'prefs' reply: %s", msg);
}

/** @brief Get all preferences for a trcak
 * @param c Client
 * @param track Track name
 * @param kp Where to store linked list of preferences
 * @return 0 on success, non-0 on error
 */
int disorder_prefs(disorder_client *c, const char *track, struct kvp **kp) {
  char **vec, **pvec;
  int nvec, npvec, n, rc;
  struct kvp *k;

  if((rc = disorder_simple_list(c, &vec, &nvec, "prefs", track, (char *)0)))
    return rc;
  for(n = 0; n < nvec; ++n) {
    if(!(pvec = split(vec[n], &npvec, SPLIT_QUOTES, pref_error_handler, 0)))
      return -1;
    if(npvec != 2) {
      pref_error_handler("malformed response", 0);
      return -1;
    }
    *kp = k = xmalloc(sizeof *k);
    k->name = pvec[0];
    k->value = pvec[1];
    kp = &k->next;
  }
  *kp = 0;
  return 0;
}

/** @brief Parse a boolean response
 * @param cmd Command for use in error messsage
 * @param value Result from server
 * @param flagp Where to store result
 * @return 0 on success, non-0 on error
 */
static int boolean(const char *cmd, const char *value,
		   int *flagp) {
  if(!strcmp(value, "yes")) *flagp = 1;
  else if(!strcmp(value, "no")) *flagp = 0;
  else {
    error(0, "malformed response to '%s'", cmd);
    return -1;
  }
  return 0;
}

/** @brief Test whether a track exists
 * @param c Client
 * @param track Track name (UTF-8)
 * @param existsp Where to store result (non-0 iff does exist)
 * @return 0 on success, non-0 on error
 */
int disorder_exists(disorder_client *c, const char *track, int *existsp) {
  char *v;
  int rc;

  if((rc = disorder_simple(c, &v, "exists", track, (char *)0)))
    return rc;
  return boolean("exists", v, existsp);
}

/** @brief Test whether playing is enabled
 * @param c Client
 * @param enabledp Where to store result (non-0 iff enabled)
 * @return 0 on success, non-0 on error
 */
int disorder_enabled(disorder_client *c, int *enabledp) {
  char *v;
  int rc;

  if((rc = disorder_simple(c, &v, "enabled", (char *)0)))
    return rc;
  return boolean("enabled", v, enabledp);
}

/** @brief Get the length of a track
 * @param c Client
 * @param track Track name (UTF-8)
 * @param valuep Where to store length in seconds
 * @return 0 on success, non-0 on error
 *
 * If the length is unknown 0 is returned.
 */
int disorder_length(disorder_client *c, const char *track,
		    long *valuep) {
  char *value;
  int rc;

  if((rc = disorder_simple(c, &value, "length", track, (char *)0)))
    return rc;
  *valuep = atol(value);
  return 0;
}

/** @brief Search for tracks
 * @param c Client
 * @param terms Search terms (UTF-8)
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_search(disorder_client *c, const char *terms,
		    char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "search", terms, (char *)0);
}

/** @brief Enable random play
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_random_enable(disorder_client *c) {
  return disorder_simple(c, 0, "random-enable", (char *)0);
}

/** @brief Disable random play
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_random_disable(disorder_client *c) {
  return disorder_simple(c, 0, "random-disable", (char *)0);
}

/** @brief Test whether random play is enabled
 * @param c Client
 * @param enabledp Where to store result (non-0 iff enabled)
 * @return 0 on success, non-0 on error
 */
int disorder_random_enabled(disorder_client *c, int *enabledp) {
  char *v;
  int rc;

  if((rc = disorder_simple(c, &v, "random-enabled", (char *)0)))
    return rc;
  return boolean("random-enabled", v, enabledp);
}

/** @brief Get server stats
 * @param c Client
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_stats(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "stats", (char *)0);
}

/** @brief Set volume
 * @param c Client
 * @param left New left channel value
 * @param right New right channel value
 * @return 0 on success, non-0 on error
 */
int disorder_set_volume(disorder_client *c, int left, int right) {
  char *ls, *rs;

  if(byte_asprintf(&ls, "%d", left) < 0
     || byte_asprintf(&rs, "%d", right) < 0)
    return -1;
  return disorder_simple(c, 0, "volume", ls, rs, (char *)0);
}

/** @brief Get volume
 * @param c Client
 * @param left Where to store left channel value
 * @param right Where to store right channel value
 * @return 0 on success, non-0 on error
 */
int disorder_get_volume(disorder_client *c, int *left, int *right) {
  char *r;
  int rc;

  if((rc = disorder_simple(c, &r, "volume", (char *)0)))
    return rc;
  if(sscanf(r, "%d %d", left, right) != 2) {
    c->last = "malformed volume response";
    error(0, "error parsing response to 'volume': '%s'", r);
    return -1;
  }
  return 0;
}

/** @brief Log to a sink
 * @param c Client
 * @param s Sink to write log lines to
 * @return 0 on success, non-0 on error
 */
int disorder_log(disorder_client *c, struct sink *s) {
  char *l;
  int rc;
    
  if((rc = disorder_simple(c, 0, "log", (char *)0)))
    return rc;
  while(inputline(c->ident, c->fpin, &l, '\n') >= 0 && strcmp(l, "."))
    if(sink_printf(s, "%s\n", l) < 0) return -1;
  if(ferror(c->fpin) || feof(c->fpin)) {
    byte_xasprintf((char **)&c->last, "input error: %s",
		   ferror(c->fpin) ? strerror(errno) : "unexpxected EOF");
    return -1;
  }
  return 0;
}

/** @brief Look up a track name part
 * @param c Client
 * @param partp Where to store result (UTF-8)
 * @param track Track name (UTF-8)
 * @param context Context (usually "sort" or "display") (UTF-8)
 * @param part Track part (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_part(disorder_client *c, char **partp,
		  const char *track, const char *context, const char *part) {
  return dequote(disorder_simple(c, partp, "part",
				 track, context, part, (char *)0), partp);
}

/** @brief Resolve aliases
 * @param c Client
 * @param trackp Where to store canonical name (UTF-8)
 * @param track Track name (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_resolve(disorder_client *c, char **trackp, const char *track) {
  return dequote(disorder_simple(c, trackp, "resolve", track, (char *)0),
		 trackp);
}

/** @brief Pause the current track
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_pause(disorder_client *c) {
  return disorder_simple(c, 0, "pause", (char *)0);
}

/** @brief Resume the current track
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_resume(disorder_client *c) {
  return disorder_simple(c, 0, "resume", (char *)0);
}

/** @brief List all known tags
 * @param c Client
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_tags(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "tags", (char *)0);
}

/** @brief List all known users
 * @param c Client
 * @param vecp Where to store list (UTF-8)
 * @param nvecp Where to store number of items, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_users(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "users", (char *)0);
}

/** @brief Get recently added tracks
 * @param c Client
 * @param vecp Where to store pointer to list (UTF-8)
 * @param nvecp Where to store count
 * @param max Maximum tracks to fetch, or 0 for all available
 * @return 0 on success, non-0 on error
 */
int disorder_new_tracks(disorder_client *c,
			char ***vecp, int *nvecp,
			int max) {
  char limit[32];

  sprintf(limit, "%d", max);
  return disorder_simple_list(c, vecp, nvecp, "new", limit, (char *)0);
}

/** @brief Set a global preference
 * @param c Client
 * @param key Preference name (UTF-8)
 * @param value Preference value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_set_global(disorder_client *c,
			const char *key, const char *value) {
  return disorder_simple(c, 0, "set-global", key, value, (char *)0);
}

/** @brief Unset a global preference
 * @param c Client
 * @param key Preference name (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_unset_global(disorder_client *c, const char *key) {
  return disorder_simple(c, 0, "unset-global", key, (char *)0);
}

/** @brief Get a global preference
 * @param c Client
 * @param key Preference name (UTF-8)
 * @param valuep Where to store preference value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_get_global(disorder_client *c, const char *key, char **valuep) {
  return dequote(disorder_simple(c, valuep, "get-global", key, (char *)0),
		 valuep);
}

/** @brief Get server's RTP address information
 * @param c Client
 * @param addressp Where to store address (UTF-8)
 * @param portp Where to store port (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_rtp_address(disorder_client *c, char **addressp, char **portp) {
  char *r;
  int rc, n;
  char **vec;

  if((rc = disorder_simple(c, &r, "rtp-address", (char *)0)))
    return rc;
  vec = split(r, &n, SPLIT_QUOTES, 0, 0);
  if(n != 2) {
    c->last = "malformed RTP address";
    error(0, "malformed rtp-address reply");
    return -1;
  }
  *addressp = vec[0];
  *portp = vec[1];
  return 0;
}

/** @brief Create a user
 * @param c Client
 * @param user Username
 * @param password Password
 * @param rights Initial rights or NULL to use default
 * @return 0 on success, non-0 on error
 */
int disorder_adduser(disorder_client *c,
		     const char *user, const char *password,
		     const char *rights) {
  return disorder_simple(c, 0, "adduser", user, password, rights, (char *)0);
}

/** @brief Delete a user
 * @param c Client
 * @param user Username
 * @return 0 on success, non-0 on error
 */
int disorder_deluser(disorder_client *c, const char *user) {
  return disorder_simple(c, 0, "deluser", user, (char *)0);
}

/** @brief Get user information
 * @param c Client
 * @param user Username
 * @param key Property name (UTF-8)
 * @param valuep Where to store value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_userinfo(disorder_client *c, const char *user, const char *key,
		      char **valuep) {
  return dequote(disorder_simple(c, valuep, "userinfo", user, key, (char *)0),
		 valuep);
}

/** @brief Set user information
 * @param c Client
 * @param user Username
 * @param key Property name (UTF-8)
 * @param value New property value (UTF-8)
 * @return 0 on success, non-0 on error
 */
int disorder_edituser(disorder_client *c, const char *user,
		      const char *key, const char *value) {
  return disorder_simple(c, 0, "edituser", user, key, value, (char *)0);
}

/** @brief Register a user
 * @param c Client
 * @param user Username
 * @param password Password
 * @param email Email address (UTF-8)
 * @param confirmp Where to store confirmation string
 * @return 0 on success, non-0 on error
 */
int disorder_register(disorder_client *c, const char *user,
		      const char *password, const char *email,
		      char **confirmp) {
  return dequote(disorder_simple(c, confirmp, "register",
				 user, password, email, (char *)0),
		 confirmp);
}

/** @brief Confirm a user
 * @param c Client
 * @param confirm Confirmation string
 * @return 0 on success, non-0 on error
 */
int disorder_confirm(disorder_client *c, const char *confirm) {
  char *u;
  int rc;
  
  if(!(rc = dequote(disorder_simple(c, &u, "confirm", confirm, (char *)0),
		    &u)))
    c->user = u;
  return rc;
}

/** @brief Make a cookie for this login
 * @param c Client
 * @param cookiep Where to store cookie string
 * @return 0 on success, non-0 on error
 */
int disorder_make_cookie(disorder_client *c, char **cookiep) {
  return dequote(disorder_simple(c, cookiep, "make-cookie", (char *)0),
		 cookiep);
}

/** @brief Revoke the cookie used by this session
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_revoke(disorder_client *c) {
  return disorder_simple(c, 0, "revoke", (char *)0);
}

/** @brief Request a password reminder email
 * @param c Client
 * @param user Username
 * @return 0 on success, non-0 on error
 */
int disorder_reminder(disorder_client *c, const char *user) {
  return disorder_simple(c, 0, "reminder", user, (char *)0);
}

/** @brief List scheduled events
 * @param c Client
 * @param idsp Where to put list of event IDs
 * @param nidsp Where to put count of event IDs, or NULL
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_list(disorder_client *c, char ***idsp, int *nidsp) {
  return disorder_simple_list(c, idsp, nidsp, "schedule-list", (char *)0);
}

/** @brief Delete a scheduled event
 * @param c Client
 * @param id Event ID to delete
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_del(disorder_client *c, const char *id) {
  return disorder_simple(c, 0, "schedule-del", id, (char *)0);
}

/** @brief Get details of a scheduled event
 * @param c Client
 * @param id Event ID
 * @param actiondatap Where to put details
 * @return 0 on success, non-0 on error
 */
int disorder_schedule_get(disorder_client *c, const char *id,
			  struct kvp **actiondatap) {
  char **lines, **bits;
  int rc, nbits;

  *actiondatap = 0;
  if((rc = disorder_simple_list(c, &lines, NULL,
				"schedule-get", id, (char *)0)))
    return rc;
  while(*lines) {
    if(!(bits = split(*lines++, &nbits, SPLIT_QUOTES, 0, 0))) {
      error(0, "invalid schedule-get reply: cannot split line");
      return -1;
    }
    if(nbits != 2) {
      error(0, "invalid schedule-get reply: wrong number of fields");
      return -1;
    }
    kvp_set(actiondatap, bits[0], bits[1]);
  }
  return 0;
}

/** @brief Add a scheduled event
 * @param c Client
 * @param when When to trigger the event
 * @param priority Event priority ("normal" or "junk")
 * @param action What action to perform
 * @param ... Action-specific arguments
 * @return 0 on success, non-0 on error
 *
 * For action @c "play" the next argument is the track.
 *
 * For action @c "set-global" next argument is the global preference name
 * and the final argument the value to set it to, or (char *)0 to unset it.
 */
int disorder_schedule_add(disorder_client *c,
			  time_t when,
			  const char *priority,
			  const char *action,
			  ...) {
  va_list ap;
  char when_str[64];
  int rc;

  snprintf(when_str, sizeof when_str, "%lld", (long long)when);
  va_start(ap, action);
  if(!strcmp(action, "play"))
    rc = disorder_simple(c, 0, "schedule-add", when_str, priority,
			 action, va_arg(ap, char *),
			 (char *)0);
  else if(!strcmp(action, "set-global")) {
    const char *key = va_arg(ap, char *);
    const char *value = va_arg(ap, char *);
    rc = disorder_simple(c, 0,"schedule-add",  when_str, priority,
			 action, key, value,
			 (char *)0);
  } else
    fatal(0, "unknown action '%s'", action);
  va_end(ap);
  return rc;
}

/** @brief Delete a playlist
 * @param c Client
 * @param playlist Playlist to delete
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_delete(disorder_client *c,
                             const char *playlist) {
  return disorder_simple(c, 0, "playlist-delete", playlist, (char *)0);
}

/** @brief Get the contents of a playlist
 * @param c Client
 * @param playlist Playlist to get
 * @param tracksp Where to put list of tracks
 * @param ntracksp Where to put count of tracks
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_get(disorder_client *c, const char *playlist,
                          char ***tracksp, int *ntracksp) {
  return disorder_simple_list(c, tracksp, ntracksp,
                              "playlist-get", playlist, (char *)0);
}

/** @brief List all readable playlists
 * @param c Client
 * @param playlistsp Where to put list of playlists
 * @param nplaylistsp Where to put count of playlists
 * @return 0 on success, non-0 on error
 */
int disorder_playlists(disorder_client *c,
                       char ***playlistsp, int *nplaylistsp) {
  return disorder_simple_list(c, playlistsp, nplaylistsp,
                              "playlists", (char *)0);
}

/** @brief Get the sharing status of a playlist
 * @param c Client
 * @param playlist Playlist to inspect
 * @param sharep Where to put sharing status
 * @return 0 on success, non-0 on error
 *
 * Possible @p sharep values are @c public, @c private and @c shared.
 */
int disorder_playlist_get_share(disorder_client *c, const char *playlist,
                                char **sharep) {
  return disorder_simple(c, sharep,
                         "playlist-get-share", playlist, (char *)0);
}

/** @brief Get the sharing status of a playlist
 * @param c Client
 * @param playlist Playlist to modify
 * @param share New sharing status
 * @return 0 on success, non-0 on error
 *
 * Possible @p share values are @c public, @c private and @c shared.
 */
int disorder_playlist_set_share(disorder_client *c, const char *playlist,
                                const char *share) {
  return disorder_simple(c, 0,
                         "playlist-set-share", playlist, share, (char *)0);
}

/** @brief Lock a playlist for modifications
 * @param c Client
 * @param playlist Playlist to lock
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_lock(disorder_client *c, const char *playlist) {
  return disorder_simple(c, 0,
                         "playlist-lock", playlist, (char *)0);
}

/** @brief Unlock the locked playlist
 * @param c Client
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_unlock(disorder_client *c) {
  return disorder_simple(c, 0,
                         "playlist-unlock", (char *)0);
}

/** @brief Set the contents of a playlst
 * @param c Client
 * @param playlist Playlist to modify
 * @param tracks List of tracks
 * @param ntracks Length of @p tracks (or -1 to count up to the first NULL)
 * @return 0 on success, non-0 on error
 */
int disorder_playlist_set(disorder_client *c,
                          const char *playlist,
                          char **tracks,
                          int ntracks) {
  return disorder_simple_body(c, 0, tracks, ntracks,
                              "playlist-set", playlist, (char *)0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
