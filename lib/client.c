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

#include <config.h>
#include "types.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <pcre.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
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

struct disorder_client {
  FILE *fpin, *fpout;
  char *ident;
  char *user;
  int verbose;
};

disorder_client *disorder_new(int verbose) {
  disorder_client *c = xmalloc(sizeof (struct disorder_client));

  c->verbose = verbose;
  return c;
}

/* read a response line.
 * If @rp@ is not a null pointer, returns the whole response through it.
 * Return value is the response code, or -1 on error. */
static int response(disorder_client *c, char **rp) {
  char *r;

  if(inputline(c->ident, c->fpin, &r, '\n'))
    return -1;
  D(("response: %s", r));
  if(rp)
    *rp = r;
  if(r[0] >= '0' && r[0] <= '9'
     && r[1] >= '0' && r[1] <= '9'
     && r[2] >= '0' && r[2] <= '9'
     && r[3] == ' ')
    return (r[0] * 10 + r[1]) * 10 + r[2] - 111 * '0';
  else {
    error(0, "invalid reply format from %s", c->ident);
    return -1;
  }
}

/* Read a response.
 * If @rp@ is not a null pointer then the response text (excluding
 * the status code) is returned through it, UNLESS the response code
 * is xx9.
 * Return value is 0 for 2xx responses and -1 otherwise.
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
    return -1;
  }
}

static int disorder_simple_v(disorder_client *c,
			     char **rp,
			     const char *cmd, va_list ap) {
  const char *arg;
  struct dynstr d;

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
    if(fputs(d.vec, c->fpout) < 0 || fflush(c->fpout)) {
      error(errno, "error writing to %s", c->ident);
      return -1;
    }
  }
  return check_response(c, rp);
}

/* Execute a simple command with any number of arguments.
 * @rp@ and return value as for check_response().
 */
static int disorder_simple(disorder_client *c,
			   char **rp,
			   const char *cmd, ...) {
  va_list ap;
  int ret;

  va_start(ap, cmd);
  ret = disorder_simple_v(c, rp, cmd, ap);
  va_end(ap);
  return ret;
}

static int connect_sock(void *vc,
			const struct sockaddr *sa,
			socklen_t len,
			const char *ident) {
  const char *username, *password;
  disorder_client *c = vc;
  
  if(!(username = config->username)) {
    error(0, "no username configured");
    return -1;
  }
  password = config->password;
  if(!password) {
    /* Maybe we can read the database */
    /* TODO failure to open the database should not be fatal */
    trackdb_init(TRACKDB_NO_RECOVER|TRACKDB_NO_UPGRADE);
    trackdb_open(TRACKDB_READ_ONLY);
    password = trackdb_get_password(username);
    trackdb_close();
  }
  if(!password) {
    /* Oh well */
    error(0, "no password configured");
    return -1;
  }
  return disorder_connect_sock(c, sa, len, username, password, ident);
}

int disorder_connect(disorder_client *c) {
  return with_sockaddr(c, connect_sock);
}

