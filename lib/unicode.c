/*
 * This file is part of DisOrder
 * Copyright (C) 2007, 2009, 2013 Richard Kettlewell
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
/** @file lib/unicode.c
 * @brief Unicode support functions
 *
 * Here by UTF-8 and UTF-8 we mean the encoding forms of those names (not the
 * encoding schemes).  The primary encoding form is UTF-32 but convenience
 * wrappers using UTF-8 are provided for a number of functions.
 *
 * The idea is that all the strings that hit the database will be in a
 * particular normalization form, and for the search and tags database
 * in case-folded form, so they can be naively compared within the
 * database code.
 *
 * As the code stands this guarantee is not well met!
 *
 * Subpages:
 * - @ref utf32props
 * - @ref utftransform
 * - @ref utf32iterator
 * - @ref utf32
 * - @ref utf8
 */

#include "common.h"

#include "mem.h"
#include "vector.h"
#include "unicode.h"
#include "unidata.h"

/** @defgroup utf32props Unicode Code Point Properties */
/*@{*/

static const struct unidata *utf32__unidata_hard(uint32_t c);

/** @brief Find definition of code point @p c
 * @param c Code point
 * @return Pointer to @ref unidata structure for @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 * The returned pointer is NOT guaranteed to be unique to @p c.
 */
static inline const struct unidata *utf32__unidata(uint32_t c) {
  /* The bottom half of the table contains almost everything of interest
   * and we can just return the right thing straight away */
  if(c < UNICODE_BREAK_START)
    return &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];
  else
    return utf32__unidata_hard(c);
}

/** @brief Find definition of code point @p c
 * @param c Code point
 * @return Pointer to @ref unidata structure for @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 * The returned pointer is NOT guaranteed to be unique to @p c.
 *
 * Don't use this function (although it will work fine) - use utf32__unidata()
 * instead.
 */
static const struct unidata *utf32__unidata_hard(uint32_t c) {
  if(c < UNICODE_BREAK_START)
    return &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];
  /* Within the break everything is unassigned */
  if(c < UNICODE_BREAK_END)
    return utf32__unidata(0xFFFF);      /* guaranteed to be Cn */
  /* Planes 15 and 16 are (mostly) private use */
  if((c >= 0xF0000 && c <= 0xFFFFD)
     || (c >= 0x100000 && c <= 0x10FFFD))
    return utf32__unidata(0xE000);      /* first Co code point */
  /* Everything else above the break top is unassigned */
  if(c >= UNICODE_BREAK_TOP)
    return utf32__unidata(0xFFFF);      /* guaranteed to be Cn */
  /* Currently the rest is language tags and variation selectors */
  c -= (UNICODE_BREAK_END - UNICODE_BREAK_START);
  return &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];
}

/** @brief Return the combining class of @p c
 * @param c Code point
 * @return Combining class of @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
static inline int utf32__combining_class(uint32_t c) {
  return utf32__unidata(c)->ccc;
}

/** @brief Return the combining class of @p c
 * @param c Code point
 * @return Combining class of @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
int utf32_combining_class(uint32_t c) {
  return utf32__combining_class(c);
}

/** @brief Return the General_Category value for @p c
 * @param c Code point
 * @return General_Category property value
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
static inline enum unicode_General_Category utf32__general_category(uint32_t c) {
  return utf32__unidata(c)->general_category;
}

/** @brief Determine Grapheme_Break property
 * @param c Code point
 * @return Grapheme_Break property value of @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
static inline enum unicode_Grapheme_Break utf32__grapheme_break(uint32_t c) {
  return utf32__unidata(c)->grapheme_break;
}

/** @brief Determine Word_Break property
 * @param c Code point
 * @return Word_Break property value of @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
static inline enum unicode_Word_Break utf32__word_break(uint32_t c) {
  return utf32__unidata(c)->word_break;
}

/** @brief Determine Sentence_Break property
 * @param c Code point
 * @return Word_Break property value of @p c
 *
 * @p c can be any 32-bit value, a sensible value will be returned regardless.
 */
static inline enum unicode_Sentence_Break utf32__sentence_break(uint32_t c) {
  return utf32__unidata(c)->sentence_break;
}

/** @brief Return true if @p c is ignorable for boundary specifications
 * @param wb Word break property value
 * @return non-0 if @p wb is unicode_Word_Break_Extend or unicode_Word_Break_Format
 */
static inline int utf32__boundary_ignorable(enum unicode_Word_Break wb) {
  return (wb == unicode_Word_Break_Extend
          || wb == unicode_Word_Break_Format);
}

/** @brief Return the canonical decomposition of @p c
 * @param c Code point
 * @return 0-terminated canonical decomposition, or 0
 */
static inline const uint32_t *utf32__decomposition_canon(uint32_t c) {
  const struct unidata *const data = utf32__unidata(c);
  const uint32_t *const decomp = data->decomp;

  if(decomp && !(data->flags & unicode_compatibility_decomposition))
    return decomp;
  else
    return 0;
}

/** @brief Return the compatibility decomposition of @p c
 * @param c Code point
 * @return 0-terminated decomposition, or 0
 */
static inline const uint32_t *utf32__decomposition_compat(uint32_t c) {
  return utf32__unidata(c)->decomp;
}

/*@}*/
/** @defgroup utftransform Functions that transform between different Unicode encoding forms */
/*@{*/

/** @brief Convert UTF-32 to UTF-8
 * @param s Source string
 * @param ns Length of source string in code points
 * @param ndp Where to store length of destination string (or NULL)
 * @return Newly allocated destination string or NULL on error
 *
 * If the UTF-32 is not valid then NULL is returned.  A UTF-32 code point is
 * invalid if:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 *
 * The return value is always 0-terminated.  The value returned via @p *ndp
 * does not include the terminator.
 */
