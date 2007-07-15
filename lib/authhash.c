/*
 * This file is part of DisOrder
 * Copyright (C) 2004, 2006 Richard Kettlewell
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

#include <stddef.h>
#include <gcrypt.h>
#include <assert.h>

#include "hex.h"
#include "log.h"
#include "authhash.h"

#ifndef AUTHHASH
# define AUTHHASH GCRY_MD_SHA1
#endif

const char *authhash(const void *challenge, size_t nchallenge,
		     const char *password) {
  gcrypt_hash_handle h;
  const char *res;

  assert(challenge != 0);
  assert(password != 0);
#if HAVE_GCRY_ERROR_T
  {
    gcry_error_t e;
    
    if((e = gcry_md_open(&h, AUTHHASH, 0))) {
      error(0, "gcry_md_open: %s", gcry_strerror(e));
      return 0;
    }
  }
#else
  h = gcry_md_open(AUTHHASH, 0);
#endif
  gcry_md_write(h, password, strlen(password));
  gcry_md_write(h, challenge, nchallenge);
  res = hex(gcry_md_read(h, AUTHHASH), gcry_md_get_algo_dlen(AUTHHASH));
  gcry_md_close(h);
  return res;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