static int check_running(void attribute((unused)) *c,
			 const struct sockaddr *sa,
			 socklen_t len,
			 const char attribute((unused)) *ident) {
  int fd, ret;

  if((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
    fatal(errno, "error calling socket");
  if(connect(fd, sa, len) < 0) {
    if(errno == ECONNREFUSED || errno == ENOENT)
      ret = 0;
    else
      fatal(errno, "error calling connect");
  } else
    ret = 1;
  xclose(fd);
  return ret;
}

int disorder_running(disorder_client *c) {
  return with_sockaddr(c, check_running);
}

int disorder_connect_sock(disorder_client *c,
			  const struct sockaddr *sa,
			  socklen_t len,
			  const char *username,
			  const char *password,
			  const char *ident) {
  int fd = -1, fd2 = -1, nrvec;
  unsigned char *nonce;
  size_t nl;
  const char *res;
  char *r, **rvec;
  const char *algo = "SHA1";

  if(!password) {
    error(0, "no password found");
    return -1;
  }
  c->fpin = c->fpout = 0;
  if((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0) {
    error(errno, "error calling socket");
    return -1;
  }
  if(connect(fd, sa, len) < 0) {
    error(errno, "error calling connect");
    goto error;
  }
  if((fd2 = dup(fd)) < 0) {
    error(errno, "error calling dup");
    goto error;
  }
  if(!(c->fpin = fdopen(fd, "rb"))) {
    error(errno, "error calling fdopen");
    goto error;
  }
  fd = -1;
  if(!(c->fpout = fdopen(fd2, "wb"))) {
    error(errno, "error calling fdopen");
    goto error;
  }
  fd2 = -1;
  c->ident = xstrdup(ident);
  if(disorder_simple(c, &r, 0, (const char *)0))
    return -1;
  if(!(rvec = split(r, &nrvec, SPLIT_QUOTES, 0, 0)))
    return -1;
  if(nrvec > 1) {
    algo = *rvec++;
    --nrvec;
  }
  if(!nrvec)
    return -1;
  if(!(nonce = unhex(*rvec, &nl)))
    return -1;
  if(!(res = authhash(nonce, nl, password, algo))) goto error;
  if(disorder_simple(c, 0, "user", username, res, (char *)0))
    return -1;
  c->user = xstrdup(username);
  return 0;
error:
  if(c->fpin) fclose(c->fpin);
  if(c->fpout) fclose(c->fpout);
  if(fd2 != -1) close(fd2);
  if(fd != -1) close(fd);
  return -1;
}

int disorder_close(disorder_client *c) {
  int ret = 0;

  if(c->fpin) {
    if(fclose(c->fpin) < 0) {
      error(errno, "error calling fclose");
      ret = -1;
    }
    c->fpin = 0;
  }
  if(c->fpout) {
    if(fclose(c->fpout) < 0) {
      error(errno, "error calling fclose");
      ret = -1;
    }
    c->fpout = 0;
  }
  return 0;
}

int disorder_become(disorder_client *c, const char *user) {
  if(disorder_simple(c, 0, "become", user, (char *)0)) return -1;
  c->user = xstrdup(user);
  return 0;
}

int disorder_play(disorder_client *c, const char *track) {
  return disorder_simple(c, 0, "play", track, (char *)0);
}

int disorder_remove(disorder_client *c, const char *track) {
  return disorder_simple(c, 0, "remove", track, (char *)0);
}

int disorder_move(disorder_client *c, const char *track, int delta) {
  char d[16];

  byte_snprintf(d, sizeof d, "%d", delta);
  return disorder_simple(c, 0, "move", track, d, (char *)0);
}

int disorder_enable(disorder_client *c) {
  return disorder_simple(c, 0, "enable", (char *)0);
}

int disorder_disable(disorder_client *c) {
  return disorder_simple(c, 0, "disable", (char *)0);
}

int disorder_scratch(disorder_client *c, const char *id) {
  return disorder_simple(c, 0, "scratch", id, (char *)0);
}

int disorder_shutdown(disorder_client *c) {
  return disorder_simple(c, 0, "shutdown", (char *)0);
}

int disorder_reconfigure(disorder_client *c) {
  return disorder_simple(c, 0, "reconfigure", (char *)0);
}

int disorder_rescan(disorder_client *c) {
  return disorder_simple(c, 0, "rescan", (char *)0);
}

int disorder_version(disorder_client *c, char **rp) {
  return disorder_simple(c, rp, "version", (char *)0);
}

static void client_error(const char *msg,
			 void attribute((unused)) *u) {
  error(0, "error parsing reply: %s", msg);
}

int disorder_playing(disorder_client *c, struct queue_entry **qp) {
  char *r;
  struct queue_entry *q;

  if(disorder_simple(c, &r, "playing", (char *)0))
    return -1;
  if(r) {
    q = xmalloc(sizeof *q);
    if(queue_unmarshall(q, r, client_error, 0))
      return -1;
    *qp = q;
  } else
    *qp = 0;
  return 0;
}

static int disorder_somequeue(disorder_client *c,
			      const char *cmd, struct queue_entry **qp) {
  struct queue_entry *qh, **qt = &qh, *q;
  char *l;

  if(disorder_simple(c, 0, cmd, (char *)0))
    return -1;
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
  if(ferror(c->fpin))
    error(errno, "error reading %s", c->ident);
  else
    error(0, "error reading %s: unexpected EOF", c->ident);
  return -1;
}

int disorder_recent(disorder_client *c, struct queue_entry **qp) {
  return disorder_somequeue(c, "recent", qp);
}

int disorder_queue(disorder_client *c, struct queue_entry **qp) {
  return disorder_somequeue(c, "queue", qp);
}

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
  if(ferror(c->fpin))
    error(errno, "error reading %s", c->ident);
  else
    error(0, "error reading %s: unexpected EOF", c->ident);
  return -1;
}

static int disorder_simple_list(disorder_client *c,
				char ***vecp, int *nvecp,
				const char *cmd, ...) {
  va_list ap;
  int ret;

  va_start(ap, cmd);
  ret = disorder_simple_v(c, 0, cmd, ap);
  va_end(ap);
  if(ret) return ret;
  return readlist(c, vecp, nvecp);
}

int disorder_directories(disorder_client *c, const char *dir, const char *re,
			 char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "dirs", dir, re, (char *)0);
}

int disorder_files(disorder_client *c, const char *dir, const char *re,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "files", dir, re, (char *)0);
}

