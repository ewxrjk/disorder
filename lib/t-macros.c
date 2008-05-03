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

  /* The simplest possible expansion */
  m = mx_parse("macro1", 1, "@macro@", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro1");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Spacing variants of the above */
  m = mx_parse("macro2", 1, "@    macro@", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro2");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");
  m = mx_parse("macro3", 1, "@macro    @", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro3");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Unterminated variants */
  m = mx_parse("macro4", 1, "@macro", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro4");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");
  m = mx_parse("macro5", 1, "@macro   ", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro5");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Macros with a :-separated argument */
  m = mx_parse("macro5", 1, "@macro:arg@", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro5");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 1);
  insist(m->next == 0);
  
  check_integer(m->args[0]->type, MX_TEXT);
  check_string(m->args[0]->filename, "macro5");
  check_integer(m->args[0]->line, 1);
  check_string(m->args[0]->text, "arg");
  insist(m->args[0]->next == 0);

  check_string(mx_dump(m), "@macro{arg}@");

  /* Multiple :-separated arguments, and spacing, and newlines */
  m = mx_parse("macro6", 1, "@macro : \n arg1 : \n arg2@", NULL);
  check_integer(m->type, MX_EXPANSION);
  check_string(m->filename, "macro6");
  check_integer(m->line, 1);
  check_string(m->name, "macro");
  check_integer(m->nargs, 2);
  insist(m->next == 0);
  
  check_integer(m->args[0]->type, MX_TEXT);
  check_string(m->args[0]->filename, "macro6");
  check_integer(m->args[0]->line, 2);
  check_string(m->args[0]->text, "arg1");
  insist(m->args[0]->next == 0);
  
  check_integer(m->args[1]->type, MX_TEXT);
  check_string(m->args[1]->filename, "macro6");
  check_integer(m->args[1]->line, 3);
  check_string(m->args[1]->text, "arg2");
  insist(m->args[1]->next == 0);

  check_string(mx_dump(m), "@macro{arg1}{arg2}@");

  /* Multiple bracketed arguments */
  m = mx_parse("macro7", 1, "@macro{arg1}{arg2}@", NULL);
  check_string(mx_dump(m), "@macro{arg1}{arg2}@");

  m = mx_parse("macro8", 1, "@macro{\narg1}{\narg2}@", NULL);
  check_string(mx_dump(m), "@macro{\narg1}{\narg2}@");
  check_integer(m->args[0]->line, 1);
  check_integer(m->args[1]->line, 2);
  /* ...yes, lines 1 and 2: the first character of the first arg is
   * the \n at the end of line 1.  Compare with macro9: */

  m = mx_parse("macro9", 1, "@macro\n{arg1}\n{arg2}@", NULL);
  check_string(mx_dump(m), "@macro{arg1}{arg2}@");
  check_integer(m->args[0]->line, 2);
  check_integer(m->args[1]->line, 3);

  /* Arguments that themselves contain expansions */
  m = mx_parse("macro10", 1, "@macro{@macro2{arg1}{arg2}@}@", NULL);
  check_string(mx_dump(m), "@macro{@macro2{arg1}{arg2}@}@");

  /* ...and with omitted trailing @ */
  m = mx_parse("macro11", 1, "@macro{@macro2{arg1}{arg2}}", NULL);
  check_string(mx_dump(m), "@macro{@macro2{arg1}{arg2}@}@");
  
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
