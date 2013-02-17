/* strptime.c - partial strptime() reimplementation
 *
 * Copyright (c) 2008, 2011 Richard Kettlewell.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/** @file lib/strptime.c
 * @brief strptime() reimplementation
 *
 * strptime() is here reimplemented because the FreeBSD (and older MacOS) one
 * is broken and does not report errors properly.  See TODO remarks below for
 * some missing bits.
 */

#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <langinfo.h>
#include "strptime.h"

/** @brief Lookup table entry for locale-specific strings */
struct locale_item_match {
  /** @brief Locale key to try */
  nl_item key;

  /** @brief Value to return if value of @ref key matches subject string */
  int value;
};

static const struct locale_item_match days[] = {
  { DAY_1, 0 },
  { DAY_2, 1 },
  { DAY_3, 2 },
  { DAY_4, 3 },
  { DAY_5, 4 },
  { DAY_6, 5 },
  { DAY_7, 6 },
  { ABDAY_1, 0 },
  { ABDAY_2, 1 },
  { ABDAY_3, 2 },
  { ABDAY_4, 3 },
  { ABDAY_5, 4 },
  { ABDAY_6, 5 },
  { ABDAY_7, 6 },
  { -1, -1 }
};

static const struct locale_item_match months[] = {
  { MON_1, 1 },
  { MON_2, 2 },
  { MON_3, 3 },
  { MON_4, 4 },
  { MON_5, 5 },
  { MON_6, 6 },
  { MON_7, 7 },
  { MON_8, 8 },
  { MON_9, 9 },
  { MON_10, 10 },
  { MON_11, 11 },
  { MON_12, 12 },
  { ABMON_1, 1 },
  { ABMON_2, 2 },
  { ABMON_3, 3 },
  { ABMON_4, 4 },
  { ABMON_5, 5 },
  { ABMON_6, 6 },
  { ABMON_7, 7 },
  { ABMON_8, 8 },
  { ABMON_9, 9 },
  { ABMON_10, 10 },
  { ABMON_11, 11 },
  { ABMON_12, 12 },
  { -1, -1 },
};

/** @brief Match a string
 * @param buf Start of subject
 * @param limit End of subject
 * @param match String to match subject against
 * @return True if match == [buf,limit) otherwise false
 *
 * The match is case-independent at least in ASCII.
 */
static int try_match(const char *buf,
                     const char *limit,
                     const char *match) {
  /* TODO this won't work well outside single-byte encodings.  A good bet is
   * probably to convert to Unicode and then use utf32_casefold_compat() (or
   * utf8_casefold_compat(); using compatibility matching will ensure missing
   * accents and so on aren't a problem.
   *
   * en_GB and en_US will probably be in any reasonable encoding for them.
   */
  while(buf < limit && *match) {
    if(tolower((unsigned char)*buf) != tolower((unsigned char)*match))
      return 0;
    ++buf;
    ++match;
  }
  if(buf != limit || *match)
    return 0;
  return 1;
}

/** @brief Match from table of locale-specific strings
 * @param buf Start of subject
 * @param limit End of subject
 * @param lim Table of locale lookups
 * @return Looked up value or -1
 *
 * The match is case-independent.
 */
static int try_locale_match(const char *buf,
                            const char *limit,
                            const struct locale_item_match *lim) {
  /* This is not very efficient!  A (correct) built-in implementation will
   * presumably have more direct access to locale information. */
  while(lim->value != -1) {
    if(try_match(buf, limit, nl_langinfo(lim->key)))
      return lim->value;
    ++lim;
  }
  return -1;
}

static int try_numeric_match(const char *buf,
                             const char *limit,
                             unsigned low,
                             unsigned high) {
  unsigned n = 0;

  while(buf < limit) {
    int ch = (unsigned char)*buf++;
    if(ch >= '0' && ch <= '9') {
      if(n > INT_MAX / 10
         || (n == INT_MAX / 10 && ch >= INT_MAX % 10 + '0'))
        return -1;                      /* overflow */
      n = 10 * n + ch - '0';
    } else
      return -1;
  }
  if(n < low || n > high)
    return -1;
  return (int)n;
}

