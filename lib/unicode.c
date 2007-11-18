/*
 * This file is part of DisOrder
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
 */

#include <config.h>
#include "types.h"

#include <string.h>
#include <stdio.h>		/* TODO */

#include "mem.h"
#include "vector.h"
#include "unicode.h"
#include "unidata.h"

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
 * @return Newly allocated destination string or NULL
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
  uint32_t c32, c;
  const uint8_t *ss = (const uint8_t *)s;

  dynstr_ucs4_init(&d);
  while(ns > 0) {
    c = *ss++;
    --ns;
    /* Acceptable UTF-8 is that which codes for Unicode Scalar Values
     * (Unicode 5.0.0 s3.9 D76)
     *
     * 0xxxxxxx
     * 7 data bits gives 0x00 - 0x7F and all are acceptable
     * 
     * 110xxxxx 10xxxxxx
     * 11 data bits gives 0x0000 - 0x07FF but only 0x0080 - 0x07FF acceptable
     *   
     * 1110xxxx 10xxxxxx 10xxxxxx
     * 16 data bits gives 0x0000 - 0xFFFF but only 0x0800 - 0xFFFF acceptable
     * (and UTF-16 surrogates are not acceptable)
     *
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
     * 21 data bits gives 0x00000000 - 0x001FFFFF
     * but only           0x00010000 - 0x0010FFFF are acceptable
     *
     * It is NOT always the case that the data bits in the first byte are
     * always non-0 for the acceptable values, so we do a separate check after
     * decoding.
     */
    if(c < 0x80)
      c32 = c;
    else if(c <= 0xDF) {
      if(ns < 1) goto error;
      c32 = c & 0x1F;
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      if(c32 < 0x80) goto error;
    } else if(c <= 0xEF) {
      if(ns < 2) goto error;
      c32 = c & 0x0F;
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      if(c32 < 0x0800 || (c32 >= 0xD800 && c32 <= 0xDFFF)) goto error;
    } else if(c <= 0xF7) {
      if(ns < 3) goto error;
      c32 = c & 0x07;
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      c = *ss++;
      if((c & 0xC0) != 0x80) goto error;
      c32 = (c32 << 6) | (c & 0x3F);
      if(c32 < 0x00010000 || c32 > 0x0010FFFF) goto error;
    } else
      goto error;
    dynstr_ucs4_append(&d, c32);
  }
  dynstr_ucs4_terminate(&d);
  if(ndp)
    *ndp = d.nvec;
  return d.vec;
error:
  xfree(d.vec);
  return 0;
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

/** @brief Return the combining class of @p c
 * @param c Code point
 * @return Combining class of @p c
 */
static inline int utf32__combining_class(uint32_t c) {
  if(c < UNICODE_NCHARS)
    return unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].ccc;
  return 0;
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
      /* We want descending order of combining class (hence <)
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
 * @return 0 on success, -1 on error
 *
 * @p s is modified in-place.  See Unicode 5.0 s3.11 for details of the
 * ordering.
 *
 * Currently we only support a maximum of 1024 combining characters after each
 * base character.  If this limit is exceeded then -1 is returned.
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
  const uint32_t *dc =                                                  \
    (c < UNICODE_NCHARS                                                 \
     ? unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].WHICH          \
     : 0);                                                              \
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
 * @return 0 on success, -1 on error
 */
static void utf32__decompose_one_canon(struct dynstr_ucs4 *d, uint32_t c) {
  utf32__decompose_one_generic(canon);
}

/** @brief Recursively compute the compatibility decomposition of @p c
 * @param d Dynamic string to store decomposition in
 * @param c Code point to decompose (must be a valid!)
 * @return 0 on success, -1 on error
 */
static void utf32__decompose_one_compat(struct dynstr_ucs4 *d, uint32_t c) {
  utf32__decompose_one_generic(compat);
}

/** @brief Guts of the decomposition functions */
#define utf32__decompose_generic(WHICH) do {            \
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
 * @return Pointer to result string, or NULL
 *
 * Computes the canonical decomposition of a string and stably sorts combining
 * characters into canonical order.  The result is in Normalization Form D and
 * (at the time of writing!) passes the NFD tests defined in Unicode 5.0's
 * NormalizationTest.txt.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint32_t *utf32_decompose_canon(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(canon);
}

