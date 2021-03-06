/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008, 2010 Richard Kettlewell
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
/** @file libtests/test.h @brief Library tests */

#ifndef TEST_H
#define TEST_H

#include "common.h"

#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <setjmp.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "charset.h"
#include "mime.h"
#include "hex.h"
#include "heap.h"
#include "unicode.h"
#include "inputline.h"
#include "wstat.h"
#include "signame.h"
#include "cache.h"
#include "filepart.h"
#include "hash.h"
#include "selection.h"
#include "syscalls.h"
#include "kvp.h"
#include "sink.h"
#include "printf.h"
#include "basen.h"
#include "split.h"
#include "configuration.h"
#include "addr.h"
#include "base64.h"
#include "url.h"
#include "regexp.h"
#include "regsub.h"

extern long long tests, errors;
extern int fail_first;
extern int verbose;
extern int skipped;

/** @brief Checks that @p expr is nonzero
 * @param expr Expression to check
 *
 * If @p expr is nonzero then logs an error (and continues).
 */
#define insist(expr) do {				\
  if(!(expr)) {						\
    count_error();						\
    fprintf(stderr, "%s:%d: error checking %s\n",	\
            __FILE__, __LINE__, #expr);			\
  }							\
  ++tests;						\
} while(0)

/** @brief Check that a pair of strings match
 * @param GOT What we actually got
 * @param WANT What we wanted
 *
 * If @p GOT and @p WANT differ then logs an error (and continues).
 *
 * @p WANT is allowed to evaluate to a null pointer (but if it comes to
 * anything else then must be safe to strcmp).
 */
#define check_string(GOT, WANT) do {                                    \
  const char *got = GOT;                                                \
  const char *want = WANT;                                              \
                                                                        \
  if(got == 0) {                                                       \
    fprintf(stderr, "%s:%d: %s returned 0\n",                           \
            __FILE__, __LINE__, #GOT);                                  \
    count_error();                                                      \
  } else if(strcmp(want, got)) {                                        \
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s\n",         \
	    __FILE__, __LINE__, #GOT, format(got), format(want));       \
    count_error();                                                      \
  }                                                                     \
  ++tests;                                                              \
 } while(0)

/** @brief Check that a string prefix matches
 * @param GOT What we actually got
 * @param WANT What we wanted
 *
 * If @p WANT is not a prefix of @p GOT then logs an error (and continues).
 *
 * @p WANT is allowed to evaluate to a null pointer (but if it comes to
 * anything else then must be safe to strcmp).
 */
#define check_string_prefix(GOT, WANT) do {                             \
  const char *got = GOT;                                                \
  const char *want = WANT;                                              \
                                                                        \
  if(want == 0) {                                                       \
    fprintf(stderr, "%s:%d: %s returned 0\n",                           \
            __FILE__, __LINE__, #GOT);                                  \
    count_error();                                                      \
  } else if(strncmp(want, got, strlen(want))) {                         \
    fprintf(stderr, "%s:%d: %s returned:\n%s\nexpected:\n%s...\n",      \
	    __FILE__, __LINE__, #GOT, format(got), format(want));       \
    count_error();                                                      \
  }                                                                     \
  ++tests;                                                              \
 } while(0)

/** @brief Check that a pair of integers match.
 * @param GOT What we actually got
 * @param WANT What we wanted
 *
 * If @p GOT and @p WANT differ then logs an error (and continues).
 */
#define check_integer(GOT, WANT) do {                           \
  const intmax_t got = GOT, want = WANT;                        \
  if(got != want) {                                             \
    fprintf(stderr, "%s:%d: %s returned: %jd  expected: %jd\n", \
            __FILE__, __LINE__, #GOT, got, want);               \
    count_error();                                              \
  }                                                             \
  ++tests;                                                      \
} while(0)

/** @brief Check that a function calls fatal()
 * @param WHAT Expression to evaluate
 *
 * Evaluates WHAT and if it does not call fatal(), logs an error.  In any case,
 * continues.  Modifies exitfn() so that fatal() isn't actually fatal.
 */
#define check_fatal(WHAT) do {                                          \
  void (*const save_exitfn)(int) attribute((noreturn)) = exitfn;        \
                                                                        \
  exitfn = test_exitfn;                                                 \
  if(setjmp(fatal_env) == 0) {                                          \
    fprintf(stderr, "Expect an error:\n ");                             \
    (void)(WHAT);                                                       \
    fprintf(stderr, "\n%s:%d: %s unexpectedly returned\n",              \
                     __FILE__, __LINE__, #WHAT);                        \
    count_error();                                                      \
  }                                                                     \
  ++tests;                                                              \
  exitfn = save_exitfn;                                                 \
} while(0)

void count_error(void);
const char *format(const char *s);
const char *format_utf32(const uint32_t *s);
uint32_t *ucs4parse(const char *s);
const char *do_printf(const char *fmt, ...);
void test_init(int argc, char **argv);

extern jmp_buf fatal_env;
void test_exitfn(int) attribute((noreturn));

/** @brief Common code for each test source file
 * @param name Name of test
 *
 * Expands to a  @c main function which:
 * - calls test_init()
 * - calls test_NAME()
 * - reports a count of errors
 * - returns the right exit status
 */
#define TEST(name)                                                      \
  int main(int argc, char **argv) {                                     \
    test_init(argc, argv);                                              \
    test_##name();                                                      \
    if(errors || verbose)                                               \
      fprintf(stderr, "test_"#name": %lld errors out of %lld tests\n",  \
              errors, tests);                                           \
    if(errors)                                                          \
      return 1;                                                         \
    if(skipped)                                                         \
      return 77;                                                        \
    return 0;                                                           \
  }                                                                     \
                                                                        \
  struct swallow_semicolon

#endif /* TEST_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