static const char *my_strptime_guts(const char *buf,
                                    const char *format,
                                    struct tm *tm) {
  int fc, mod, spec, next, value;
  const char *limit;
  /* nl_langinfo() is allowed to trash its last return value so we copy.
   * (We're relying on it being usable at all in multithreaded environments
   * though.) */
#define USE_SUBFORMAT(ITEM, EITEM, DEF) do {            \
  const char *s;                                        \
  char subformat[128];                                  \
                                                        \
  if(mod == 'E') {                                      \
    s = nl_langinfo(EITEM);                             \
    if(!s || !*s)                                       \
      s = nl_langinfo(ITEM);                            \
  } else                                                \
    s = nl_langinfo(ITEM);                              \
  if(!s || !*s)                                         \
    s = DEF;                                            \
  if(strlen(s) >= sizeof subformat)                     \
    s = DEF;                                            \
  strcpy(subformat, s);                                 \
  if(!(buf = my_strptime_guts(buf, subformat, tm)))     \
    return NULL;                                        \
} while(0)

  while(*format) {
    fc = (unsigned char)*format++;
    if(fc == '%') {
      /* Get the character defining the converstion specification */
      spec = (unsigned char)*format++;
      if(spec == 'E' || spec == 'O') {
        /* Oops, there's a modifier first */
        mod = spec;
        spec = (unsigned char)*format++;
      } else
        mod = 0;
      if(!spec)
        return NULL;                    /* format string broken! */
      /* See what the next directive is.  The specification is written in terms
       * of stopping the match at a character that matches the next directive.
       * This implementation mirrors this aspect of the specification
       * directly. */
      next = (unsigned char)*format;
      if(next) {
        limit = buf;
        if(isspace(next)) {
          /* Next directive is whitespace, so bound the input string (at least)
           * by that */
          while(*limit && !isspace((unsigned char)*limit))
            ++limit;
        } else if(next == '%') {
          /* Prohibited: "The application shall ensure that there is
           * white-space or other non-alphanumeric characters between any two
           * conversion specifications".  In fact we let alphanumerics
           * through.
           *
           * Forbidding even %% seems a bit harsh but is consistent with the
           * specification as written.
           */
          return NULL;
        } else {
          /* Next directive is a specific character, so bound the input string
           * (at least) by that.  This will work badly in the face of multibyte
           * characters, but then the spec is vague about what kind of string
           * we're dealing with anyway so you probably couldn't safely use them
           * in the format string at least in any case. */
          while(*limit && *limit != next)
            ++limit;
        }
      } else
        limit = buf + strlen(buf);
      switch(spec) {
      case 'A': case 'a':               /* day name (abbrev or full) */
        if((value = try_locale_match(buf, limit, days)) == -1)
          return NULL;
        tm->tm_wday = value;
        break;
      case 'B': case 'b': case 'h':     /* month name (abbrev or full) */
        if((value = try_locale_match(buf, limit, months)) == -1)
          return NULL;
        tm->tm_mon = value - 1;
        break;
      case 'c':                         /* locale date+time */
        USE_SUBFORMAT(D_T_FMT, ERA_D_T_FMT, "%a %b %e %H:%M:%S %Y");
        break;
      case 'C':                         /* century number 0-99 */
        /* TODO  */
        return NULL;
      case 'd': case 'e':               /* day of month 1-31 */
        if((value = try_numeric_match(buf, limit, 1, 31)) == -1)
          return NULL;
        tm->tm_mday = value;
        break;
      case 'D':                         /* == "%m / %d / %y" */
        if(!(buf = my_strptime_guts(buf, "%m / %d / %y", tm)))
          return NULL;
        break;
      case 'H':                         /* hour 0-23 */
        if((value = try_numeric_match(buf, limit, 0, 23)) == -1)
          return NULL;
        tm->tm_hour = value;
        break;
      case 'I':                         /* hour 1-12 */
        /* TODO */ 
        return NULL;
      case 'j':                         /* day 1-366 */
        if((value = try_numeric_match(buf, limit, 1, 366)) == -1)
          return NULL;
        tm->tm_yday = value - 1;
        return NULL;
      case 'm':                         /* month 1-12 */
        if((value = try_numeric_match(buf, limit, 1, 12)) == -1)
          return NULL;
        tm->tm_mon = value - 1;
        break;
      case 'M':                         /* minute 0-59 */
        if((value = try_numeric_match(buf, limit, 0, 59)) == -1)
          return NULL;
        tm->tm_min = value;
        break;
      case 'n': case 't':               /* any whitespace */
        goto matchwhitespace;
      case 'p':                         /* locale am/pm */
        /* TODO */
        return NULL;
      case 'r':                         /* == "%I : %M : %S %p" */
        /* TODO actually this is locale-dependent; and we don't implement %I
         * anyway, so it's not going to work even as it stands. */
        if(!(buf = my_strptime_guts(buf, "%I : %M : %S %p", tm)))
          return NULL;
        break;
      case 'R':                         /* == "%H : %M" */
        if(!(buf = my_strptime_guts(buf, "%H : %M", tm)))
          return NULL;
        break;
      case 'S':                         /* seconds 0-60 */
        if((value = try_numeric_match(buf, limit, 0, 60)) == -1)
          return NULL;
        tm->tm_sec = value;
        break;
      case 'U':                         /* week number from Sunday 0-53 */
        /* TODO */
        return NULL;
      case 'w':                         /* day number 0-6 from Sunday */
        if((value = try_numeric_match(buf, limit, 0, 6)) == -1)
          return NULL;
        tm->tm_wday = value;
        break;
      case 'W':                         /* week number from Monday 0-53 */
        /* TODO */ 
        return NULL;
      case 'x':                         /* locale date format */
        USE_SUBFORMAT(D_FMT, ERA_D_FMT, "%m/%d/%y");
        break;
      case 'X':                         /* locale time format */
        USE_SUBFORMAT(T_FMT, ERA_T_FMT, "%H:%M:%S");
        break;
      case 'y':                         /* year mod 100 */
        if((value = try_numeric_match(buf, limit, 0, INT_MAX)) == -1)
          return NULL;
        if(value >= 0 && value <= 68)
          value = 2000 + value;
        else if(value >= 69 && value <= 99)
          value = 1900 + value;
        tm->tm_year = value - 1900;
        break;
      case 'Y':                         /* year */
        if((value = try_numeric_match(buf, limit, 1, INT_MAX)) == -1)
          return NULL;
        tm->tm_year = value - 1900;
        break;
      case '%':
        goto matchself;
      default:
        /* The spec is a bit vague about what to do with invalid format
         * strings.  We return NULL immediately and hope someone will
         * notice. */
        return NULL;
      }
      buf = limit;
    } else if(isspace(fc)) {
    matchwhitespace:
      /* Any format whitespace matches any number of input whitespace
       * characters.  The directive can formally contain more than one
       * whitespace character; for the second and subsequent ones we'll match 0
       * characters from the input. */
      while(isspace((unsigned char)*buf))
        ++buf;
    } else {
    matchself:
      /* Non-% non-whitespace characters must match themselves exactly */
      if(fc != (unsigned char)*buf++)
        return NULL;
    }
  }
  /* When we run out of format string we return a pointer to the rest of the
   * input. */
  return buf;
}

/** @brief Reimplementation of strptime()
 * @param buf Input buffer
 * @param format Format string
 * @param tm Where to put result
 * @return Pointer to first unparsed input character, or NULL on error
 *
 * Based on <a
 * href="http://www.opengroup.org/onlinepubs/009695399/functions/strptime.html">http://www.opengroup.org/onlinepubs/009695399/functions/strptime.html</a>.
 */
char *my_strptime(const char *buf,
                  const char *format,
                  struct tm *tm) {
  /* Whether to overwrite or update is unspecified (rather bizarrely).  This
   * implementation does not overwrites, as xgetdate() depends on this
   * behavior. */

  if(!(buf = my_strptime_guts(buf, format, tm)))
    return NULL;
  /* TODO various things we could/should do: 
   * - infer day/month from %j+year
   * - infer day/month from %U/%W+%w/%a+year
   * - infer hour from %p+%I
   * - fill wday/yday from other fields
   */
  return (char *)buf;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