/** @brief Compatibility decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL
 *
 * Computes the compatibility decomposition of a string and stably sorts
 * combining characters into canonical order.  The result is in Normalization
 * Form KD and (at the time of writing!) passes the NFKD tests defined in
 * Unicode 5.0's NormalizationTest.txt.
 *
 * Returns NULL if the string is not valid for either of the following reasons:
 * - it codes for a UTF-16 surrogate
 * - it codes for a value outside the unicode code space
 */
uint32_t *utf32_decompose_compat(const uint32_t *s, size_t ns, size_t *ndp) {
  utf32__decompose_generic(compat);
}

/** @brief Single-character case-fold and decompose operation */
#define utf32__casefold_one(WHICH) do {                                 \
  const uint32_t *cf =                                                  \
     (c < UNICODE_NCHARS                                                \
      ? unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].casefold      \
      : 0);                                                             \
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
 * @return Pointer to result string, or NULL
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
  for(n = 0; n < ns; ++n) {
    c = s[n];
    if(c < UNICODE_NCHARS
       && (unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].flags
           & unicode_normalize_before_casefold))
      break;
  }
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

/** @brief Compatibilit case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL
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

  for(n = 0; n < ns; ++n) {
    c = s[n];
    if(c < UNICODE_NCHARS
       && (unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].flags
           & unicode_normalize_before_casefold))
      break;
  }
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

/** @brief Return the General_Category value for @p c
 * @param Code point
 * @return General_Category property value
 */
static inline enum unicode_gc_cat utf32__general_category(uint32_t c) {
  if(c < UNICODE_NCHARS) {
    const struct unidata *const ud = &unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS];
    return ud->gc;
  } else
    return unicode_gc_Cn;
}

/** @brief Check Grapheme_Cluster_Break property
 * @param c Code point
 * @return 0 if it is as described, 1 otherwise
 */
static int utf32__is_control_or_cr_or_lf(uint32_t c) {
  switch(utf32__general_category(c)) {
  default:
    return 0;
  case unicode_gc_Zl:
  case unicode_gc_Zp:
  case unicode_gc_Cc:
    return 1;
  case unicode_gc_Cf:
    if(c == 0x200C || c == 0x200D)
      return 0;
    return 1;
  }
}

#define Hangul_Syllable_Type_NA 0
#define Hangul_Syllable_Type_L 0x1100
#define Hangul_Syllable_Type_V 0x1160
#define Hangul_Syllable_Type_T 0x11A8
#define Hangul_Syllable_Type_LV 0xAC00
#define Hangul_Syllable_Type_LVT 0xAC01

/** @brief Determine Hangul_Syllable_Type of @p c
 * @param c Code point
 * @return Equivalance class of @p c, or Hangul_Syllable_Type_NA 
 *
 * If this is a Hangul character then a representative member of its
 * equivalence class is returned.  Otherwise Hangul_Syllable_Type_NA is
 * returned.
 */
static uint32_t utf32__hangul_syllable_type(uint32_t c) {
  /* Dispose of the bulk of the non-Hangul code points first */
  if(c < 0x1100) return Hangul_Syllable_Type_NA;
  if(c > 0x1200 && c < 0xAC00) return Hangul_Syllable_Type_NA;
  if(c >= 0xD800) return Hangul_Syllable_Type_NA;
  /* Now we pick out the assigned Hangul code points */
  if((c >= 0x1100 && c <= 0x1159) || c == 0x115F) return Hangul_Syllable_Type_L;
  if(c >= 0x1160 && c <= 0x11A2) return Hangul_Syllable_Type_V;
  if(c >= 0x11A8 && c <= 0x11F9) return Hangul_Syllable_Type_T;
  if(c >= 0xAC00 && c <= 0xD7A3) {
    if(c % 28 == 16)
      return Hangul_Syllable_Type_LV;
    else
      return Hangul_Syllable_Type_LVT;
  }
  return Hangul_Syllable_Type_NA;
}

/** @brief Determine Word_Break property
 * @param c Code point
 * @return Word_Break property value of @p c
 */
static enum unicode_Word_Break utf32__word_break(uint32_t c) {
  if(c < 0xAC00 || c > 0xD7A3) {
    if(c < UNICODE_NCHARS)
      return unidata[c / UNICODE_MODULUS][c % UNICODE_MODULUS].word_break;
    else
      return unicode_Word_Break_Other;
  } else
    return unicode_Word_Break_ALetter;
}