int disorder_allfiles(disorder_client *c, const char *dir, const char *re,
		      char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "allfiles", dir, re, (char *)0);
}

char *disorder_user(disorder_client *c) {
  return c->user;
}

int disorder_set(disorder_client *c, const char *track,
		 const char *key, const char *value) {
  return disorder_simple(c, 0, "set", track, key, value, (char *)0);
}

int disorder_unset(disorder_client *c, const char *track,
		   const char *key) {
  return disorder_simple(c, 0, "unset", track, key, (char *)0);
}

int disorder_get(disorder_client *c,
		 const char *track, const char *key, char **valuep) {
  return disorder_simple(c, valuep, "get", track, key, (char *)0);
}

static void pref_error_handler(const char *msg,
			       void attribute((unused)) *u) {
  error(0, "error handling 'prefs' reply: %s", msg);
}

int disorder_prefs(disorder_client *c, const char *track, struct kvp **kp) {
  char **vec, **pvec;
  int nvec, npvec, n;
  struct kvp *k;

  if(disorder_simple_list(c, &vec, &nvec, "prefs", track, (char *)0))
    return -1;
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

int disorder_exists(disorder_client *c, const char *track, int *existsp) {
  char *v;

  if(disorder_simple(c, &v, "exists", track, (char *)0)) return -1;
  return boolean("exists", v, existsp);
}

int disorder_enabled(disorder_client *c, int *enabledp) {
  char *v;

  if(disorder_simple(c, &v, "enabled", (char *)0)) return -1;
  return boolean("enabled", v, enabledp);
}

int disorder_length(disorder_client *c, const char *track,
		    long *valuep) {
  char *value;

  if(disorder_simple(c, &value, "length", track, (char *)0)) return -1;
  *valuep = atol(value);
  return 0;
}

int disorder_search(disorder_client *c, const char *terms,
		    char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "search", terms, (char *)0);
}

int disorder_random_enable(disorder_client *c) {
  return disorder_simple(c, 0, "random-enable", (char *)0);
}

int disorder_random_disable(disorder_client *c) {
  return disorder_simple(c, 0, "random-disable", (char *)0);
}

