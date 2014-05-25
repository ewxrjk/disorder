/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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

struct {
  const char *in;
  const char *expect[10];
} wtest[] = {
  /* Empty string */
  { "", { 0 } },
  /* Only whitespace and punctuation */
  { "    ", { 0 } },
  { " '   ", { 0 } },
  { " !  ", { 0 } },
  { " \"\"  ", { 0 } },
  { " @  ", { 0 } },
  /* Basics */
  { "wibble", { "wibble", 0 } },
  { " wibble", { "wibble", 0 } },
  { " wibble ", { "wibble", 0 } },
  { "wibble ", { "wibble", 0 } },
  { "wibble spong", { "wibble", "spong", 0 } },
  { " wibble  spong", { "wibble", "spong", 0 } },
  { " wibble  spong   ", { "wibble", "spong", 0 } },
  { "wibble   spong  ", { "wibble", "spong", 0 } },
  { "wibble   spong splat foo zot  ", { "wibble", "spong", "splat", "foo", "zot", 0 } },
  /* Apostrophes */
  { "wibble 'spong", { "wibble", "spong", 0 } },
  { " wibble's", { "wibble's", 0 } },
  { " wibblespong'   ", { "wibblespong", 0 } },
  { "wibble   sp''ong  ", { "wibble", "sp", "ong", 0 } },
};
#define NWTEST (sizeof wtest / sizeof *wtest)

static void test_words(void) {
  size_t t, nexpect, ngot, i;
  int right;
  
  for(t = 0; t < NWTEST; ++t) {
    char **got = utf8_word_split(wtest[t].in, strlen(wtest[t].in), &ngot, 0);

    assert(got != NULL);
    for(nexpect = 0; wtest[t].expect[nexpect]; ++nexpect)
      ;
    if(nexpect == ngot) {
      for(i = 0; i < ngot; ++i)
        if(strcmp(wtest[t].expect[i], got[i]))
          break;
      right = i == ngot;
    } else
      right = 0;
    if(!right) {
      fprintf(stderr, "word split %zu failed\n", t);
      fprintf(stderr, "input: %s\n", wtest[t].in);
      fprintf(stderr, "    | %-30s | %-30s\n",
              "expected", "got");
      for(i = 0; i < nexpect || i < ngot; ++i) {
        const char *e = i < nexpect ? wtest[t].expect[i] : "<none>";
        const char *g = i < ngot ? got[i] : "<none>";
        fprintf(stderr, " %2zu | %-30s | %-30s\n", i, e, g);
      }
      count_error();
    }
    ++tests;
  }
}

TEST(words);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
