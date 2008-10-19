/*
 * This file is part of DisOrder.
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
#include "test.h"
#include "arcfour.h"

#define TEST_ARCFOUR(K, P, C) do {			\
  arcfour_setkey(ac, K, strlen(K));			\
  arcfour_stream(ac, P, (char *)output, strlen(P));	\
  output_hex = hex(output, strlen(P));			\
  check_string(output_hex, C);				\
} while(0)
  
static void test_arcfour(void) {
  arcfour_context ac[1];
  uint8_t output[64];
  char *output_hex;

  /* from wikipedia */
  TEST_ARCFOUR("Key", "Plaintext", "bbf316e8d940af0ad3");
  TEST_ARCFOUR("Wiki", "pedia", "1021bf0420");
  TEST_ARCFOUR("Secret", "Attack at dawn", "45a01f645fc35b383552544b9bf5");

}

TEST(arcfour);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