int disorder_random_enabled(disorder_client *c, int *enabledp) {
  char *v;

  if(disorder_simple(c, &v, "random-enabled", (char *)0)) return -1;
  return boolean("random-enabled", v, enabledp);
}

int disorder_stats(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "stats", (char *)0);
}

int disorder_set_volume(disorder_client *c, int left, int right) {
  char *ls, *rs;

  if(byte_asprintf(&ls, "%d", left) < 0
     || byte_asprintf(&rs, "%d", right) < 0)
    return -1;
  if(disorder_simple(c, 0, "volume", ls, rs, (char *)0)) return -1;
  return 0;
}

int disorder_get_volume(disorder_client *c, int *left, int *right) {
  char *r;

  if(disorder_simple(c, &r, "volume", (char *)0)) return -1;
  if(sscanf(r, "%d %d", left, right) != 2) {
    error(0, "error parsing response to 'volume': '%s'", r);
    return -1;
  }
  return 0;
}

int disorder_log(disorder_client *c, struct sink *s) {
  char *l;
    
  if(disorder_simple(c, 0, "log", (char *)0)) return -1;
  while(inputline(c->ident, c->fpin, &l, '\n') >= 0 && strcmp(l, "."))
    if(sink_printf(s, "%s\n", l) < 0) return -1;
  if(ferror(c->fpin) || feof(c->fpin)) return -1;
  return 0;
}

int disorder_part(disorder_client *c, char **partp,
		  const char *track, const char *context, const char *part) {
  return disorder_simple(c, partp, "part", track, context, part, (char *)0);
}

int disorder_resolve(disorder_client *c, char **trackp, const char *track) {
  return disorder_simple(c, trackp, "resolve", track, (char *)0);
}

int disorder_pause(disorder_client *c) {
  return disorder_simple(c, 0, "pause", (char *)0);
}

int disorder_resume(disorder_client *c) {
  return disorder_simple(c, 0, "resume", (char *)0);
}

int disorder_tags(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "tags", (char *)0);
}

int disorder_users(disorder_client *c,
		   char ***vecp, int *nvecp) {
  return disorder_simple_list(c, vecp, nvecp, "users", (char *)0);
}

/** @brief Get recentl added tracks
 * @param c Client
 * @param vecp Where to store pointer to list
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

int disorder_set_global(disorder_client *c,
			const char *key, const char *value) {
  return disorder_simple(c, 0, "set-global", key, value, (char *)0);
}

int disorder_unset_global(disorder_client *c, const char *key) {
  return disorder_simple(c, 0, "unset-global", key, (char *)0);
}

int disorder_get_global(disorder_client *c, const char *key, char **valuep) {
  return disorder_simple(c, valuep, "get-global", key, (char *)0);
}

int disorder_rtp_address(disorder_client *c, char **addressp, char **portp) {
  char *r;
  int rc, n;
  char **vec;

  if((rc = disorder_simple(c, &r, "rtp-address", (char *)0)))
    return rc;
  vec = split(r, &n, SPLIT_QUOTES, 0, 0);
  if(n != 2) {
    error(0, "malformed rtp-address reply");
    return -1;
  }
  *addressp = vec[0];
  *portp = vec[1];
  return 0;
}

int disorder_adduser(disorder_client *c,
		     const char *user, const char *password) {
  return disorder_simple(c, 0, "adduser", user, password, (char *)0);
}

int disorder_deluser(disorder_client *c, const char *user) {
  return disorder_simple(c, 0, "deluser", user, (char *)0);
}

int disorder_userinfo(disorder_client *c, const char *user, const char *key,
		      char **valuep) {
  return disorder_simple(c, valuep, "userinfo", user, key, (char *)0);
}

int disorder_edituser(disorder_client *c, const char *user,
		      const char *key, const char *value) {
  return disorder_simple(c, 0, "edituser", user, key, value, (char *)0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