char *utf32_to_utf8(const uint32_t *s, size_t ns, size_t *ndp) {
  struct dynstr d;
  uint32_t c;

  dynstr_init(&d);
  while(ns > 0) {
    c = *s++;
    if(c < 0x80)
      dynstr_append(&d, c);
    else if(c < 0x0800) {
      dynstr_append(&d, 0xC0 | (c >> 6));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else if(c < 0x10000) {
      if(c >= 0xD800 && c <= 0xDFFF)
	goto error;
      dynstr_append(&d, 0xE0 | (c >> 12));
      dynstr_append(&d, 0x80 | ((c >> 6) & 0x3F));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else if(c < 0x110000) {
      dynstr_append(&d, 0xF0 | (c >> 18));
      dynstr_append(&d, 0x80 | ((c >> 12) & 0x3F));
      dynstr_append(&d, 0x80 | ((c >> 6) & 0x3F));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else
      goto error;
    --ns;
  }
  dynstr_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  return 0;
}

/** @brief Convert UTF-8 to UTF-32
 * @param s Source string
 * @param ns Length of source string in code points
 * @param ndp Where to store length of destination string (or NULL)
 * @return Newly allocated destination string or NULL on error
 *
 * The return value is always 0-terminated.  The value returned via @p *ndp
 * does not include the terminator.
 *
 * If the UTF-8 is not valid then NULL is returned.  A UTF-8 sequence
 * for a code point is invalid if:
 * - it is not the shortest possible sequence for the code point
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint32_t *utf8_to_utf32(const char *s, size_t ns, size_t *ndp) {
  struct dynstr_ucs4 d;
  uint32_t c32;
  const uint8_t *ss = (const uint8_t *)s;
  int n;

  dynstr_ucs4_init(&d);
  while(ns > 0) {
    const struct unicode_utf8_row *const r = &unicode_utf8_valid[*ss];
    if(r->count <= ns) {
      switch(r->count) {
      case 1:
        c32 = *ss;
        break;
      case 2:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x1F;
        break;
      case 3:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x0F;
        break;
      case 4:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x07;
        break;
      default:
        goto error;
      }
    } else
      goto error;
    for(n = 1; n < r->count; ++n) {
      if(ss[n] < 0x80 || ss[n] > 0xBF)
        goto error;
      c32 = (c32 << 6) | (ss[n] & 0x3F);
    }
    dynstr_ucs4_append(&d, c32);
    ss += r->count;
    ns -= r->count;
  }
  dynstr_ucs4_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  return 0;
}

/** @brief Convert UTF-16 to UTF-8
 * @param s Source string
 * @param ns Length of source string in code points
 * @param ndp Where to store length of destination string (or NULL)
 * @return Newly allocated destination string or NULL on error
 *
 * If the UTF-16 is not valid then NULL is returned.  A UTF-16 sequence t is
 * invalid if it contains an incomplete surrogate.
 *
 * The return value is always 0-terminated.  The value returned via @p *ndp
 * does not include the terminator.
 */
char *utf16_to_utf8(const uint16_t *s, size_t ns, size_t *ndp) {
  struct dynstr d;
  uint32_t c;

  dynstr_init(&d);
  while(ns > 0) {
    c = *s++;
    --ns;
    if(c >= 0xD800 && c <= 0xDBFF) {
      if(ns && *s >= 0xDC00 && c <= 0xDFFF)
        c = ((c - 0xD800) << 10) + (*s++ - 0xDC00) + 0x10000;
      else
        goto error;
    } else if(c >= 0xDC00 && c <= 0xDFFF)
      goto error;
    if(c < 0x80)
      dynstr_append(&d, c);
    else if(c < 0x0800) {
      dynstr_append(&d, 0xC0 | (c >> 6));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else if(c < 0x10000) {
      if(c >= 0xD800 && c <= 0xDFFF)
	goto error;
      dynstr_append(&d, 0xE0 | (c >> 12));
      dynstr_append(&d, 0x80 | ((c >> 6) & 0x3F));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else if(c < 0x110000) {
      dynstr_append(&d, 0xF0 | (c >> 18));
      dynstr_append(&d, 0x80 | ((c >> 12) & 0x3F));
      dynstr_append(&d, 0x80 | ((c >> 6) & 0x3F));
      dynstr_append(&d, 0x80 | (c & 0x3F));
    } else
      goto error;
  }
  dynstr_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  return 0;
}

/** @brief Convert UTF-8 to UTF-16
 * @param s Source string
 * @param ns Length of source string in code points
 * @param ndp Where to store length of destination string (or NULL)
 * @return Newly allocated destination string or NULL on error
 *
 * The return value is always 0-terminated.  The value returned via @p *ndp
 * does not include the terminator.
 *
 * If the UTF-8 is not valid then NULL is returned.  A UTF-8 sequence
 * for a code point is invalid if:
 * - it is not the shortest possible sequence for the code point
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint16_t *utf8_to_utf16(const char *s, size_t ns, size_t *ndp) {
  struct dynstr_utf16 d;
  uint32_t c32;
  const uint8_t *ss = (const uint8_t *)s;
  int n;

  dynstr_utf16_init(&d);
  while(ns > 0) {
    const struct unicode_utf8_row *const r = &unicode_utf8_valid[*ss];
    if(r->count <= ns) {
      switch(r->count) {
      case 1:
        c32 = *ss;
        break;
      case 2:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x1F;
        break;
      case 3:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x0F;
        break;
      case 4:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          goto error;
        c32 = *ss & 0x07;
        break;
      default:
        goto error;
      }
    } else
      goto error;
    for(n = 1; n < r->count; ++n) {
      if(ss[n] < 0x80 || ss[n] > 0xBF)
        goto error;
      c32 = (c32 << 6) | (ss[n] & 0x3F);
    }
    if(c32 >= 0x10000) {
      c32 -= 0x10000;
      dynstr_utf16_append(&d, 0xD800 + (c32 >> 10));
      dynstr_utf16_append(&d, 0xDC00 + (c32 & 0x03FF));
    } else
      dynstr_utf16_append(&d, c32);
    ss += r->count;
    ns -= r->count;
  }
  dynstr_utf16_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  return 0;
}

/** @brief Test whether [s,s+ns) is valid UTF-8
 * @param s Start of string
 * @param ns Length of string
 * @return non-0 if @p s is valid UTF-8, 0 if it is not valid
 *
 * This function is intended to be much faster than calling utf8_to_utf32() and
 * throwing away the result.
 */
int utf8_valid(const char *s, size_t ns) {
  const uint8_t *ss = (const uint8_t *)s;
  while(ns > 0) {
    const struct unicode_utf8_row *const r = &unicode_utf8_valid[*ss];
    if(r->count <= ns) {
      switch(r->count) {
      case 1:
        break;
      case 2:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          return 0;
        break;
      case 3:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          return 0;
        if(ss[2] < 0x80 || ss[2] > 0xBF)
          return 0;
        break;
      case 4:
        if(ss[1] < r->min2 || ss[1] > r->max2)
          return 0;
        if(ss[2] < 0x80 || ss[2] > 0xBF)
          return 0;
        if(ss[3] < 0x80 || ss[3] > 0xBF)
          return 0;
        break;
      default:
        return 0;
      }
    } else
      return 0;
    ss += r->count;
    ns -= r->count;
  }
  return 1;
}

/*@}*/
/** @defgroup utf32iterator UTF-32 string iterators */
/*@{*/

struct utf32_iterator_data {
  /** @brief Start of string */
  const uint32_t *s;

  /** @brief Length of string */
  size_t ns;

  /** @brief Current position */
  size_t n;

  /** @brief Last two non-ignorable characters or (uint32_t)-1
   *
   * last[1] is the non-Extend/Format character just before position @p n;
   * last[0] is the one just before that.
   *
   * Exception 1: if there is no such non-Extend/Format character then an
   * Extend/Format character is accepted instead.
   *
   * Exception 2: if there is no such character even taking that into account
   * the value is (uint32_t)-1.
   */
  uint32_t last[2];

  /** @brief Tailoring for Word_Break */
  unicode_property_tailor *word_break;
};

/** @brief Initialize an internal private iterator
 * @param it Iterator
 * @param s Start of string
 * @param ns Length of string
 * @param n Absolute position
 */
static void utf32__iterator_init(utf32_iterator it,
                                 const uint32_t *s, size_t ns, size_t n) {
  it->s = s;
  it->ns = ns;
  it->n = 0;
  it->last[0] = it->last[1] = -1;
  it->word_break = 0;
  utf32_iterator_set(it, n);
}

/** @brief Create a new iterator pointing at the start of a string
 * @param s Start of string
 * @param ns Length of string
 * @return New iterator
 */
utf32_iterator utf32_iterator_new(const uint32_t *s, size_t ns) {
  utf32_iterator it = xmalloc(sizeof *it);
  utf32__iterator_init(it, s, ns, 0);
  return it;
}

/** @brief Tailor this iterator's interpretation of the Word_Break property.
 * @param it Iterator
 * @param pt Property tailor function or NULL
 *
 * After calling this the iterator will call @p pt to determine the Word_Break
 * property of each code point.  If it returns -1 the default value will be
 * used otherwise the returned value will be used.
 *
 * @p pt can be NULL to revert to the default value of the property.
 *
 * It is safe to call this function at any time; the iterator's internal state
 * will be reset to suit the new tailoring.
 */
void utf32_iterator_tailor_word_break(utf32_iterator it,
                                      unicode_property_tailor *pt) {
  it->word_break = pt;
  utf32_iterator_set(it, it->n);
}

static inline enum unicode_Word_Break utf32__iterator_word_break(utf32_iterator it,
                                                                 uint32_t c) {
  if(!it->word_break)
    return utf32__word_break(c);
  else {
    const int t = it->word_break(c);

    if(t < 0)
      return utf32__word_break(c);
    else
      return t;
  }
}

/** @brief Destroy an iterator
 * @param it Iterator
 */
void utf32_iterator_destroy(utf32_iterator it) {
  xfree(it);
}

/** @brief Find the current position of an interator
 * @param it Iterator
 */
size_t utf32_iterator_where(utf32_iterator it) {
  return it->n;
}

/** @brief Set an iterator's absolute position
 * @param it Iterator
 * @param n Absolute position
 * @return 0 on success, non-0 on error
 *
 * It is an error to position the iterator outside the string (but acceptable
 * to point it at the hypothetical post-final character).  If an invalid value
 * of @p n is specified then the iterator is not changed.
 *
 * This function works by backing up and then advancing to reconstruct the
 * iterator's internal state for position @p n.  The worst case will be O(n)
 * time complexity (with a worse constant factor that utf32_iterator_advance())
 * but the typical case is essentially constant-time.
 */
int utf32_iterator_set(utf32_iterator it, size_t n) {
  /* We can't just jump to position @p n; the @p last[] values will be wrong.
   * What we need is to jump a bit behind @p n and then advance forward,
   * updating @p last[] along the way.  How far back?  We need to cross two
   * non-ignorable code points as we advance forwards, so we'd better pass two
   * such characters on the way back (if such are available).
   */
  size_t m;

  if(n > it->ns)                        /* range check */
    return -1;
  /* Walk backwards skipping ignorable code points */
  m = n;
  while(m > 0
        && (utf32__boundary_ignorable(utf32__iterator_word_break(it,
                                                                 it->s[m-1]))))
    --m;
  /* Either m=0 or s[m-1] is not ignorable */
  if(m > 0) {
    --m;
    /* s[m] is our first non-ignorable code; look for a second in the same
       way **/
    while(m > 0
          && (utf32__boundary_ignorable(utf32__iterator_word_break(it,
                                                                   it->s[m-1]))))
      --m;
    /* Either m=0 or s[m-1] is not ignorable */
    if(m > 0)
      --m;
  }
  it->last[0] = it->last[1] = -1;
  it->n = m;
  return utf32_iterator_advance(it, n - m);
}

/** @brief Advance an iterator
 * @param it Iterator
 * @param count Number of code points to advance by
 * @return 0 on success, non-0 on error
 *
 * It is an error to advance an iterator beyond the hypothetical post-final
 * character of the string.  If an invalid value of @p n is specified then the
 * iterator is not changed.
 *
 * This function has O(n) time complexity: it works by advancing naively
 * forwards through the string.
 */
int utf32_iterator_advance(utf32_iterator it, size_t count) {
  if(count <= it->ns - it->n) {
    while(count > 0) {
      const uint32_t c = it->s[it->n];
      const enum unicode_Word_Break wb = utf32__iterator_word_break(it, c);
      if(it->last[1] == (uint32_t)-1
         || !utf32__boundary_ignorable(wb)) {
        it->last[0] = it->last[1];
        it->last[1] = c;
      }
      ++it->n;
      --count;
    }
    return 0;
  } else
    return -1;
}

/** @brief Find the current code point
 * @param it Iterator
 * @return Current code point or 0
 *
 * If the iterator points at the hypothetical post-final character of the
 * string then 0 is returned.  NB that this doesn't mean that there aren't any
 * 0 code points inside the string!
 */
uint32_t utf32_iterator_code(utf32_iterator it) {
  if(it->n < it->ns)
    return it->s[it->n];
  else
    return 0;
}

/** @brief Test for a grapheme boundary
 * @param it Iterator
 * @return Non-0 if pointing just after a grapheme boundary, otherwise 0
 *
 * This function identifies default grapheme cluster boundaries as described in
 * UAX #29 s3.  It returns non-0 if @p it points at the code point just after a
 * grapheme cluster boundary (including the hypothetical code point just after
 * the end of the string).
 */
int utf32_iterator_grapheme_boundary(utf32_iterator it) {
  uint32_t before, after;
  enum unicode_Grapheme_Break gbbefore, gbafter;
  /* GB1 and GB2 */
  if(it->n == 0 || it->n == it->ns)
    return 1;
  /* Now we know that s[n-1] and s[n] are safe to inspect */
  /* GB3 */
  before = it->s[it->n-1];
  after = it->s[it->n];
  if(before == 0x000D && after == 0x000A)
    return 0;
  gbbefore = utf32__grapheme_break(before);
  gbafter = utf32__grapheme_break(after);
  /* GB4 */
  if(gbbefore == unicode_Grapheme_Break_Control
     || before == 0x000D
     || before == 0x000A)
    return 1;
  /* GB5 */
  if(gbafter == unicode_Grapheme_Break_Control
     || after == 0x000D
     || after == 0x000A)
    return 1;
  /* GB6 */
  if(gbbefore == unicode_Grapheme_Break_L
     && (gbafter == unicode_Grapheme_Break_L
         || gbafter == unicode_Grapheme_Break_V
         || gbafter == unicode_Grapheme_Break_LV
         || gbafter == unicode_Grapheme_Break_LVT))
    return 0;
  /* GB7 */
  if((gbbefore == unicode_Grapheme_Break_LV
      || gbbefore == unicode_Grapheme_Break_V)
     && (gbafter == unicode_Grapheme_Break_V
         || gbafter == unicode_Grapheme_Break_T))
    return 0;
  /* GB8 */
  if((gbbefore == unicode_Grapheme_Break_LVT
      || gbbefore == unicode_Grapheme_Break_T)
     && gbafter == unicode_Grapheme_Break_T)
    return 0;
  /* GB9 */
  if(gbafter == unicode_Grapheme_Break_Extend)
    return 0;
  /* GB9a */
  if(gbafter == unicode_Grapheme_Break_SpacingMark)
    return 0;
  /* GB9b */
  if(gbbefore == unicode_Grapheme_Break_Prepend)
    return 0;
  /* GB10 */
  return 1;

}

/** @brief Test for a word boundary
 * @param it Iterator
 * @return Non-0 if pointing just after a word boundary, otherwise 0
 *
 * This function identifies default word boundaries as described in UAX #29 s4.
 * It returns non-0 if @p it points at the code point just after a word
 * boundary (including the hypothetical code point just after the end of the
 * string) and 0 otherwise.
 */
int utf32_iterator_word_boundary(utf32_iterator it) {
  uint32_t before, after;
  enum unicode_Word_Break wbtwobefore, wbbefore, wbafter, wbtwoafter;
  size_t nn;

  /* WB1 and WB2 */
  if(it->n == 0 || it->n == it->ns)
    return 1;
  before = it->s[it->n-1];
  after = it->s[it->n];
  /* WB3 */
  if(before == 0x000D && after == 0x000A)
    return 0;
  /* WB3a */
  if(utf32__iterator_word_break(it, before) == unicode_Word_Break_Newline
     || before == 0x000D
     || before == 0x000A)
    return 1;
  /* WB3b */
  if(utf32__iterator_word_break(it, after) == unicode_Word_Break_Newline
     || after == 0x000D
     || after == 0x000A)
    return 1;
  /* WB4 */
  /* (!Sep) x (Extend|Format) as in UAX #29 s6.2 */
  if(utf32__sentence_break(before) != unicode_Sentence_Break_Sep
     && utf32__boundary_ignorable(utf32__iterator_word_break(it, after)))
    return 0;
  /* Gather the property values we'll need for the rest of the test taking the
   * s6.2 changes into account */
  /* First we look at the code points after the proposed boundary */
  nn = it->n;                           /* <it->ns */
  wbafter = utf32__iterator_word_break(it, it->s[nn++]);
  if(!utf32__boundary_ignorable(wbafter)) {
    /* X (Extend|Format)* -> X */
    while(nn < it->ns
          && utf32__boundary_ignorable(utf32__iterator_word_break(it,
                                                                  it->s[nn])))
      ++nn;
  }
  /* It's possible now that nn=ns */
  if(nn < it->ns)
    wbtwoafter = utf32__iterator_word_break(it, it->s[nn]);
  else
    wbtwoafter = unicode_Word_Break_Other;

  /* We've already recorded the non-ignorable code points before the proposed
   * boundary */
  wbbefore = utf32__iterator_word_break(it, it->last[1]);
  wbtwobefore = utf32__iterator_word_break(it, it->last[0]);

  /* WB5 */
  if(wbbefore == unicode_Word_Break_ALetter
     && wbafter == unicode_Word_Break_ALetter)
    return 0;
  /* WB6 */
  if(wbbefore == unicode_Word_Break_ALetter
     && (wbafter == unicode_Word_Break_MidLetter
         || wbafter == unicode_Word_Break_MidNumLet)
     && wbtwoafter == unicode_Word_Break_ALetter)
    return 0;
  /* WB7 */
  if(wbtwobefore == unicode_Word_Break_ALetter
     && (wbbefore == unicode_Word_Break_MidLetter
         || wbbefore == unicode_Word_Break_MidNumLet)
     && wbafter == unicode_Word_Break_ALetter)
    return 0;
  /* WB8 */
  if(wbbefore == unicode_Word_Break_Numeric
     && wbafter == unicode_Word_Break_Numeric)
    return 0;
  /* WB9 */
  if(wbbefore == unicode_Word_Break_ALetter
     && wbafter == unicode_Word_Break_Numeric)
    return 0;
  /* WB10 */
  if(wbbefore == unicode_Word_Break_Numeric
     && wbafter == unicode_Word_Break_ALetter)
    return 0;
   /* WB11 */
  if(wbtwobefore == unicode_Word_Break_Numeric
     && (wbbefore == unicode_Word_Break_MidNum
         || wbbefore == unicode_Word_Break_MidNumLet)
     && wbafter == unicode_Word_Break_Numeric)
    return 0;
  /* WB12 */
  if(wbbefore == unicode_Word_Break_Numeric
     && (wbafter == unicode_Word_Break_MidNum
         || wbafter == unicode_Word_Break_MidNumLet)
     && wbtwoafter == unicode_Word_Break_Numeric)
    return 0;
  /* WB13 */
  if(wbbefore == unicode_Word_Break_Katakana
     && wbafter == unicode_Word_Break_Katakana)
    return 0;
  /* WB13a */
  if((wbbefore == unicode_Word_Break_ALetter
      || wbbefore == unicode_Word_Break_Numeric
      || wbbefore == unicode_Word_Break_Katakana
      || wbbefore == unicode_Word_Break_ExtendNumLet)
     && wbafter == unicode_Word_Break_ExtendNumLet)
    return 0;
  /* WB13b */
  if(wbbefore == unicode_Word_Break_ExtendNumLet
     && (wbafter == unicode_Word_Break_ALetter
         || wbafter == unicode_Word_Break_Numeric
         || wbafter == unicode_Word_Break_Katakana))
    return 0;
  /* WB14 */
  return 1;
}

/*@}*/
/** @defgroup utf32 Functions that operate on UTF-32 strings */
/*@{*/

/** @brief Return the length of a 0-terminated UTF-32 string
 * @param s Pointer to 0-terminated string
 * @return Length of string in code points (excluding terminator)
 *
 * Unlike the conversion functions no validity checking is done on the string.
 */
size_t utf32_len(const uint32_t *s) {
  const uint32_t *t = s;

  while(*t)
    ++t;
  return (size_t)(t - s);
}

/** @brief Stably sort [s,s+ns) into descending order of combining class
 * @param s Start of array
 * @param ns Number of elements, must be at least 1
 * @param buffer Buffer of at least @p ns elements
 */
static void utf32__sort_ccc(uint32_t *s, size_t ns, uint32_t *buffer) {
  uint32_t *a, *b, *bp;
  size_t na, nb;

  switch(ns) {
  case 1:			/* 1-element array is always sorted */
    return;
  case 2:			/* 2-element arrays are trivial to sort */
    if(utf32__combining_class(s[0]) > utf32__combining_class(s[1])) {
      uint32_t tmp = s[0];
      s[0] = s[1];
      s[1] = tmp;
    }
    return;
  default:
    /* Partition the array */
    na = ns / 2;
    nb = ns - na;
    a = s;
    b = s + na;
    /* Sort the two halves of the array */
    utf32__sort_ccc(a, na, buffer);
    utf32__sort_ccc(b, nb, buffer);
    /* Merge them back into one, via the buffer */
    bp = buffer;
    while(na > 0 && nb > 0) {
      /* We want ascending order of combining class (hence <)
       * and we want stability within combining classes (hence <=)
       */
      if(utf32__combining_class(*a) <= utf32__combining_class(*b)) {
	*bp++ = *a++;
	--na;
      } else {
	*bp++ = *b++;
	--nb;
      }
    }
    while(na > 0) {
      *bp++ = *a++;
      --na;
    }
    while(nb > 0) {
      *bp++ = *b++;
      --nb;
    }
    memcpy(s, buffer,  ns * sizeof(uint32_t));
    return;
  }
}

/** @brief Put combining characters into canonical order
 * @param s Pointer to UTF-32 string
 * @param ns Length of @p s
 * @return 0 on success, non-0 on error
 *
 * @p s is modified in-place.  See Unicode 5.0 s3.11 for details of the
 * ordering.
 *
 * Currently we only support a maximum of 1024 combining characters after each
 * base character.  If this limit is exceeded then a non-0 value is returned.
 */
static int utf32__canonical_ordering(uint32_t *s, size_t ns) {
  size_t nc;
  uint32_t buffer[1024];

  /* The ordering amounts to a stable sort of each contiguous group of
   * characters with non-0 combining class. */
  while(ns > 0) {
    /* Skip non-combining characters */
    if(utf32__combining_class(*s) == 0) {
      ++s;
      --ns;
      continue;
    }
    /* We must now have at least one combining character; see how many
     * there are */
    for(nc = 1; nc < ns && utf32__combining_class(s[nc]) != 0; ++nc)
      ;
    if(nc > 1024)
      return -1;
    /* Sort the array */
    utf32__sort_ccc(s, nc, buffer);
    s += nc;
    ns -= nc;
  }
  return 0;
}

/* Magic numbers from UAX #15 s16 */
#define SBase 0xAC00
#define LBase 0x1100
#define VBase 0x1161
#define TBase 0x11A7
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

/** @brief Guts of the decomposition lookup functions */
#define utf32__decompose_one_generic(WHICH) do {                        \
  const uint32_t *dc = utf32__decomposition_##WHICH(c);	                \
  if(dc) {                                                              \
    /* Found a canonical decomposition in the table */                  \
    while(*dc)                                                          \
      utf32__decompose_one_##WHICH(d, *dc++);                           \
  } else if(c >= SBase && c < SBase + SCount) {                         \
    /* Mechanically decomposable Hangul syllable (UAX #15 s16) */       \
    const uint32_t SIndex = c - SBase;                                  \
    const uint32_t L = LBase + SIndex / NCount;                         \
    const uint32_t V = VBase + (SIndex % NCount) / TCount;              \
    const uint32_t T = TBase + SIndex % TCount;                         \
    dynstr_ucs4_append(d, L);                                           \
    dynstr_ucs4_append(d, V);                                           \
    if(T != TBase)                                                      \
      dynstr_ucs4_append(d, T);                                         \
  } else                                                                \
    /* Equal to own canonical decomposition */                          \
    dynstr_ucs4_append(d, c);                                           \
} while(0)

/** @brief Recursively compute the canonical decomposition of @p c
 * @param d Dynamic string to store decomposition in
 * @param c Code point to decompose (must be a valid!)
 * @return 0 on success, non-0 on error
 */
static void utf32__decompose_one_canon(struct dynstr_ucs4 *d, uint32_t c) {
  utf32__decompose_one_generic(canon);
}

/** @brief Recursively compute the compatibility decomposition of @p c
 * @param d Dynamic string to store decomposition in
 * @param c Code point to decompose (must be a valid!)
 * @return 0 on success, non-0 on error
 */
static void utf32__decompose_one_compat(struct dynstr_ucs4 *d, uint32_t c) {
  utf32__decompose_one_generic(compat);
}

/** @brief Magic utf32__compositions() return value for Hangul Choseong */
static const uint32_t utf32__hangul_L[1];

/** @brief Return the list of compositions that @p c starts
 * @param c Starter code point
 * @return Composition list or NULL
 *
 * For Hangul leading (Choseong) jamo we return the special value
 * utf32__hangul_L.  These code points are not listed as the targets of
 * canonical decompositions (make-unidata checks) so there is no confusion with
 * real decompositions here.
 */
static const uint32_t *utf32__compositions(uint32_t c) {
  const uint32_t *compositions = utf32__unidata(c)->composed;

  if(compositions)
    return compositions;
  /* Special-casing for Hangul */
  switch(utf32__grapheme_break(c)) {
  default:
    return 0;
  case unicode_Grapheme_Break_L:
    return utf32__hangul_L;
  }
}

/** @brief Composition step
 * @param s Start of string
 * @param ns Length of string
 * @return New length of string
 *
 * This is called from utf32__decompose_generic() to compose the result string
 * in place.
 */
static size_t utf32__compose(uint32_t *s, size_t ns) {
  const uint32_t *compositions;
  uint32_t *start = s, *t = s, *tt, cc;

  while(ns > 0) {
    uint32_t starter = *s++;
    int block_starters = 0;
    --ns;
    /* We don't attempt to compose the following things:
     * - final characters whatever kind they are
     * - non-starter characters
     * - starters that don't take part in a canonical decomposition mapping
     */
    if(ns == 0
       || utf32__combining_class(starter)
       || !(compositions = utf32__compositions(starter))) {
      *t++ = starter;
      continue;
    }
    if(compositions != utf32__hangul_L) {
      /* Where we'll put the eventual starter */
      tt = t++;
      do {
        /* See if we can find composition of starter+*s */
        const uint32_t cchar = *s, *cp = compositions;
        while((cc = *cp++)) {
          const uint32_t *decomp = utf32__decomposition_canon(cc);
          /* We know decomp[0] == starter */
          if(decomp[1] == cchar)
            break;
        }
        if(cc) {
          /* Found a composition: cc decomposes to starter,*s */
          starter = cc;
          compositions = utf32__compositions(starter);
          ++s;
          --ns;
        } else {
          /* No composition found. */
          const int class = utf32__combining_class(*s);
          if(class) {
            /* Transfer the uncomposable combining character to the output */
            *t++ = *s++;
            --ns;
            /* All the combining characters of the same class of the
             * uncomposable character are blocked by it, but there may be
             * others of higher class later.  We eat the uncomposable and
             * blocked characters and go back round the loop for that higher
             * class. */
            while(ns > 0 && utf32__combining_class(*s) == class) {
              *t++ = *s++;
              --ns;
            }
            /* Block any subsequent starters */
            block_starters = 1;
          } else {
            /* The uncombinable character is itself a starter, so we don't
             * transfer it to the output but instead go back round the main
             * loop. */
            break;
          }
        }
        /* Keep going while there are still characters and the starter takes
         * part in some composition */
      } while(ns > 0 && compositions
              && (!block_starters || utf32__combining_class(*s)));
      /* Store any remaining combining characters */
      while(ns > 0 && utf32__combining_class(*s)) {
        *t++ = *s++;
        --ns;
      }
      /* Store the resulting starter */
      *tt = starter;
    } else {
      /* Special-casing for Hangul
       *
       * If there are combining characters between the L and the V then they
       * will block the V and so no composition happens.  Similarly combining
       * characters between V and T will block the T and so we only get as far
       * as LV.
       */
      if(utf32__grapheme_break(*s) == unicode_Grapheme_Break_V) {
        const uint32_t V = *s++;
        const uint32_t LIndex = starter - LBase;
        const uint32_t VIndex = V - VBase;
        uint32_t TIndex;
        --ns;
        if(ns > 0
           && utf32__grapheme_break(*s) == unicode_Grapheme_Break_T) {
          /* We have an L V T sequence */
          const uint32_t T = *s++;
          TIndex = T - TBase;
          --ns;
        } else
          /* It's just L V */
          TIndex = 0;
        /* Compose to LVT or LV as appropriate */
        starter = (LIndex * VCount + VIndex) * TCount + TIndex + SBase;
      } /* else we only have L or LV and no V or T */
      *t++ = starter;
      /* There could be some combining characters that belong to the V or T.
       * These will be treated as non-starter characters at the top of the loop
       * and thuss transferred to the output. */
    }
  }
  return t - start;
}

/** @brief Guts of the composition and decomposition functions
 * @param WHICH @c canon or @c compat to choose decomposition
 * @param COMPOSE @c 0 or @c 1 to compose
 */
#define utf32__decompose_generic(WHICH, COMPOSE) do {   \
  struct dynstr_ucs4 d;                                 \
  uint32_t c;                                           \
                                                        \
  dynstr_ucs4_init(&d);                                 \
  while(ns) {                                           \
    c = *s++;                                           \
    if((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)    \
      goto error;                                       \
    utf32__decompose_one_##WHICH(&d, c);                \
    --ns;                                               \
  }                                                     \
  if(utf32__canonical_ordering(d.vec, d.nvec))          \
    goto error;                                         \
  if(COMPOSE)                                           \
    d.nvec = utf32__compose(d.vec, d.nvec);             \
  dynstr_ucs4_terminate(&d);                            \
  if(ndp)                                               \
    *ndp = d.nvec;                                      \
  return d.vec;                                         \
error:                                                  \
  xfree(d.vec);                                         \
  return 0;                                             \
} while(0)

/** @brief Canonically decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFD (Normalization Form D) of the string at @p s.  This implies
 * performing all canonical decompositions and then normalizing the order of
 * combining characters.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 *
 * See also:
 * - utf32_decompose_compat()
 * - utf32_compose_canon()
 */
uint32_t *utf32_decompose_canon(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(canon, 0);
}

/** @brief Compatibility decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFKD (Normalization Form KD) of the string at @p s.  This implies
 * performing all canonical and compatibility decompositions and then
 * normalizing the order of combining characters.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 *
 * See also:
 * - utf32_decompose_canon()
 * - utf32_compose_compat()
 */
uint32_t *utf32_decompose_compat(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(compat, 0);
}

/** @brief Canonically compose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFC (Normalization Form C) of the string at @p s.  This implies
 * performing all canonical decompositions, normalizing the order of combining
 * characters and then composing all unblocked primary compositables.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 *
 * See also:
 * - utf32_compose_compat()
 * - utf32_decompose_canon()
 */
uint32_t *utf32_compose_canon(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(canon, 1);
}

/** @brief Compatibility compose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFKC (Normalization Form KC) of the string at @p s.  This implies
 * performing all canonical and compatibility decompositions, normalizing the
 * order of combining characters and then composing all unblocked primary
 * compositables.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 *
 * See also:
 * - utf32_compose_canon()
 * - utf32_decompose_compat()
 */
uint32_t *utf32_compose_compat(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(compat, 1);
}

/** @brief Single-character case-fold and decompose operation */
#define utf32__casefold_one(WHICH) do {                                 \
  const uint32_t *cf = utf32__unidata(c)->casefold;                     \
  if(cf) {                                                              \
    /* Found a case-fold mapping in the table */                        \
    while(*cf)                                                          \
      utf32__decompose_one_##WHICH(&d, *cf++);                          \
  } else                                                                \
    utf32__decompose_one_##WHICH(&d, c);                                \
} while(0)

/** @brief Case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Case-fold the string at @p s according to full default case-folding rules
 * (s3.13) for caseless matching.  The result will be in NFD.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint32_t *utf32_casefold_canon(const uint32_t *s, size_t ns, size_t *ndp) {
  struct dynstr_ucs4 d;
  uint32_t c;
  size_t n;
  uint32_t *ss = 0;

  /* If the canonical decomposition of the string includes any combining
   * character that case-folds to a non-combining character then we must
   * normalize before we fold.  In Unicode 5.0.0 this means 0345 COMBINING
   * GREEK YPOGEGRAMMENI in its decomposition and the various characters that
   * canonically decompose to it. */
  for(n = 0; n < ns; ++n)
    if(utf32__unidata(s[n])->flags & unicode_normalize_before_casefold)
      break;
  if(n < ns) {
    /* We need a preliminary decomposition */
    if(!(ss = utf32_decompose_canon(s, ns, &ns)))
      return 0;
    s = ss;
  }
  dynstr_ucs4_init(&d);
  while(ns) {
    c = *s++;
    if((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
      goto error;
    utf32__casefold_one(canon);
    --ns;
  }
  if(utf32__canonical_ordering(d.vec, d.nvec))
    goto error;
  dynstr_ucs4_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  xfree(ss);
  return 0;
}

/** @brief Compatibility case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Case-fold the string at @p s according to full default case-folding rules
 * (s3.13) for compatibility caseless matching.  The result will be in NFKD.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint32_t *utf32_casefold_compat(const uint32_t *s, size_t ns, size_t *ndp) {
  struct dynstr_ucs4 d;
  uint32_t c;
  size_t n;
  uint32_t *ss = 0;

  for(n = 0; n < ns; ++n)
    if(utf32__unidata(s[n])->flags & unicode_normalize_before_casefold)
      break;
  if(n < ns) {
    /* We need a preliminary _canonical_ decomposition */
    if(!(ss = utf32_decompose_canon(s, ns, &ns)))
      return 0;
    s = ss;
  }
  /* This computes NFKD(toCaseFold(s)) */
#define compat_casefold_middle() do {                   \
  dynstr_ucs4_init(&d);                                 \
  while(ns) {                                           \
    c = *s++;                                           \
    if((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)    \
      goto error;                                       \
    utf32__casefold_one(compat);                        \
    --ns;                                               \
  }                                                     \
  if(utf32__canonical_ordering(d.vec, d.nvec))          \
    goto error;                                         \
} while(0)
  /* Do the inner (NFKD o toCaseFold) */
  compat_casefold_middle();
  /* We can do away with the NFD'd copy of the input now */
  xfree(ss);
  s = ss = d.vec;
  ns = d.nvec;
  /* Do the outer (NFKD o toCaseFold) */
  compat_casefold_middle();
  /* That's all */
  dynstr_ucs4_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  xfree(ss);
  return 0;
}

/** @brief Order a pair of UTF-32 strings
 * @param a First 0-terminated string
 * @param b Second 0-terminated string
 * @return -1, 0 or 1 for a less than, equal to or greater than b
 *
 * "Comparable to strcmp() at its best."
 */
int utf32_cmp(const uint32_t *a, const uint32_t *b) {
  while(*a && *b && *a == *b) {
    ++a;
    ++b;
  }
  return *a < *b ? -1 : (*a > *b ? 1 : 0);
}

/** @brief Identify a grapheme cluster boundary
 * @param s Start of string (must be NFD)
 * @param ns Length of string
 * @param n Index within string (in [0,ns].)
 * @return 1 at a grapheme cluster boundary, 0 otherwise
 *
 * This function identifies default grapheme cluster boundaries as described in
 * UAX #29 s3.  It returns non-0 if @p n points at the code point just after a
 * grapheme cluster boundary (including the hypothetical code point just after
 * the end of the string).
 *
 * This function uses utf32_iterator_set() internally; see that function for
 * remarks on performance.
 */
int utf32_is_grapheme_boundary(const uint32_t *s, size_t ns, size_t n) {
  struct utf32_iterator_data it[1];

  utf32__iterator_init(it, s, ns, n);
  return utf32_iterator_grapheme_boundary(it);
}

/** @brief Identify a word boundary
 * @param s Start of string (must be NFD)
 * @param ns Length of string
 * @param n Index within string (in [0,ns].)
 * @return 1 at a word boundary, 0 otherwise
 *
 * This function identifies default word boundaries as described in UAX #29 s4.
 * It returns non-0 if @p n points at the code point just after a word boundary
 * (including the hypothetical code point just after the end of the string).
 *
 * This function uses utf32_iterator_set() internally; see that function for
 * remarks on performance.
 */
int utf32_is_word_boundary(const uint32_t *s, size_t ns, size_t n) {
  struct utf32_iterator_data it[1];

  utf32__iterator_init(it, s, ns, n);
  return utf32_iterator_word_boundary(it);
}

/** @brief Split [s,ns) into multiple words
 * @param s Pointer to start of string
 * @param ns Length of string
 * @param nwp Where to store word count, or NULL
 * @param wbreak Word_Break property tailor, or NULL
 * @return Pointer to array of pointers to words
 *
 * The returned array is terminated by a NULL pointer and individual
 * strings are 0-terminated.
 */
uint32_t **utf32_word_split(const uint32_t *s, size_t ns, size_t *nwp,
                            unicode_property_tailor *wbreak) {
  struct utf32_iterator_data it[1];
  size_t b1 = 0, b2 = 0 ,i;
  int isword;
  struct vector32 v32[1];
  uint32_t *w;

  vector32_init(v32);
  utf32__iterator_init(it, s, ns, 0);
  it->word_break = wbreak;
  /* Work our way through the string stopping at each word break. */
  do {
    if(utf32_iterator_word_boundary(it)) {
      /* We've found a new boundary */
      b1 = b2;
      b2 = it->n;
      /*fprintf(stderr, "[%zu, %zu) is a candidate word\n", b1, b2);*/
      /* Inspect the characters between the boundary and form an opinion as to
       * whether they are a word or not */
      isword = 0;
      for(i = b1; i < b2; ++i) {
        switch(utf32__iterator_word_break(it, it->s[i])) {
        case unicode_Word_Break_ALetter:
        case unicode_Word_Break_Numeric:
        case unicode_Word_Break_Katakana:
          isword = 1;
          break;
        default:
          break;
        }
      }
      /* If it's a word add it to the list of results */
      if(isword) {
        const size_t len = b2 - b1;
        w = xcalloc_noptr(len + 1, sizeof(uint32_t));
        memcpy(w, it->s + b1, len * sizeof (uint32_t));
        w[len] = 0;
        vector32_append(v32, w);
      }
    }
  } while(!utf32_iterator_advance(it, 1));
  vector32_terminate(v32);
  if(nwp)
    *nwp = v32->nvec;
  return v32->vec;
}

/*@}*/
/** @defgroup utf8 Functions that operate on UTF-8 strings */
/*@{*/

/** @brief Wrapper to transform a UTF-8 string using the UTF-32 function */
#define utf8__transform(FN) do {                                \
  uint32_t *to32 = 0, *decomp32 = 0;                            \
  size_t nto32, ndecomp32;                                      \
  char *decomp8 = 0;                                            \
                                                                \
  if(!(to32 = utf8_to_utf32(s, ns, &nto32))) goto error;        \
  if(!(decomp32 = FN(to32, nto32, &ndecomp32))) goto error;     \
  decomp8 = utf32_to_utf8(decomp32, ndecomp32, ndp);            \
error:                                                          \
  xfree(to32);                                                  \
  xfree(decomp32);                                              \
  return decomp8;                                               \
} while(0)

/** @brief Canonically decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFD (Normalization Form D) of the string at @p s.  This implies
 * performing all canonical decompositions and then normalizing the order of
 * combining characters.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also:
 * - utf32_decompose_canon().
 * - utf8_decompose_compat()
 * - utf8_compose_canon()
 */
char *utf8_decompose_canon(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_decompose_canon);
}