/** @brief Identify a grapheme cluster boundary
 * @param s Start of string (must be NFD)
 * @param ns Length of string
 * @param n Index within string (in [0,ns].)
 * @return 1 at a grapheme cluster boundary, 0 otherwise
 *
 * This function identifies default grapheme cluster boundaries as described in
 * UAX #29 s3.  It returns 1 if @p n points at the code point just after a
 * grapheme cluster boundary (including the hypothetical code point just after
 * the end of the string).
 */
int utf32_is_gcb(const uint32_t *s, size_t ns, size_t n) {
  uint32_t before, after;
  uint32_t hbefore, hafter;
  /* GB1 and GB2 */
  if(n == 0 || n == ns)
    return 1;
  /* Now we know that s[n-1] and s[n] are safe to inspect */
  /* GB3 */
  before = s[n-1];
  after = s[n];
  if(before == 0x000D && after == 0x000A)
    return 0;
  /* GB4 and GB5 */
  if(utf32__is_control_or_cr_or_lf(before)
     || utf32__is_control_or_cr_or_lf(after))
    return 1;
  hbefore = utf32__hangul_syllable_type(before);
  hafter = utf32__hangul_syllable_type(after);
  /* GB6 */
  if(hbefore == Hangul_Syllable_Type_L
     && (hafter == Hangul_Syllable_Type_L
         || hafter == Hangul_Syllable_Type_V
         || hafter == Hangul_Syllable_Type_LV
         || hafter == Hangul_Syllable_Type_LVT))
    return 0;
  /* GB7 */
  if((hbefore == Hangul_Syllable_Type_LV
      || hbefore == Hangul_Syllable_Type_V)
     && (hafter == Hangul_Syllable_Type_V
         || hafter == Hangul_Syllable_Type_T))
    return 0;
  /* GB8 */
  if((hbefore == Hangul_Syllable_Type_LVT
      || hbefore == Hangul_Syllable_Type_T)
     && hafter == Hangul_Syllable_Type_T)
    return 0;
  /* GB9 */
  if(utf32__word_break(after) == unicode_Word_Break_Extend)
    return 0;
  /* GB10 */
  return 1;
}

/** @brief Return true if @p c is ignorable for boundary specifications */
static inline int utf32__boundary_ignorable(enum unicode_Word_Break wb) {
  return (wb == unicode_Word_Break_Extend
          || wb == unicode_Word_Break_Format);
}

/** @brief Identify a word boundary
 * @param s Start of string (must be NFD)
 * @param ns Length of string
 * @param n Index within string (in [0,ns].)
 * @return 1 at a word boundary, 0 otherwise
 *
 * This function identifies default word boundaries as described in UAX #29 s4.
 * It returns 1 if @p n points at the code point just after a word boundary
 * (including the hypothetical code point just after the end of the string).
 */
