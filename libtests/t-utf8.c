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

static void test_utf8(void) {
  /* Test validutf8, convert to UCS-4, check the answer is right,
   * convert back to UTF-8, check we got to where we started */
#define U8(CHARS, WORDS) do {			\
  uint32_t *w = ucs4parse(WORDS);		\
  uint32_t *ucs;				\
  char *u8;					\
						\
  insist(validutf8(CHARS));			\
  ucs = utf8_to_utf32(CHARS, strlen(CHARS), 0); \
  insist(ucs != 0);				\
  insist(!utf32_cmp(w, ucs));			\
  u8 = utf32_to_utf8(ucs, utf32_len(ucs), 0);   \
  insist(u8 != 0);				\
  check_string(u8, CHARS);			\
} while(0)

#define validutf8(S) utf8_valid((S), strlen(S))

  /* empty string */

  U8("", "");
  
  /* ASCII characters */

  U8(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
     "0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x2a 0x2b 0x2c 0x2d "
     "0x2e 0x2f 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39 0x3a "
     "0x3b 0x3c 0x3d 0x3e 0x3f 0x40 0x41 0x42 0x43 0x44 0x45 0x46 0x47 "
     "0x48 0x49 0x4a 0x4b 0x4c 0x4d 0x4e 0x4f 0x50 0x51 0x52 0x53 0x54 "
     "0x55 0x56 0x57 0x58 0x59 0x5a 0x5b 0x5c 0x5d 0x5e 0x5f 0x60 0x61 "
     "0x62 0x63 0x64 0x65 0x66 0x67 0x68 0x69 0x6a 0x6b 0x6c 0x6d 0x6e "
     "0x6f 0x70 0x71 0x72 0x73 0x74 0x75 0x76 0x77 0x78 0x79 0x7a 0x7b "
     "0x7c 0x7d 0x7e");
  U8("\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\177",
     "0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xa 0xb 0xc 0xd 0xe 0xf 0x10 "
     "0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1a 0x1b 0x1c 0x1d "
     "0x1e 0x1f 0x7f");

  /* from RFC3629 */

  /* UTF8-2      = %xC2-DF UTF8-tail */
  insist(!validutf8("\xC0\x80"));
  insist(!validutf8("\xC1\x80"));
  insist(!validutf8("\xC2\x7F"));
  U8("\xC2\x80", "0x80");
  U8("\xDF\xBF", "0x7FF");
  insist(!validutf8("\xDF\xC0"));

  /*  UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
   *                %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
   */
  insist(!validutf8("\xE0\x9F\x80"));
  U8("\xE0\xA0\x80", "0x800");
  U8("\xE0\xBF\xBF", "0xFFF");
  insist(!validutf8("\xE0\xC0\xBF"));

  insist(!validutf8("\xE1\x80\x7F"));
  U8("\xE1\x80\x80", "0x1000");
  U8("\xEC\xBF\xBF", "0xCFFF");
  insist(!validutf8("\xEC\xC0\xBF"));
  
  U8("\xED\x80\x80", "0xD000");
  U8("\xED\x9F\xBF", "0xD7FF");
  insist(!validutf8("\xED\xA0\xBF"));

  insist(!validutf8("\xEE\x7f\x80"));
  U8("\xEE\x80\x80", "0xE000");
  U8("\xEF\xBF\xBF", "0xFFFF");
  insist(!validutf8("\xEF\xC0\xBF"));

  /*  UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
   *                %xF4 %x80-8F 2( UTF8-tail )
   */
  insist(!validutf8("\xF0\x8F\x80\x80"));
  U8("\xF0\x90\x80\x80", "0x10000");
  U8("\xF0\xBF\xBF\xBF", "0x3FFFF");
  insist(!validutf8("\xF0\xC0\x80\x80"));

  insist(!validutf8("\xF1\x80\x80\x7F"));
  U8("\xF1\x80\x80\x80", "0x40000");
  U8("\xF3\xBF\xBF\xBF", "0xFFFFF");
  insist(!validutf8("\xF3\xC0\x80\x80"));

  insist(!validutf8("\xF4\x80\x80\x7F"));
  U8("\xF4\x80\x80\x80", "0x100000");
  U8("\xF4\x8F\xBF\xBF", "0x10FFFF");
  insist(!validutf8("\xF4\x90\x80\x80"));
  insist(!validutf8("\xF4\x80\xFF\x80"));

  /* miscellaneous non-UTF-8 rubbish */
  insist(!validutf8("\x80"));
  insist(!validutf8("\xBF"));
  insist(!validutf8("\xC0"));
  insist(!validutf8("\xC0\x7F"));
  insist(!validutf8("\xC0\xC0"));
  insist(!validutf8("\xE0"));
  insist(!validutf8("\xE0\x7F"));
  insist(!validutf8("\xE0\xC0"));
  insist(!validutf8("\xE0\x80"));
  insist(!validutf8("\xE0\x80\x7f"));
  insist(!validutf8("\xE0\x80\xC0"));
  insist(!validutf8("\xF0"));
  insist(!validutf8("\xF0\x7F"));
  insist(!validutf8("\xF0\xC0"));
  insist(!validutf8("\xF0\x80"));
  insist(!validutf8("\xF0\x80\x7f"));
  insist(!validutf8("\xF0\x80\xC0"));
  insist(!validutf8("\xF0\x80\x80\x7f"));
  insist(!validutf8("\xF0\x80\x80\xC0"));
  insist(!validutf8("\xF5\x80\x80\x80"));
  insist(!validutf8("\xF8"));
}

TEST(utf8);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
