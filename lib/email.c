/*
 * This file is part of DisOrder
 * Copyright (C) 2008 Richard Kettlewell
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

/** @file lib/email.c
 * @brief Email addresses
 */

#include "common.h"

#include "sendmail.h"

/** @brief Test email address validity
 * @param address to verify
 * @return 1 if it might be valid, 0 if it is definitely not
 *
 * This function doesn't promise to tell you whether an address is deliverable,
 * it just does basic syntax checks.
 */
int email_valid(const char *address) {
  /* There must be an '@' sign */
  const char *at = strchr(address, '@');
  if(!at)
    return 0;
  /* There must be only one of them */
  if(strchr(at + 1, '@'))
    return 0;
  /* It mustn't be the first or last character */
  if(at == address || !at[1])
    return 0;
  /* Local part must be valid */
  /* TODO */
  /* Domain part must be valid */
  /* TODO */
  return 1;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
