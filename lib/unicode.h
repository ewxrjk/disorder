/*
 * This file is part of DisOrde
 * Copyright (C) 2007, 2008, 2013 Richard Kettlewell
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
/** @file lib/unicode.h
 * @brief Unicode support functions
 */

#ifndef UNICODE_H
#define UNICODE_H

/** @brief Smart pointer into a string
 *
 * Iterators can be efficiently moved either forwards or back to the start of
 * the string.  They cannot (currently) efficiently be moved backwards.  Their
 * advantage is that they remember internal state to speed up boundary
 * detection.
 *
 * Iterators can point to any code point of the string, or to a hypothetical
 * post-final code point of value 0, but not outside the string.
 */
typedef struct utf32_iterator_data *utf32_iterator;

/** @brief Property tailor function
 * @param c Code point
 * @return Tailored property or -1 to use standard value
 *
 * See also utf32_iterator_tailor_word_break().
 */
typedef int unicode_property_tailor(uint32_t c);

char *utf32_to_utf8(const uint32_t *s, size_t ns, size_t *nd);
uint32_t *utf8_to_utf32(const char *s, size_t ns, size_t *nd);
char *utf16_to_utf8(const uint16_t *s, size_t ns, size_t *nd);
uint16_t *utf8_to_utf16(const char *s, size_t ns, size_t *nd);
int utf8_valid(const char *s, size_t ns);

int utf32_combining_class(uint32_t c);

size_t utf32_len(const uint32_t *s);
size_t utf16_len(const uint16_t *s);
int utf32_cmp(const uint32_t *a, const uint32_t *b);

uint32_t *utf32_decompose_canon(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_decompose_canon(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_decompose_compat(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_decompose_compat(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_compose_canon(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_compose_canon(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_compose_compat(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_compose_compat(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_casefold_canon(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_casefold_canon(const char *s, size_t ns, size_t *ndp);

uint32_t *utf32_casefold_compat(const uint32_t *s, size_t ns, size_t *ndp);
char *utf8_casefold_compat(const char *s, size_t ns, size_t *ndp);

int utf32_is_grapheme_boundary(const uint32_t *s, size_t ns, size_t n);
int utf32_is_word_boundary(const uint32_t *s, size_t ns, size_t n);

utf32_iterator utf32_iterator_new(const uint32_t *s, size_t ns);
void utf32_iterator_destroy(utf32_iterator it);

size_t utf32_iterator_where(utf32_iterator it);
int utf32_iterator_set(utf32_iterator it, size_t n);
int utf32_iterator_advance(utf32_iterator it, size_t n);
uint32_t utf32_iterator_code(utf32_iterator it);
int utf32_iterator_grapheme_boundary(utf32_iterator it);
int utf32_iterator_word_boundary(utf32_iterator it);
void utf32_iterator_tailor_word_break(utf32_iterator it,
                                      unicode_property_tailor *pt);

uint32_t **utf32_word_split(const uint32_t *s, size_t ns, size_t *nwp,
                            unicode_property_tailor *wbreak);
char **utf8_word_split(const char *s, size_t ns, size_t *nwp,
                            unicode_property_tailor *wbreak);

/** @brief Convert 0-terminated UTF-32 to UTF-8
 * @param s 0-terminated UTF-32 string
 * @return 0-terminated UTF-8 string or 0 on error
 *
 * See utf32_to_utf8() for possible causes of errors.
 */
static inline char *utf32nt_to_utf8(const uint32_t *s) {
  return utf32_to_utf8(s, utf32_len(s), 0);
}

/** @brief Convert 0-terminated UTF-16 to UTF-8
 * @param s 0-terminated UTF-16 string
 * @return 0-terminated UTF-8 string or 0 on error
 *
 * See utf16_to_utf8() for possible causes of errors.
 */
static inline char *utf16nt_to_utf8(const uint16_t *s) {
  return utf16_to_utf8(s, utf16_len(s), 0);
}

/** @brief Convert 0-terminated UTF-8 to UTF-32
 * @param s 0-terminated UTF-8 string
 * @return 0-terminated UTF-32 string or 0 on error
 *
 * See utf8_to_utf32() for possible causes of errors.
 */
static inline uint32_t *utf8nt_to_utf32(const char *s) {
  return utf8_to_utf32(s, strlen(s), 0);
}

/** @brief Convert 0-terminated UTF-8 to UTF-16
 * @param s 0-terminated UTF-8 string
 * @return 0-terminated UTF-16 string or 0 on error
 *
 * See utf8_to_utf16() for possible causes of errors.
 */
static inline uint16_t *utf8nt_to_utf16(const char *s) {
  return utf8_to_utf16(s, strlen(s), 0);
}

#if _WIN32
static inline wchar_t *utf8nt_to_wchar(const char *s) {
  return (wchar_t *)utf8nt_to_utf16(s);
}

static inline char *wcharnt_to_utf8(const wchar_t *s) {
  return utf16nt_to_utf8((const uint16_t *)s);
}
#else
static inline wchar_t *utf8nt_to_wchar(const char *s) {
  return (wchar_t *)utf8nt_to_utf32(s);
}

static inline char *wcharnt_to_utf8(const wchar_t *s) {
  return utf32nt_to_utf8((const uint32_t *)s);
}
#endif

#endif /* UNICODE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
