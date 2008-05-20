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

/* launder a string constant to stop gcc warnings */
static const char *L(const char *s) {
  return s;
}

static void test_printf(void) {
  char c;
  short s;
  int i;
  long l;
  long long ll;
  intmax_t m;
  ssize_t ssz;
  ptrdiff_t p;
  char *cp;
  char buffer[16];
  FILE *fp;
  
  check_string(do_printf("%d", 999), "999");
  check_string(do_printf("%d", -999), "-999");
  check_string(do_printf("%+d", 999), "+999");
  check_string(do_printf("%+d", -999), "-999");
  check_string(do_printf("%i", 999), "999");
  check_string(do_printf("%i", -999), "-999");
  check_string(do_printf("%u", 999), "999");
  check_string(do_printf("%2u", 999), "999");
  check_string(do_printf("%10u", 999), "       999");
  check_string(do_printf("%-10u", 999), "999       ");
  check_string(do_printf("%010u", 999), "0000000999");
  check_string(do_printf("%-10d", -999), "-999      ");
  check_string(do_printf("%-010d", -999), "-999      "); /* "-" beats "0" */
  check_string(do_printf("%66u", 999), "                                                               999");
  check_string(do_printf("%o", 999), "1747");
  check_string(do_printf("%#o", 999), "01747");
  check_string(do_printf("%#o", 0), "0");
  check_string(do_printf("%x", 999), "3e7");
  check_string(do_printf("%#x", 999), "0x3e7");
  check_string(do_printf("%#X", 999), "0X3E7");
  check_string(do_printf("%#x", 0), "0");
  check_string(do_printf("%hd", (short)999), "999");
  check_string(do_printf("%hhd", (short)99), "99");
  check_string(do_printf("%ld", 100000L), "100000");
  check_string(do_printf("%lld", 10000000000LL), "10000000000");
  check_string(do_printf("%qd", 10000000000LL), "10000000000");
  check_string(do_printf("%jd", (intmax_t)10000000000LL), "10000000000");
  check_string(do_printf("%zd", (ssize_t)2000000000), "2000000000");
  check_string(do_printf("%td", (ptrdiff_t)2000000000), "2000000000");
  check_string(do_printf("%hu", (short)999), "999");
  check_string(do_printf("%hhu", (short)99), "99");
  check_string(do_printf("%lu", 100000L), "100000");
  check_string(do_printf("%llu", 10000000000LL), "10000000000");
  check_string(do_printf("%ju", (uintmax_t)10000000000LL), "10000000000");
  check_string(do_printf("%zu", (size_t)2000000000), "2000000000");
  check_string(do_printf("%tu", (ptrdiff_t)2000000000), "2000000000");
  check_string(do_printf("%p", (void *)0x100), "0x100");
  check_string(do_printf("%s", "wibble"), "wibble");
  check_string(do_printf("%s-%s", "wibble", "wobble"), "wibble-wobble");
  check_string(do_printf("%10s", "wibble"), "    wibble");
  check_string(do_printf("%010s", "wibble"), "    wibble"); /* 0 ignored for %s */
  check_string(do_printf("%-10s", "wibble"), "wibble    ");
  check_string(do_printf("%2s", "wibble"), "wibble");
  check_string(do_printf("%.2s", "wibble"), "wi");
  check_string(do_printf("%.2s", "w"), "w");
  check_string(do_printf("%4.2s", "wibble"), "  wi");
  check_string(do_printf("%c", 'a'), "a");
  check_string(do_printf("%4c", 'a'), "   a");
  check_string(do_printf("%-4c", 'a'), "a   ");
  check_string(do_printf("%*c", 0, 'a'), "a");
  check_string(do_printf("x%hhny", &c), "xy");
  insist(c == 1);
  check_string(do_printf("xx%hnyy", &s), "xxyy");
  insist(s == 2);
  check_string(do_printf("xxx%nyyy", &i), "xxxyyy");
  insist(i == 3);
  check_string(do_printf("xxxx%lnyyyy", &l), "xxxxyyyy");
  insist(l == 4);
  check_string(do_printf("xxxxx%llnyyyyy", &ll), "xxxxxyyyyy");
  insist(ll == 5);
  check_string(do_printf("xxxxxx%jnyyyyyy", &m), "xxxxxxyyyyyy");
  insist(m == 6);
  check_string(do_printf("xxxxxxx%znyyyyyyy", &ssz), "xxxxxxxyyyyyyy");
  insist(ssz == 7);
  check_string(do_printf("xxxxxxxx%tnyyyyyyyy", &p), "xxxxxxxxyyyyyyyy");
  insist(p == 8);
  check_string(do_printf("%*d", 5, 99), "   99");
  check_string(do_printf("%*d", -5, 99), "99   ");
  check_string(do_printf("%.*d", 5, 99), "00099");
  check_string(do_printf("%.*d", -5, 99), "99");
  check_string(do_printf("%.0d", 0), "");
  check_string(do_printf("%.d", 0), "");
  check_string(do_printf("%.d", 0), "");
  check_string(do_printf("%%"), "%");
  check_string(do_printf("wibble"), "wibble");
  insist(do_printf("%") == 0);
  insist(do_printf("%=") == 0);
  i = byte_asprintf(&cp, "xyzzy %d", 999);
  insist(i == 9);
  check_string(cp, "xyzzy 999");
  i = byte_snprintf(buffer, sizeof buffer, "xyzzy %d", 999);
  insist(i == 9);
  check_string(buffer, "xyzzy 999");
  i = byte_snprintf(buffer, sizeof buffer, "%*d", 32, 99);
  insist(i == 32);
  check_string(buffer, "               ");
  {
    /* bizarre workaround for compiler checking of format strings */
    char f[] = "xyzzy %";
    i = byte_asprintf(&cp, f);
    insist(i == -1);
  }

  fp = tmpfile();
  insist(byte_fprintf(fp, "%10s\n", "wibble") == 11);
  rewind(fp);
  insist(fgets(buffer, sizeof buffer, fp) == buffer);
  check_string(buffer, "    wibble\n");
  fclose(fp);
  check_integer(byte_snprintf(buffer, sizeof buffer,
                              "%18446744073709551616d", 10), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer,
                              "%.18446744073709551616d", 10), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%hs"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%qs"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%js"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%zs"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%ts"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%Ls"), ""), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%hp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%lp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%qp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%jp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%zp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%tp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%Lp"), (void *)0), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%h%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%l%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%q%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%j%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%z%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%t%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, L("%L%")), -1);
  check_integer(byte_snprintf(buffer, sizeof buffer, "%2147483647s%2147483647s", "", ""), -1);
  check_integer(byte_sinkprintf(sink_error(), ""), 0);
  check_integer(byte_sinkprintf(sink_error(), "%5s", ""), -1);
  check_integer(byte_sinkprintf(sink_error(), "%d", 0), -1);
  check_integer(byte_sinkprintf(sink_error(), "%d", 1), -1);
  check_integer(byte_sinkprintf(sink_error(), "%2d", 0), -1);
  check_integer(byte_sinkprintf(sink_error(), "%d", -1), -1);
  check_integer(byte_sinkprintf(sink_error(), "%#x", 10), -1);
  check_integer(byte_sinkprintf(sink_error(), "%-d", 0), -1);
  check_integer(byte_sinkprintf(sink_error(), "%-d", 1), -1);
  check_integer(byte_sinkprintf(sink_error(), "%-2d", 0), -1);
  check_integer(byte_sinkprintf(sink_error(), "%-d", -1), -1);
  check_integer(byte_sinkprintf(sink_error(), "%-#x", 10), -1);

}

TEST(printf);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
