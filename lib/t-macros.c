/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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
#include "macros.h"

static void test_macros(void) {
  const struct mx_node *m;
#define L1 "this is just some\n"
#define L2 "plain text\n"
  static const char plain[] = L1 L2;
  char *s;
  const char *cs;

  /* Plain text ------------------------------------------------------------- */
  
  /* As simple as it gets */
  m = mx_parse("plaintext1", 1, "", NULL);
  insist(m == 0);

  /* Almost as simple as that */
  m = mx_parse("plaintext1", 1, plain, NULL);
  check_integer(m->type, MX_TEXT);
  check_string(m->filename, "plaintext1");
  check_integer(m->line, 1);
  check_string(m->text, L1 L2);
  insist(m->next == 0);
  check_string(mx_dump(m), plain);

  /* Check that partial parses stop in the right place */
  m = mx_parse("plaintext2", 5, plain, plain + strlen(L1));
  check_integer(m->type, MX_TEXT);
  check_string(m->filename, "plaintext2");
  check_integer(m->line, 5);
  check_string(m->text, L1);
  insist(m->next == 0);
  check_string(mx_dump(m), L1);

  /* Simple macro parsing --------------------------------------------------- */

  /* The simplest possible expansion */
  m = mx_parse("macro1", 1, "@macro", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro1");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro");

  m = mx_parse("macro2", 1, "@macro    ", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro2");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->next != 0);
  check_integer(m->next->type, MX_TEXT);
  check_string(mx_dump(m), "@macro    ");

  /* Multiple bracketed arguments */
  m = mx_parse("macro7", 1, "@macro{arg1}{arg2}", NULL);
  check_string(mx_dump(m), "@macro{arg1}{arg2}");

  m = mx_parse("macro8", 1, "@macro{\narg1}{\narg2}", NULL);
  check_string(mx_dump(m), "@macro{\narg1}{\narg2}");
  check_integer(m->args[0]->line, 1);
  check_integer(m->args[1]->line, 2);
  /* ...yes, lines 1 and 2: the first character of the first arg is
   * the \n at the end of line 1.  Compare with macro9: */

  m = mx_parse("macro9", 1, "@macro\n{arg1}\n{arg2}", NULL);
  check_string(mx_dump(m), "@macro{arg1}{arg2}");
  check_integer(m->args[0]->line, 2);
  check_integer(m->args[1]->line, 3);

  /* Arguments that themselves contain expansions */
  m = mx_parse("macro10", 1, "@macro{@macro2{arg1}{arg2}}", NULL);
  check_string(mx_dump(m), "@macro{@macro2{arg1}{arg2}}");

  /* ...and with omitted trailing @ */
  m = mx_parse("macro11", 1, "@macro{@macro2{arg1}{arg2}}", NULL);
  check_string(mx_dump(m), "@macro{@macro2{arg1}{arg2}}");

  /* Similarly but with more whitespace; NB that the whitespace is
   * preserved. */
  m = mx_parse("macro12", 1, "@macro {@macro2 {arg1} {arg2}  }\n", NULL);
  check_string(mx_dump(m), "@macro{@macro2{arg1}{arg2}  }\n");

  /* Simple expansions ------------------------------------------------------ */

  mx_register_builtin();
  mx_search_path(".");
  mx_search_path("lib");
  if((cs = getenv("srcdir")))
    mx_search_path(cs);
  
#define check_macro(NAME, INPUT, OUTPUT, RET) do {              \
  m = mx_parse(NAME, 1, INPUT, NULL);                           \
  check_integer(mx_expandstr(m, &s, 0/*u*/, NAME), (RET));      \
  if(s && strcmp(s, OUTPUT)) {                                  \
    fprintf(stderr, "%s:%d: test %s\n"                          \
            "     INPUT:\n%s\n"                                 \
            "  EXPECTED: '%s'\n"                                \
            "       GOT: '%s'\n",                               \
            __FILE__, __LINE__, NAME, INPUT, OUTPUT, s);        \
    count_error();                                              \
  }                                                             \
} while(0)

  check_macro("empty", "", "", 0);
  check_macro("plain", plain, plain, 0);
  check_macro("quote1", "@@", "@", 0);
  check_macro("quote2", "@@@@", "@@", 0);
  check_macro("nothing1", "@_", "", 0);
  check_macro("nothing2", "<@_>", "<>", 0);

  check_macro("if1", "@if{true}{yes}{no}", "yes", 0);
  check_macro("if2", "@if{true}{yes}", "yes", 0);
  check_macro("if3", "@if{false}{yes}{no}", "no", 0);
  check_macro("if4", "@if{false}{yes}", "", 0);
  check_macro("if5", "@if{ true}{yes}", "", 0);
  check_macro("if6", "@if{true}{yes}@_{wible}t", "yes{wible}t", 0);

  check_macro("br1", "@if(true)(yes)(no)", "yes", 0);
  check_macro("br1", "@if[true][yes]{no}", "yes{no}", 0);
  
  check_macro("and1", "@and", "true", 0);
  check_macro("and2", "@and{true}", "true", 0);
  check_macro("and3", "@and{false}", "false", 0);
  check_macro("and4", "@and{true}{true}", "true", 0);
  check_macro("and5", "@and{false}{true}", "false", 0);
  check_macro("and6", "@and{true}{false}", "false", 0);
  check_macro("and7", "@and{false}{false}", "false", 0);

  check_macro("or1", "@or", "false", 0);
  check_macro("or2", "@or{true}", "true", 0);
  check_macro("or2", "@or{false}", "false", 0);
  check_macro("or3", "@or{true}{true}", "true", 0);
  check_macro("or4", "@or{false}{true}", "true", 0);
  check_macro("or5", "@or{true}{false}", "true", 0);
  check_macro("or7", "@or{false}{false}", "false", 0);

  check_macro("not1", "@not{true}", "false", 0);
  check_macro("not2", "@not{false}", "true", 0);
  check_macro("not3", "@not{wibble}", "true", 0);

  check_macro("comment1", "@# wibble\n", "", 0);
  check_macro("comment2", "@# comment\nplus a line", "plus a line", 0);

  check_macro("discard1", "@discard{wibble}", "", 0);
  check_macro("discard2", "@discard{comment with a\nnewline in}", "", 0);

  check_macro("eq1", "@eq", "true", 0);
  check_macro("eq2", "@eq{}", "true", 0);
  check_macro("eq3", "@eq{a}", "true", 0);
  check_macro("eq4", "@eq{a}{a}", "true", 0);
  check_macro("eq5", "@eq{a}{a}{a}", "true", 0);
  check_macro("eq7", "@eq{a}{b}", "false", 0);
  check_macro("eq8", "@eq{a}{b}{a}", "false", 0);
  check_macro("eq9", "@eq{a}{a}{b}", "false", 0);
  check_macro("eq10", "@eq{b}{a}{a}", "false", 0);

  check_macro("ne1", "@ne", "true", 0);
  check_macro("ne2", "@ne{}", "true", 0);
  check_macro("ne3", "@ne{a}", "true", 0);
  check_macro("ne4", "@ne{a}{a}", "false", 0);
  check_macro("ne5", "@ne{a}{a}{a}", "false", 0);
  check_macro("ne7", "@ne{a}{b}", "true", 0);
  check_macro("ne8", "@ne{a}{b}{a}", "false", 0);
  check_macro("ne9", "@ne{a}{a}{b}", "false", 0);
  check_macro("ne10", "@ne{b}{a}{a}", "false", 0);
  check_macro("ne11", "@ne{a}{b}{c}", "true", 0);

  check_macro("sh1", "@shell{true}", "", 0);
  check_macro("sh2", "@shell{echo spong}", "spong\n", 0);
  fprintf(stderr, ">>> expect error message about shell command:\n");
  check_macro("sh3", "@shell{echo spong;exit 3}", "spong\n", 0);

  check_macro("url1", "@urlquote{unreserved}", "unreserved", 0);
  check_macro("url2", "@urlquote{has space}", "has%20space", 0);
  check_macro("url3", "@urlquote{\xc0\xc1}", "%c0%c1", 0);

  check_macro("include1", "@include{t-macros-1.tmpl}",
              "yes\n", 0);
  check_macro("include2", "@include{t-macros-2}",
              "wibble\n", 0);
  fprintf(stderr, ">>> expect error message about t-macros-nonesuch:\n");
  check_macro("include3", "<@include{t-macros-nonesuch}>",
              "<[[cannot find 't-macros-nonesuch']]>", 0);
  fprintf(stderr, ">>> expect error message about 'wibble':\n");
  check_macro("badex1", "<@wibble>",
              "<[['wibble' unknown]]>", 0);
  fprintf(stderr, ">>> expect error message about 'if':\n");
  check_macro("badex2", "<@if>",
              "<[['if' too few args]]>", 0);
  fprintf(stderr, ">>> expect error message about 'if':\n");
  check_macro("badex3", "<@if{1}{2}{3}{4}{5}>",
              "<[['if' too many args]]>", 0);
  
  /* Macro definitions ------------------------------------------------------ */

  check_macro("macro1", "@define{m}{a b c}{@c @b @a}@#\n"
              "@m{1}{2}{3}",
              "3 2 1", 0);
  check_macro("macro2", "@m{b}{c}{a}",
              "a c b", 0);
  check_macro("macro3", "@m{@eq{z}{z}}{p}{q}",
              "q p true", 0);
  check_macro("macro4",
              "@discard{\n"
              "  @define{n}{a b c}\n"
              "    {@if{@eq{@a}{@b}} {@c} {no}}\n"
              "}@#\n"
              "@n{x}{y}{z}",
              "no", 0);
  check_macro("macro5",
              "@n{x}{x}{z}",
              "z", 0);

}

TEST(macros);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
