/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell, 2018 Mark Wooding
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
#include "salsa208.h"

#define TEST_SALSA208(K, N, P, C) do {                                  \
  Kbytes = unhex(K, &Klen);                                             \
  salsa208_setkey(ac, Kbytes, Klen);                                    \
  Nbytes = unhex(N, &Nlen);                                             \
  salsa208_setnonce(ac, Nbytes, Nlen);                                  \
  Pbytes = unhex(P, &Plen);                                             \
  salsa208_stream(ac, Pbytes, output, Plen);                            \
  output_hex = hex(output, Plen);                                       \
  check_string(output_hex, C);                                          \
} while(0)
  
static void test_salsa208(void) {
  salsa208_context ac[1];
  uint8_t output[80], *Kbytes, *Nbytes, *Pbytes;
  char *output_hex;
  size_t Klen, Nlen, Plen;

  /* from the eStream submission */
  TEST_SALSA208("0f62b5085bae0154a7fa4da0f34699ec3f92e5388bde3184d72a7dd02376c91c",
                "288ff65dc42b92f9",
                "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "36ceb42e23ce2fed61d1a4e5a6e0a600dcca12ce4f1316c175c0bde0825d90972f574a7a25665fe6c3b91a70f1b83795330f5cfa8922c8f9b0589beade0b1432");

  /* test against Catacomb implementation; checks XOR, state stepping */
  TEST_SALSA208("ce9b04eeb18bb1434d6f534880d8516ff65158f60832325269b5c5e517adb27e",
                "41f4e1e0db3ef6f2",
                "d3df3ab24ce7ef617148fdd461757d81b1b3abecb808b4e3ebb542675597c0ab6a4ae3888a7717a8eb2f80b8a3ca33e8c4280757b2f71d409c8618ee50648e35810dfdcbb3ad9436368fde5e645ef019",
                "3132381a28814d1989bcf09656e64a0ee8c6dd723a3ba5f6a02111f86f5156321ea7300976b2393821d44c425754f6cc08b755ea07287cc77fead40c581259d24d127880b7597fc6a9ea8fba89dd3f4c");
}

TEST(salsa208);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
