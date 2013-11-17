/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/charsetf.c @brief Character set conversion with free() */

#include "common.h"

#include "charset.h"
#include "mem.h"

char *mb2utf8_f(char *mb) {
  char *s = mb2utf8(mb);
  xfree(mb);
  return s;
}

char *utf82mb_f(char *utf8) {
  char *s = utf82mb(utf8);
  xfree(utf8);
  return s;
}

#if !_WIN32
char *any2utf8_f(const char *from,
                 char *any) {
  char *s = any2utf8(from, any);
  xfree(any);
  return s;
}

char *any2mb_f(const char *from,
               char *any) {
  char *s = any2mb(from, any);
  xfree(any);
  return s;
}

char *any2any_f(const char *from,
                const char *to,
                char *any) {
  char *s = any2any(from, to, any);
  xfree(any);
  return s;
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
