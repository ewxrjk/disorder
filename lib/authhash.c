/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2006, 2007, 2009, 2013 Richard Kettlewell
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
/** @file lib/authhash.c @brief The authorization hash */

#include "common.h"

#include <stddef.h>
#if HAVE_GCRYPT_H
# include <gcrypt.h>
#elif HAVE_BCRYPT_H
# include <bcrypt.h>
# pragma comment(lib, "bcrypt")
#else
# error No crypto API available
#endif

#include "hex.h"
#include "log.h"
#include "authhash.h"
#include "vector.h"

/** @brief Structure of algorithm lookup table */
struct algorithm {
  /** @brief DisOrder algorithm name */
  const char *name;

#if HAVE_GCRYPT_H
  /** @brief gcrypt algorithm ID */
  int id;
#elif HAVE_BCRYPT_H
  /** @brief CNG algorithm ID */
  const wchar_t *id;
#endif

};

/** @brief Algorithm lookup table
 *
 * We don't use gcry_md_map_name() since that would import gcrypt's API into
 * the disorder protocol.
 */
static const struct algorithm algorithms[] = {
#if HAVE_GCRYPT_H
  { "SHA1", GCRY_MD_SHA1 },
  { "sha1", GCRY_MD_SHA1 },
  { "SHA256", GCRY_MD_SHA256 },
  { "sha256", GCRY_MD_SHA256 },
  { "SHA384", GCRY_MD_SHA384 },
  { "sha384", GCRY_MD_SHA384 },
  { "SHA512", GCRY_MD_SHA512 },
  { "sha512", GCRY_MD_SHA512 },
#elif HAVE_BCRYPT_H
  { "SHA1", BCRYPT_SHA1_ALGORITHM },
  { "sha1", BCRYPT_SHA1_ALGORITHM },
  { "SHA256", BCRYPT_SHA256_ALGORITHM },
  { "sha256", BCRYPT_SHA256_ALGORITHM },
  { "SHA384", BCRYPT_SHA384_ALGORITHM },
  { "sha384", BCRYPT_SHA384_ALGORITHM },
  { "SHA512", BCRYPT_SHA512_ALGORITHM },
  { "sha512", BCRYPT_SHA512_ALGORITHM },
#endif
};

/** @brief Number of supported algorithms */
#define NALGORITHMS (sizeof algorithms / sizeof *algorithms)

/** @brief Perform the authorization hash function
 * @param challenge Pointer to challange
 * @param nchallenge Size of challenge
 * @param password Password
 * @param algo Algorithm to use
 * @return Hex string or NULL on error
 *
 * Computes H(challenge|password) and returns it as a newly allocated hex
 * string, or returns NULL on error.
 */
char *authhash(const void *challenge, size_t nchallenge,
               const char *password, const char *algo) {
#if HAVE_GCRYPT_H
  gcrypt_hash_handle h;
  int id;
#elif HAVE_BCRYPT_H
  BCRYPT_ALG_HANDLE alg = 0;
  BCRYPT_HASH_HANDLE hash = 0;
  DWORD hashlen, hashlenlen;
  PBYTE hashed = 0;
  NTSTATUS rc;
  struct dynstr d;
#endif
  char *res;
  size_t n;

  assert(challenge != 0);
  assert(password != 0);
  assert(algo != 0);
  for(n = 0; n < NALGORITHMS; ++n)
    if(!strcmp(algo, algorithms[n].name))
      break;
  if(n >= NALGORITHMS)
    return NULL;
#if HAVE_GCRYPT_H
  id = algorithms[n].id;
#if HAVE_GCRY_ERROR_T
  {
    gcry_error_t e;
    
    if((e = gcry_md_open(&h, id, 0))) {
      disorder_error(0, "gcry_md_open: %s", gcry_strerror(e));
      return 0;
    }
  }
#else
  h = gcry_md_open(id, 0);
#endif
  gcry_md_write(h, password, strlen(password));
  gcry_md_write(h, challenge, nchallenge);
  res = hex(gcry_md_read(h, id), gcry_md_get_algo_dlen(id));
  gcry_md_close(h);
#elif HAVE_BCRYPT_H
  dynstr_init(&d);
  dynstr_append_string(&d, password);
  dynstr_append_bytes(&d, challenge, nchallenge);
#define DO(fn, args) do { \
  if((rc = fn args)) { disorder_error(0, "%s: %d", #fn, rc); goto error; } \
  } while(0)
  res = NULL;
  DO(BCryptOpenAlgorithmProvider, (&alg, algorithms[n].id, NULL, 0));
  DO(BCryptGetProperty, (alg, BCRYPT_HASH_LENGTH, (PBYTE)&hashlen, sizeof hashlen, &hashlenlen, 0));
  DO(BCryptCreateHash, (alg, &hash, NULL, 0, NULL, 0, 0));
  DO(BCryptHashData, (hash, d.vec, d.nvec, 0));
  hashed = xmalloc(hashlen);
  DO(BCryptFinishHash, (hash, hashed, hashlen, 0));
  res = hex(hashed, hashlen);
error:
  if(hash) BCryptDestroyHash(hash);
  if(alg) BCryptCloseAlgorithmProvider(alg, 0);
  xfree(hashed);
#endif
  return res;
}

/** @brief Return non-zero if @p algo is a valid algorithm */
int valid_authhash(const char *algo) {
  size_t n;

  for(n = 0; n < NALGORITHMS; ++n)
    if(!strcmp(algo, algorithms[n].name))
      return 1;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