int utf32_is_word_boundary(const uint32_t *s, size_t ns, size_t n) {
  enum unicode_Word_Break twobefore, before, after, twoafter;
  size_t nn;

  /* WB1 and WB2 */
  if(n == 0 || n == ns)
    return 1;
  /* WB3 */
  if(s[n-1] == 0x000D && s[n] == 0x000A)
    return 0;
  /* WB4 */
  /* (!Sep) x (Extend|Format) as in UAX #29 s6.2 */
  switch(s[n-1]) {                      /* bit of a bodge */
  case 0x000A:
  case 0x000D:
  case 0x0085:
  case 0x2028:
  case 0x2029:
    break;
  default:
    if(utf32__boundary_ignorable(utf32__word_break(s[n])))
      return 0;
    break;
  }
  /* Gather the property values we'll need for the rest of the test taking the
   * s6.2 changes into account */
  /* First we look at the code points after the proposed boundary */
  nn = n;                               /* <ns */
  after = utf32__word_break(s[nn++]);
  if(!utf32__boundary_ignorable(after)) {
    /* X (Extend|Format)* -> X */
    while(nn < ns && utf32__boundary_ignorable(utf32__word_break(s[nn])))
      ++nn;
  }
  /* It's possible now that nn=ns */
  if(nn < ns)
    twoafter = utf32__word_break(s[nn]);
  else
    twoafter = unicode_Word_Break_Other;

  /* Next we look at the code points before the proposed boundary.  This is a
   * bit fiddlier. */
  nn = n;
  while(nn > 0 && utf32__boundary_ignorable(utf32__word_break(s[nn - 1])))
    --nn;
  if(nn == 0) {
    /* s[nn] must be ignorable */
    before = utf32__word_break(s[nn]);
    twobefore = unicode_Word_Break_Other;
  } else {
    /* s[nn] is ignorable or after the proposed boundary; but s[nn-1] is not
     * ignorable. */
    before = utf32__word_break(s[nn - 1]);
    --nn;
    /* Repeat the exercise */
    while(nn > 0 && utf32__boundary_ignorable(utf32__word_break(s[nn - 1])))
      --nn;
    if(nn == 0)
      twobefore = utf32__word_break(s[nn]);
    else
      twobefore = utf32__word_break(s[nn - 1]);
  }

  /* WB5 */
  if(before == unicode_Word_Break_ALetter
     && after == unicode_Word_Break_ALetter)
    return 0;
  /* WB6 */
  if(before == unicode_Word_Break_ALetter
     && after == unicode_Word_Break_MidLetter
     && twoafter == unicode_Word_Break_ALetter)
    return 0;
  /* WB7 */
  if(twobefore == unicode_Word_Break_ALetter
     && before == unicode_Word_Break_MidLetter
     && after == unicode_Word_Break_ALetter)
    return 0;
  /* WB8 */  
  if(before == unicode_Word_Break_Numeric
     && after == unicode_Word_Break_Numeric)
    return 0;
  /* WB9 */
  if(before == unicode_Word_Break_ALetter
     && after == unicode_Word_Break_Numeric)
    return 0;
  /* WB10 */
  if(before == unicode_Word_Break_Numeric
     && after == unicode_Word_Break_ALetter)
    return 0;
   /* WB11 */
  if(twobefore == unicode_Word_Break_Numeric
     && before == unicode_Word_Break_MidNum
     && after == unicode_Word_Break_Numeric)
    return 0;
  /* WB12 */
  if(before == unicode_Word_Break_Numeric
     && after == unicode_Word_Break_MidNum
     && twoafter == unicode_Word_Break_Numeric)
    return 0;
  /* WB13 */
  if(before == unicode_Word_Break_Katakana
     && after == unicode_Word_Break_Katakana)
    return 0;
  /* WB13a */
  if((before == unicode_Word_Break_ALetter
      || before == unicode_Word_Break_Numeric
      || before == unicode_Word_Break_Katakana
      || before == unicode_Word_Break_ExtendNumLet)
     && after == unicode_Word_Break_ExtendNumLet)
    return 0;
  /* WB13b */
  if(before == unicode_Word_Break_ExtendNumLet
     && (after == unicode_Word_Break_ALetter
         || after == unicode_Word_Break_Numeric
         || after == unicode_Word_Break_Katakana))
    return 0;
  /* WB14 */
  return 1;
}

/*@}*/
/** @defgroup Functions that operate on UTF-8 strings */
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
 * @return Pointer to result string, or NULL
 *
 * Computes the canonical decomposition of a string and stably sorts combining
 * characters into canonical order.  The result is in Normalization Form D and
 * (at the time of writing!) passes the NFD tests defined in Unicode 5.0's
 * NormalizationTest.txt.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also utf32_decompose_canon().
 */
char *utf8_decompose_canon(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_decompose_canon);
}

/** @brief Compatibility decompose @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL
 *
 * Computes the compatibility decomposition of a string and stably sorts
 * combining characters into canonical order.  The result is in Normalization
 * Form KD and (at the time of writing!) passes the NFKD tests defined in
 * Unicode 5.0's NormalizationTest.txt.
 *
 * Returns NULL if the string is not valid; see utf8_to_utf32() for reasons why
 * this might be.
 *
 * See also utf32_decompose_compat().
 */
char *utf8_decompose_compat(const char *s, size_t ns, size_t *ndp) {
  utf8__transform(utf32_decompose_compat);
}

/** @brief Case-fold @p [s,s+ns)
 * @param s Pointer to string
 * @param ns Length of string
 * @param ndp Where to store length of result
 * @return Pointer to result string, or NULL
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
 * @return Pointer to result string, or NULL
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

/*@}*/

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
