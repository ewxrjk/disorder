/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2006, 2007 Richard Kettlewell
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
#include <gcrypt.h>

#include "hex.h"
#include "log.h"
#include "authhash.h"

/** @brief Structure of algorithm lookup table */
struct algorithm {
  /** @brief DisOrder algorithm name */
  const char *name;

  /** @brief gcrypt algorithm ID */
  int id;
};

/** @brief Algorithm lookup table
 *
 * We don't use gcry_md_map_name() since that would import gcrypt's API into
 * the disorder protocol.
 */
static const struct algorithm algorithms[] = {
  { "SHA1", GCRY_MD_SHA1 },
  { "sha1", GCRY_MD_SHA1 },
  { "SHA256", GCRY_MD_SHA256 },
  { "sha256", GCRY_MD_SHA256 },
  { "SHA384", GCRY_MD_SHA384 },
  { "sha384", GCRY_MD_SHA384 },
  { "SHA512", GCRY_MD_SHA512 },
  { "sha512", GCRY_MD_SHA512 },
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
const char *authhash(const void *challenge, size_t nchallenge,
		     const char *password, const char *algo) {
  gcrypt_hash_handle h;
  const char *res;
  size_t n;
  int id;

  assert(challenge != 0);
  assert(password != 0);
  assert(algo != 0);
  for(n = 0; n < NALGORITHMS; ++n)
    if(!strcmp(algo, algorithms[n].name))
      break;
  if(n >= NALGORITHMS)
    return NULL;
  id = algorithms[n].id;
#if HAVE_GCRY_ERROR_T
  {
    gcry_error_t e;
    
    if((e = gcry_md_open(&h, id, 0))) {
      error(0, "gcry_md_open: %s", gcry_strerror(e));
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
