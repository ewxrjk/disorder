/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
#include "test.h"

static void test_kvp(void) {
  struct kvp *k;
  size_t n;
  
  /* decoding */
#define KVP_URLDECODE(S) kvp_urldecode((S), strlen(S))
  fprintf(stderr, "5 ERROR reports expected {\n");
  insist(KVP_URLDECODE("=%zz") == 0);
  insist(KVP_URLDECODE("=%0") == 0);
  insist(KVP_URLDECODE("=%0z") == 0);
  insist(KVP_URLDECODE("=%%") == 0);
  insist(KVP_URLDECODE("==%") == 0);
  fprintf(stderr, "}\n");
  insist(KVP_URLDECODE("wibble") == 0);
  insist(KVP_URLDECODE("") == 0);
  insist(KVP_URLDECODE("wibble&") == 0);
  insist((k = KVP_URLDECODE("one=bl%61t+foo")) != 0);
  check_string(kvp_get(k, "one"), "blat foo");
  insist(kvp_get(k, "ONE") == 0);
  insist(k->next == 0);
  insist((k = KVP_URLDECODE("wibble=splat&bar=spong")) != 0);
  check_string(kvp_get(k, "wibble"), "splat");
  check_string(kvp_get(k, "bar"), "spong");
  insist(kvp_get(k, "ONE") == 0);
  insist(k->next->next == 0);
  /* encoding */
  insist(kvp_set(&k, "bar", "spong") == 0);
  insist(kvp_set(&k, "bar", "foo") == 1);
  insist(kvp_set(&k, "zog", "%") == 1);
  insist(kvp_set(&k, "wibble", 0) == 1);
  insist(kvp_set(&k, "wibble", 0) == 0);
  check_string(kvp_urlencode(k, 0),
               "bar=foo&zog=%25");
  check_string(kvp_urlencode(k, &n),
               "bar=foo&zog=%25");
  insist(n == strlen("bar=foo&zog=%25"));
  check_string(urlencodestring("abc% +\n"),
               "abc%25%20%2b%0a");
  check_integer(urldecode(sink_error(), "bar=foo", 7), -1);
  check_integer(urlencode(sink_error(), "wibble", 7), -1);
  check_integer(urlencode(sink_error(), " ", 1), -1);
}

TEST(kvp);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
