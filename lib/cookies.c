/*
 * This file is part of DisOrder
 * Copyright (C) 2007, 2008 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file lib/cookies.c
 * @brief Cookie support
 */

#include "common.h"

#include <errno.h>
#include <time.h>
#include <gcrypt.h>

#include "cookies.h"
#include "hash.h"
#include "mem.h"
#include "log.h"
#include "printf.h"
#include "base64.h"
#include "configuration.h"
#include "kvp.h"
#include "trackdb.h"
#include "syscalls.h"

/** @brief Hash function used in signing HMAC */
#define ALGO GCRY_MD_SHA1

/** @brief Size of key to use */
#define HASHSIZE 20

/** @brief Signing key */
static uint8_t signing_key[HASHSIZE];

/** @brief Previous signing key */
static uint8_t old_signing_key[HASHSIZE];

/** @brief Signing key validity limit or 0 if none */
static time_t signing_key_validity_limit;

/** @brief Hash of revoked cookies */
static hash *revoked;

/** @brief Callback to expire revocation list */
static int revoked_cleanup_callback(const char *key, void *value,
                                    void *u) {
  if(*(time_t *)value < *(time_t *)u)
    hash_remove(revoked, key);
  return 0;
}

/** @brief Generate a new key */
static void newkey(void) {
  time_t now;

  xtime(&now);
  memcpy(old_signing_key, signing_key, HASHSIZE);
  gcry_randomize(signing_key, HASHSIZE, GCRY_STRONG_RANDOM);
  signing_key_validity_limit = now + config->cookie_key_lifetime;
  /* Now is a good time to clean up the revocation list... */
  if(revoked)
    hash_foreach(revoked, revoked_cleanup_callback, &now);
}

/** @brief Base64 mapping table for cookies
 *
 * Stupid Safari cannot cope with quoted cookies, so cookies had better not
 * need quoting.  We use $ to separate the parts of the cookie and +%# to where
 * MIME uses +/=; see @ref base64.c.  See http_separator() for the characters
 * to avoid.
 */
static const char cookie_base64_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+%#";

/** @brief Sign @p subject with @p key and return the base64 of the result
 * @param key Key to sign with (@ref HASHSIZE bytes)
 * @param subject Subject string
 * @return Base64-encoded signature or NULL
 */
static char *sign(const uint8_t *key,
		  const char *subject) {
  gcry_error_t e;
  gcry_md_hd_t h;
  uint8_t *sig;
  char *sig64;

  if((e = gcry_md_open(&h, ALGO, GCRY_MD_FLAG_HMAC))) {
    disorder_error(0, "gcry_md_open: %s", gcry_strerror(e));
    return 0;
  }
  if((e = gcry_md_setkey(h, key, HASHSIZE))) {
    disorder_error(0, "gcry_md_setkey: %s", gcry_strerror(e));
    gcry_md_close(h);
    return 0;
  }
  gcry_md_write(h, subject, strlen(subject));
  sig = gcry_md_read(h, ALGO);
  sig64 = generic_to_base64(sig, HASHSIZE, cookie_base64_table);
  gcry_md_close(h);
  return sig64;
}

/** @brief Create a login cookie
 * @param user Username
 * @return Cookie or NULL
 */
char *make_cookie(const char *user) {
  const char *password;
  time_t now;
  char *b, *bp, *c, *g;

  /* dollar signs aren't allowed in usernames */
  if(strchr(user, '$')) {
    disorder_error(0, "make_cookie for username with dollar sign");
    return 0;
  }
  /* look up the password */
  password = trackdb_get_password(user);
  if(!password) {
    disorder_error(0, "make_cookie for nonexistent user");
    return 0;
  }
  /* make sure we have a valid signing key */
  xtime(&now);
  if(now >= signing_key_validity_limit)
    newkey();
  /* construct the subject */
  byte_xasprintf(&b, "%jx$%s$", (intmax_t)now + config->cookie_login_lifetime,
		 urlencodestring(user));
  byte_xasprintf(&bp, "%s%s", b, password);
  /* sign it */
  if(!(g = sign(signing_key, bp)))
    return 0;
  /* put together the final cookie */
  byte_xasprintf(&c, "%s%s", b, g);
  return c;
}

/** @brief Verify a cookie
 * @param cookie Cookie to verify
 * @param rights Where to store rights value
 * @return Verified user or NULL
 */
char *verify_cookie(const char *cookie, rights_type *rights) {
  char *c1, *c2;
  intmax_t t;
  time_t now;
  char *user, *bp, *sig;
  const char *password;
  struct kvp *k;

  /* check the revocation list */
  if(revoked && hash_find(revoked, cookie)) {
    disorder_error(0, "attempt to log in with revoked cookie");
    return 0;
  }
  /* parse the cookie */
  errno = 0;
  t = strtoimax(cookie, &c1, 16);
  if(errno) {
    disorder_error(errno, "error parsing cookie timestamp");
    return 0;
  }
  if(*c1 != '$') {
    disorder_error(0, "invalid cookie timestamp");
    return 0;
  }
  /* There'd better be two dollar signs */
  c2 = strchr(c1 + 1, '$');
  if(c2 == 0) {
    disorder_error(0, "invalid cookie syntax");
    return 0;
  }
  /* Extract the username */
  user = xstrndup(c1 + 1, c2 - (c1 + 1));
  /* check expiry */
  xtime(&now);
  if(now >= t) {
    disorder_error(0, "cookie has expired");
    return 0;
  }
  /* look up the password */
  k = trackdb_getuserinfo(user);
  if(!k) {
    disorder_error(0, "verify_cookie for nonexistent user");
    return 0;
  }
  password = kvp_get(k, "password");
  if(!password) password = "";
  if(parse_rights(kvp_get(k, "rights"), rights, 1))
    return 0;
  /* construct the expected subject.  We re-encode the timestamp and the
   * password. */
  byte_xasprintf(&bp, "%jx$%s$%s", t, urlencodestring(user), password);
  /* Compute the expected signature.  NB we base64 the expected signature and
   * compare that rather than exposing our base64 parser to the cookie. */
  if(!(sig = sign(signing_key, bp)))
    return 0;
  if(!strcmp(sig, c2 + 1))
    return user;
  /* that didn't match, try the old key */
  if(!(sig = sign(old_signing_key, bp)))
    return 0;
  if(!strcmp(sig, c2 + 1))
    return user;
  /* that didn't match either */
  disorder_error(0, "cookie signature does not match");
  return 0;
}

/** @brief Revoke a cookie
 * @param cookie Cookie to revoke
 *
 * Further attempts to log in with @p cookie will fail.
 */
void revoke_cookie(const char *cookie) {
  time_t when;
  char *ptr;

  /* find the cookie's expiry time */
  errno = 0;
  when = (time_t)strtoimax(cookie, &ptr, 16);
  /* reject bogus cookies */
  if(errno)
    return;
  if(*ptr != '$')
    return;
  /* make sure the revocation list exists */
  if(!revoked)
    revoked = hash_new(sizeof(time_t));
  /* add the cookie to it; its value is the expiry time */
  hash_add(revoked, cookie, &when, HASH_INSERT);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