/** @brief Compatibility decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFKD (Normalization Form KD) of the string at @p s.  This implies
 * performing all canonical and compatibility decompositions and then
 * normalizing the order of combining characters.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also:
 * - utf32_decompose_compat().
 * - utf8_decompose_canon()
 * - utf8_compose_compat()
 */
char *utf8_decompose_compat(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_decompose_compat);
}

/** @brief Canonically compose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFC (Normalization Form C) of the string at @p s.  This implies
 * performing all canonical decompositions, normalizing the order of combining
 * characters and then composing all unblocked primary compositables.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also:
 * - utf32_compose_canon()
 * - utf8_compose_compat()
 * - utf8_decompose_canon()
 */
char *utf8_compose_canon(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_compose_canon);
}

/** @brief Compatibility compose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Computes NFKC (Normalization Form KC) of the string at @p s.  This implies
 * performing all canonical and compatibility decompositions, normalizing the
 * order of combining characters and then composing all unblocked primary
 * compositables.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also:
 * - utf32_compose_compat()
 * - utf8_compose_canon()
 * - utf8_decompose_compat()
 */
char *utf8_compose_compat(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_compose_compat);
}

/** @brief Case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Case-fold the string at @p s according to full default case-folding rules
 * (s3.13).  The result will be in NFD.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 */
