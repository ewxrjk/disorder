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

static void test_casefold(void) {
  uint32_t c, l;
  const char *input, *canon_folded, *compat_folded, *canon_expected, *compat_expected;

  /* This isn't a very exhaustive test.  Unlike for normalization, there don't
   * seem to be any public test vectors for these algorithms. */
  
  for(c = 1; c < 256; ++c) {
    input = utf32_to_utf8(&c, 1, 0);
    canon_folded = utf8_casefold_canon(input, strlen(input), 0);
    compat_folded = utf8_casefold_compat(input, strlen(input), 0);
    switch(c) {
    default:
      if((c >= 'A' && c <= 'Z')
	 || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
	l = c ^ 0x20;
      else
	l = c;
      break;
    case 0xB5:				/* MICRO SIGN */
      l = 0x3BC;			/* GREEK SMALL LETTER MU */
      break;
    case 0xDF:				/* LATIN SMALL LETTER SHARP S */
      check_string(canon_folded, "ss");
      check_string(compat_folded, "ss");
      l = 0;
      break;
    }
    if(l) {
      uint32_t *d;
      /* Case-folded data is now normalized */
      d = utf32_decompose_canon(&l, 1, 0);
      canon_expected = utf32_to_utf8(d, utf32_len(d), 0);
      if(strcmp(canon_folded, canon_expected)) {
	fprintf(stderr, "%s:%d: canon-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(canon_folded), format(canon_expected));
	count_error();
      }
      ++tests;
      d = utf32_decompose_compat(&l, 1, 0);
      compat_expected = utf32_to_utf8(d, utf32_len(d), 0);
      if(strcmp(compat_folded, compat_expected)) {
	fprintf(stderr, "%s:%d: compat-casefolding %#lx got '%s', expected '%s'\n",
		__FILE__, __LINE__, (unsigned long)c,
		format(compat_folded), format(compat_expected));
	count_error();
      }
      ++tests;
    }
  }
  check_string(utf8_casefold_canon("", 0, 0), "");
}

TEST(casefold);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
