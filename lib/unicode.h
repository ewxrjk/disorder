/*
 * This file is part of DisOrde
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file lib/unicode.h
 * @brief Unicode support functions
 */

#ifndef UNICODE_H
#define UNICODE_H

char *utf32_to_utf8(const uint32_t *s, size_t ns, size_t *nd);
uint32_t *utf8_to_utf32(const char *s, size_t ns, size_t *nd);

size_t utf32_len(const uint32_t *s);
int utf32_cmp(const uint32_t *a, const uint32_t *b);

uint32_t *utf32_decompose_canon(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_decompose_canon(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_decompose_compat(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_decompose_compat(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_casefold_canon(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_casefold_canon(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_casefold_compat(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_casefold_compat(const char *s, size_t ns, size_t *ndp);

int utf32_is_gcb(const uint32_t *s, size_t ns, size_t n);
int utf32_is_word_boundary(const uint32_t *s, size_t ns, size_t n);

#endif /* UNICODE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