char *utf8_casefold_canon(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_casefold_canon);
}

/** @brief Compatibility case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL on error
 *
 * Case-fold the string at @p s according to full default case-folding rules
 * (s3.13).  The result will be in NFKD.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 */
char *utf8_casefold_compat(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_casefold_compat);
}

/** @brief Split [s,ns) into multiple words
 * @param s Pointer to start of string
 * @param ns Length of string
 * @param nwp Where to store word count, or NULL
 * @param wbreak Word_Break property tailor, or NULL
 * @return Pointer to array of pointers to words
 *
 * The returned array is terminated by a NULL pointer and individual
 * strings are 0-terminated.
 */
char **utf8_word_split(const char *s, size_t ns, size_t *nwp,
                       unicode_property_tailor *wbreak) {
  uint32_t *to32 = 0, **v32 = 0;
  size_t nto32, nv, n;
  char **v8 = 0, **ret = 0;

  if(!(to32 = utf8_to_utf32(s, ns, &nto32))) goto error;
  if(!(v32 = utf32_word_split(to32, nto32, &nv, wbreak))) goto error;
  v8 = xcalloc(sizeof (char *), nv + 1);
  for(n = 0; n < nv; ++n)
    if(!(v8[n] = utf32_to_utf8(v32[n], utf32_len(v32[n]), 0)))
      goto error;
  ret = v8;
  *nwp = nv;
  v8 = 0;                               /* don't free */
error:
  if(v8) {
    for(n = 0; n < nv; ++n)
      xfree(v8[n]);
    xfree(v8);
  }
  if(v32) {
    for(n = 0; n < nv; ++n)
      xfree(v32[n]);
    xfree(v32);
  }
  xfree(to32);
  return ret;
}


/*@}*/

/** @brief Return the length of a 0-terminated UTF-16 string
 * @param s Pointer to 0-terminated string
 * @return Length of string in code points (excluding terminator)
 *
 * Unlike the conversion functions no validity checking is done on the string.
 */
size_t utf16_len(const uint16_t *s) {
  const uint16_t *t = s;

  while(*t)
    ++t;
  return (size_t)(t - s);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
